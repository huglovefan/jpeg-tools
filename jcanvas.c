#include "jcanvas.h"

#include <assert.h>
#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jpeglib.h>

#if !defined(WITH_D)
 #define WITH_D 0
#endif
#if WITH_D
 #define D if(1)
#else
 #define D if(0)
#endif

#define U(x) (__builtin_expect(!!(x), 0))
#define L(x) (__builtin_expect(!!(x), 1))

// https://github.com/libjpeg-turbo/libjpeg-turbo/blob/c23672c/jutils.c#L84
#define round_up(a, b) (((a) + (b) - 1) - (((a) + (b) - 1) & ((b) - 1)))
_Static_assert(round_up(7, 8) == 8, "");
_Static_assert(round_up(8, 8) == 8, "");
_Static_assert(round_up(9, 8) == 16, "");
_Static_assert(round_up(15, 16) == 16, "");
_Static_assert(round_up(16, 16) == 16, "");
_Static_assert(round_up(17, 16) == 32, "");

// https://github.com/libjpeg-turbo/libjpeg-turbo/blob/c23672c/jutils.c#L75
#define jdiv_round_up(a, b) (((a) + (b) - 1) / (b))
_Static_assert(jdiv_round_up(7, 8) == 1, "");
_Static_assert(jdiv_round_up(8, 8) == 1, "");
_Static_assert(jdiv_round_up(9, 8) == 2, "");
_Static_assert(jdiv_round_up(15, 16) == 1, "");
_Static_assert(jdiv_round_up(16, 16) == 1, "");
_Static_assert(jdiv_round_up(17, 16) == 2, "");

// https://github.com/libjpeg-turbo/libjpeg-turbo/blob/c23672c/jutils.c#L119
static void jcopy_block_row(JBLOCKROW input_row, JBLOCKROW output_row, JDIMENSION num_blocks)
{
	memcpy(output_row, input_row, num_blocks*(DCTSIZE2*sizeof(JCOEF)));
}

// https://en.wikipedia.org/wiki/Mathematics
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// https://stackoverflow.com/a/600306
#define ispow2(x) ((x) != 0 && ((x) & ((x) - 1)) == 0)
_Static_assert(!ispow2(0), "");
_Static_assert( ispow2(1), "");
_Static_assert( ispow2(2), "");
_Static_assert(!ispow2(3), "");
_Static_assert(!ispow2(7), "");
_Static_assert( ispow2(8), "");
_Static_assert(!ispow2(9), "");
_Static_assert(!ispow2(15), "");
_Static_assert( ispow2(16), "");
_Static_assert(!ispow2(17), "");

struct jc {
	struct jpeg_compress_struct dstinfo;
	jvirt_barray_ptr *dst_coef_arrays;

	struct jc_image {
		struct jpeg_decompress_struct srcinfo;
		jvirt_barray_ptr *src_coef_arrays;
	} *images;
	unsigned images_cnt;

#define x_y_w_to_i(x, y, w) ((y)*(w) + (x))
#define i_w_to_x_y(i, w, xp, yp) ({ *(xp) = (i)%(w); *(yp) = ((i)-*(xp))/(w); })
	struct jc_block {
		short src_x;
		short src_y;
		unsigned short img_idx;
		unsigned short initialized;
	} *blocks;
	unsigned blocks_cnt;
	unsigned blocks_arr_width;
	unsigned blocks_arr_height;

	struct {
		FILE *f;
		int w;
		int h;
	} params;

	struct jc_errmgr {
		struct jpeg_error_mgr jerr; // must be the first member
		bool valid;
		jmp_buf ret;
	} err;
};

#if 1
 #define JC_TRY(self) if (self->err.valid = true, __builtin_expect(setjmp(self->err.ret) == 0, 1))
 #define JC_CATCH(self) else if (self->err.valid = false, 1)
 #define JC_ENDTRY(self) self->err.valid = false
#else
 #define JC_TRY(self) if (1)
 #define JC_CATCH(self) else if (0)
 #define JC_ENDTRY(self)
#endif

__attribute__((cold))
static void jc_error_handler(j_common_ptr cinfo)
{
	struct jpeg_error_mgr *errmgr = cinfo->err;
	struct jc_errmgr *err = (struct jc_errmgr *)errmgr;
	char errmsg[JMSG_LENGTH_MAX];

	if L (err->valid) {
		err->valid = false;
		longjmp(err->ret, 1);
	} else {
		cinfo->err->format_message(cinfo, errmsg);
		fprintf(stderr, "jcanvas: unexpected error: %s\n", errmsg);
		abort();
	}
}

// -----------------------------------------------------------------------------

struct jc *jc_new(const char *savepath, int w, int h)
{
	struct jc *self;
	FILE *f;

	if U (!(self = calloc(1, sizeof(*self))))
		return NULL;

	if U (!(f = fopen(savepath, "w"))) {
		free(self);
		return NULL;
	}

	self->dstinfo.err = jpeg_std_error(&self->err.jerr);
	self->err.jerr.error_exit = jc_error_handler;

	jpeg_create_compress(&self->dstinfo);
	jpeg_stdio_dest(&self->dstinfo, f);

	self->params.f = f;
	self->params.w = w;
	self->params.h = h;

	return self;
}

// -----------------------------------------------------------------------------

static struct jc_image *jc_alloc_next_image(struct jc *self);
static const char *jc_check_supported(struct jc *self, j_decompress_ptr img);
static const char *jc_check_compatible(struct jc *self, j_decompress_ptr img0, j_decompress_ptr imgx);
static bool jc_alloc_output(struct jc *self, struct jc_image *image);

int jc_add_image(struct jc *self, const char *path)
{
	FILE *f;
	struct jc_image *image;
	const char *reason;

	if U (!self)
		return -1;

	if U (!(f = fopen(path, "r")))
		return -1;

	if U (!(image = jc_alloc_next_image(self))) {
		fclose(f);
		return -1;
	}

	image->srcinfo.err = &self->err.jerr;
	jpeg_create_decompress(&image->srcinfo);
	jpeg_stdio_src(&image->srcinfo, f);

	JC_TRY(self) {
		jpeg_read_header(&image->srcinfo, /* require_image */ TRUE);
	} JC_CATCH(self) {
		jpeg_destroy_decompress(&image->srcinfo);
		fclose(f);
		return -1;
	} JC_ENDTRY(self);

	image->src_coef_arrays = jpeg_read_coefficients(&image->srcinfo);
	jpeg_stdio_src(&image->srcinfo, NULL);
	fclose(f); f = NULL;

	if U ((reason = jc_check_supported(self, &image->srcinfo)) ||
	      (self->images_cnt > 0 && (reason = jc_check_compatible(self, &self->images[0].srcinfo, &image->srcinfo)))) {
		jpeg_destroy_decompress(&image->srcinfo);
		return -1;
	}

	if (self->images_cnt == 0) {
		if U (!jc_alloc_output(self, image)) {
			jpeg_destroy_decompress(&image->srcinfo);
			return -1;
		}
	}

	return self->images_cnt++;
}

static struct jc_image *jc_alloc_next_image(struct jc *self)
{
	struct jc_image *newimages;
	struct jc_image *image;

	newimages = reallocarray(self->images, self->images_cnt+1, sizeof(*self->images));
	if U (!newimages)
		return NULL;

	self->images = newimages;

	image = &self->images[self->images_cnt];
	memset(image, 0, sizeof(*image));

	return image;
}

static const char *jc_check_supported(struct jc *self, j_decompress_ptr img)
{
	if U (img->arith_code)
		return "image uses arithmetic coding";

#if JPEG_LIB_VERSION >= 70
	// https://libjpeg-turbo.org/About/SmartScale
	if U (img->scale_num != 1 || img->scale_denom != 1)
		return "image uses DCT scaling";
#endif

	// these are assumed to be powers of two, might as well check them
D	assert(img->max_h_samp_factor > 0 && ispow2(img->max_h_samp_factor));
D	assert(img->max_v_samp_factor > 0 && ispow2(img->max_v_samp_factor));
	for (int ci = 0; ci < img->num_components; ci++) {
		jpeg_component_info *compptr = &img->comp_info[ci];
D		assert(compptr->h_samp_factor > 0 && ispow2(compptr->h_samp_factor));
D		assert(compptr->v_samp_factor > 0 && ispow2(compptr->v_samp_factor));
	}

	return NULL;
}

static const char *jc_check_compatible(struct jc *self, j_decompress_ptr img0, j_decompress_ptr imgx)
{
	if U (imgx->jpeg_color_space != img0->jpeg_color_space)
		return "image has a different color space";
	if U (imgx->num_components != img0->num_components)
		return "image has a different number of components";

	if U (imgx->jpeg_color_space == JCS_UNKNOWN)
		return "can't combine images with unknown color spaces";

	for (int i = 0; i < img0->num_components; i++) {
		jpeg_component_info *c0 = &img0->comp_info[i];
		jpeg_component_info *cx = &imgx->comp_info[i];
		void *q0, *qx;
		size_t qsz;

		if U (c0->h_samp_factor != cx->h_samp_factor)
			goto err_sampling;
		if U (c0->v_samp_factor != cx->v_samp_factor)
			goto err_sampling;
		if U (c0->quant_tbl_no != cx->quant_tbl_no)
			goto err_quant_no;

		q0 = img0->quant_tbl_ptrs[c0->quant_tbl_no]->quantval;
		qx = imgx->quant_tbl_ptrs[c0->quant_tbl_no]->quantval;
		qsz = sizeof(img0->quant_tbl_ptrs[0]->quantval);

		if U (memcmp(q0, qx, qsz) != 0)
			goto err_quanttable;
	}

	return NULL;
err_sampling:
	return "image has different subsampling";
err_quanttable:
	return "image has different quantization tables";
err_quant_no:
	return "components use different quantization table indexes";
}

static bool jc_alloc_blocks(struct jc *self, int w, int h);

static bool jc_alloc_output(struct jc *self, struct jc_image *image)
{
	struct jpeg_decompress_struct *srcinfo = &image->srcinfo;
	jvirt_barray_ptr *coef_arrays;
	int w = self->params.w;
	int h = self->params.h;
	int dataw, datah;

	if (w == -1)
		w = image->srcinfo.image_width;
	if (h == -1)
		h = image->srcinfo.image_height;

	dataw = round_up(w, srcinfo->max_h_samp_factor*DCTSIZE);
	datah = round_up(h, srcinfo->max_v_samp_factor*DCTSIZE);
	if U (!jc_alloc_blocks(self, dataw, datah))
		return false;

	coef_arrays = self->dstinfo.mem->alloc_small(
	    (j_common_ptr)&self->dstinfo,
	    JPOOL_IMAGE,
	    srcinfo->num_components*sizeof(jvirt_barray_ptr));

	for (int ci = 0; ci < srcinfo->num_components; ci++) {
		jpeg_component_info *compptr = &srcinfo->comp_info[ci];
		int width_in_blocks = jdiv_round_up(w, (srcinfo->max_h_samp_factor>>(compptr->h_samp_factor>>1))*DCTSIZE);
		int height_in_blocks = jdiv_round_up(h, (srcinfo->max_v_samp_factor>>(compptr->v_samp_factor>>1))*DCTSIZE);

		// check that the size calculation matches the original

D		assert(!(w > srcinfo->image_width) || width_in_blocks >= compptr->width_in_blocks);
D		assert(!(w == srcinfo->image_width) || width_in_blocks == compptr->width_in_blocks);
D		assert(!(w < srcinfo->image_width) || width_in_blocks <= compptr->width_in_blocks);

D		assert(!(h > srcinfo->image_height) || height_in_blocks >= compptr->height_in_blocks);
D		assert(!(h == srcinfo->image_height) || height_in_blocks == compptr->height_in_blocks);
D		assert(!(h < srcinfo->image_height) || height_in_blocks <= compptr->height_in_blocks);

		coef_arrays[ci] = self->dstinfo.mem->request_virt_barray(
		    (j_common_ptr)&self->dstinfo,
		    /* pool_id */ JPOOL_IMAGE,
		    /* pre_zero */ FALSE,
		    /* blocksperrow */ width_in_blocks*compptr->h_samp_factor,
		    /* numrows */ height_in_blocks*compptr->v_samp_factor,
		    /* maxaccess */ height_in_blocks*compptr->v_samp_factor);
	}

	jpeg_copy_critical_parameters(srcinfo, &self->dstinfo);

	self->dstinfo.image_width = w;
	self->dstinfo.image_height = h;

#if JPEG_LIB_VERSION >= 70
	self->dstinfo.jpeg_width = w;
	self->dstinfo.jpeg_height = h;
#endif

	JC_TRY(self) {
		jpeg_write_coefficients(&self->dstinfo, coef_arrays);
	} JC_CATCH(self) {
		return false;
	} JC_ENDTRY(self);

	self->dst_coef_arrays = coef_arrays;
	self->params.w = w;
	self->params.h = h;

	return true;
}

static bool jc_alloc_blocks(struct jc *self, int w, int h)
{
	struct jc_block *blocks;
	unsigned blkw = jdiv_round_up(w, 8);
	unsigned blkh = jdiv_round_up(h, 8);
	unsigned blocks_cnt = blkw*blkh;

	blocks = calloc(blocks_cnt, sizeof(*blocks));
	if U (!blocks)
		return false;

	free(self->blocks);

	self->blocks = blocks;
	self->blocks_cnt = blocks_cnt;
	self->blocks_arr_width = blkw;
	self->blocks_arr_height = blkh;

	return true;
}

// -----------------------------------------------------------------------------

bool jc_get_info(struct jc *self, int idx, struct jc_info_struct *info_out)
{
	j_decompress_ptr srcinfo;
	j_compress_ptr dstinfo;

	memset(info_out, 0, sizeof(*info_out));

	if U (!self)
		return false;
	if U (self->images_cnt == 0)
		return false;

	if (idx >= 0 && idx < self->images_cnt) {
		srcinfo = &self->images[idx].srcinfo;

		info_out->width = srcinfo->image_width;
		info_out->height = srcinfo->image_height;
	} else if (idx == JC_SELF) {
		srcinfo = &self->images[0].srcinfo;
		dstinfo = &self->dstinfo;

		info_out->width = dstinfo->image_width;
		info_out->height = dstinfo->image_height;
	} else {
		return false;
	}

	info_out->block_width = srcinfo->max_h_samp_factor*DCTSIZE;
	info_out->block_height = srcinfo->max_v_samp_factor*DCTSIZE;

	info_out->data_width = round_up(info_out->width, info_out->block_width);
	info_out->data_height = round_up(info_out->height, info_out->block_height);

	return true;
}

// -----------------------------------------------------------------------------

bool jc_drawimage(struct jc *self, int idx,
	unsigned destX, unsigned destY,
	unsigned srcX, unsigned srcY,
	int width, int height)
{
	struct jc_info_struct destinfo;
	struct jc_info_struct srcinfo;
	int error = 0;
	int offX, offY;
	unsigned blkw;

	if U (!jc_get_info(self, JC_SELF, &destinfo) ||
	      !jc_get_info(self, idx, &srcinfo))
		return false;

	if (width == -1)
		width = MIN(srcinfo.data_width-srcX, destinfo.data_width-destX);
	if (height == -1)
		height = MIN(srcinfo.data_height-srcY, destinfo.data_height-destY);

	if U (srcX+width > srcinfo.data_width)
		return false;
	if U (srcY+height > srcinfo.data_height)
		return false;

	if U (destX+width > destinfo.data_width)
		return false;
	if U (destY+height > destinfo.data_height)
		return false;

	if U (width <= 0)
		return false;
	if U (height <= 0)
		return false;

	error |= srcX&(srcinfo.block_width-1);
	error |= srcY&(srcinfo.block_height-1);

	error |= destX&(destinfo.block_width-1);
	error |= destY&(destinfo.block_height-1);

	error |= width&(srcinfo.block_width-1);
	error |= height&(srcinfo.block_height-1);

	if U (error != 0)
		return false;

	srcX >>= 3;
	srcY >>= 3;
	destX >>= 3;
	destY >>= 3;
	width >>= 3;
	height >>= 3;

	blkw = self->blocks_arr_width;

	if (idx != JC_SELF) {
		for (offY = 0; offY < height; offY++) {
D			assert(destY+offY < self->blocks_arr_height);
			for (offX = 0; offX < width; offX++) {
				unsigned i = x_y_w_to_i(destX+offX, destY+offY, blkw);

D				assert(destX+offX < self->blocks_arr_width);
D				assert(i < self->blocks_cnt);

				self->blocks[i].src_x = srcX+offX;
				self->blocks[i].src_y = srcY+offY;
				self->blocks[i].img_idx = idx;
				self->blocks[i].initialized = 1;

D				assert(self->blocks[i].src_x*8 < srcinfo.data_width);
D				assert(self->blocks[i].src_y*8 < srcinfo.data_height);
			}
		}
	} else {
D		assert(srcX+width <= self->blocks_arr_width);
D		assert(destX+width <= self->blocks_arr_width);

D		assert(srcY+height <= self->blocks_arr_height);
D		assert(destY+height <= self->blocks_arr_height);

#pragma nounroll
		for (offY = 0; offY < height; offY++) {
			unsigned srcI = x_y_w_to_i(srcX, srcY+offY, blkw);
			unsigned destI = x_y_w_to_i(destX, destY+offY, blkw);

D			assert(srcI < self->blocks_cnt);
D			assert(destI < self->blocks_cnt);

			memcpy(
			    &self->blocks[destI],
			    &self->blocks[srcI],
			    width*sizeof(*self->blocks));
		}
	}

	return true;
}

// -----------------------------------------------------------------------------

static bool jc_apply_blocks(struct jc *self);

bool jc_save_and_free(struct jc *self)
{
	bool rv = false;

	if L (self) {
		if L (self->images_cnt != 0 && jc_apply_blocks(self)) {
			jpeg_finish_compress(&self->dstinfo);
			rv = true;
		}
		jpeg_destroy_compress(&self->dstinfo);

		for (int i = 0; i < self->images_cnt; i++)
			jpeg_destroy_decompress(&self->images[i].srcinfo);

		fclose(self->params.f);

		free(self->images);
		free(self->blocks);
		free(self);
	}

	return rv;
}

static bool jc_apply_blocks(struct jc *self)
{
	struct jc_image *images = self->images;
	j_decompress_ptr srcinfo0 = &self->images[0].srcinfo;

	const __auto_type access_virt_barray = self->dstinfo.mem->access_virt_barray;

	int blkw = self->blocks_arr_width;
	int blkh = self->blocks_arr_height;

	int init;

D	assert(access_virt_barray != NULL); // double free

	init = 1;
	for (int i = 0; i < self->blocks_cnt; i++)
		init &= self->blocks[i].initialized;
	if U (!init)
		return false;

	for (int ci = 0; ci < self->dstinfo.num_components; ci++) {
		jpeg_component_info *compptr = &self->dstinfo.comp_info[ci];

		// how many 8x8 blocks in one subsampled block
		int x_howmany = srcinfo0->max_h_samp_factor>>(compptr->h_samp_factor>>1);
		int y_howmany = srcinfo0->max_v_samp_factor>>(compptr->v_samp_factor>>1);

		// shift amount for converting between 8x8 and subsampled block sizes
		int x_howmany_s = x_howmany>>1;
		int y_howmany_s = y_howmany>>1;

		JBLOCKARRAY dst_row;
		dst_row = access_virt_barray(
		    (j_common_ptr)&self->dstinfo, self->dst_coef_arrays[ci],
		    /* start_row */ 0,
		    /* num_rows */ blkh>>y_howmany_s,
		    /* writable */ TRUE);

		for (int dy = 0; dy < blkh; dy += y_howmany) {
			for (int dx = 0; dx < blkw; dx += x_howmany) {
				int i = x_y_w_to_i(dx, dy, blkw);
				struct jc_image *img = &images[self->blocks[i].img_idx];
				int sx = self->blocks[i].src_x;
				int sy = self->blocks[i].src_y;
				JBLOCKARRAY src_row;

D				assert(i >= 0 && i < self->blocks_cnt);

				src_row = access_virt_barray(
				    (j_common_ptr)&img->srcinfo, img->src_coef_arrays[ci],
				    /* start_row */ sy>>y_howmany_s,
				    /* num_rows */ 1,
				    /* writable */ FALSE);

				jcopy_block_row(
				    &src_row[0][sx>>x_howmany_s],
				    &dst_row[dy>>y_howmany_s][dx>>x_howmany_s], 1);
			}
D			assert(dx == self->blocks_arr_width);
		}
D		assert(dy == self->blocks_arr_height);
	}

	return true;
}
