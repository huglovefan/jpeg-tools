#include <assert.h>
#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jpeglib.h>

#define round_up(a, b) (((a) + (b) - 1) - (((a) + (b) - 1) & ((b) - 1)))
#define jdiv_round_up(a, b) (((a) + (b) - 1) / (b))

static void jcopy_block_row(JBLOCKROW input_row, JBLOCKROW output_row, JDIMENSION num_blocks)
{
	memcpy(output_row, input_row, num_blocks*(DCTSIZE2*sizeof(JCOEF)));
}

__attribute__((unused))
static int compare_dct(const void *p1, const void *p2)
{
	return memcmp(p1, p2, DCTSIZE2*sizeof(JCOEF));
}

__attribute__((cold))
static void error_handler(j_common_ptr cinfo)
{
	char errmsg[JMSG_LENGTH_MAX];

	cinfo->err->format_message(cinfo, errmsg);
	fprintf(stderr, "libjpeg error: %s\n", errmsg);
	exit(1);
}

int main(int argc, char **argv)
{
	struct jpeg_decompress_struct srcinfo;
	struct jpeg_compress_struct dstinfo;
	struct jpeg_error_mgr jerr;
	jvirt_barray_ptr *src_coef_arrays;
	jvirt_barray_ptr *dst_coef_arrays;
	FILE *infile, *outfile;
	int ci;

	if (argc != 3) {
		fprintf(stderr, "usage: jsort <infile> <outfile>\n");
		return 1;
	}

	infile = fopen(argv[1], "r");
	outfile = fopen(argv[2], "w");

	if (!infile) {
		perror("failed to open input file");
		return 1;
	}
	if (!outfile) {
		perror("failed to open output file");
		return 1;
	}

	dstinfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = error_handler;
	jpeg_create_compress(&dstinfo);
	jpeg_stdio_dest(&dstinfo, outfile);
	srcinfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = error_handler;
	jpeg_create_decompress(&srcinfo);
	jpeg_stdio_src(&srcinfo, infile);
	jpeg_read_header(&srcinfo, /* require_image */ TRUE);
	src_coef_arrays = jpeg_read_coefficients(&srcinfo);

	dst_coef_arrays = dstinfo.mem->alloc_small(
	    (j_common_ptr)&dstinfo,
	    JPOOL_IMAGE,
	    srcinfo.num_components*sizeof(jvirt_barray_ptr));

	for (ci = 0; ci < srcinfo.num_components; ci++) {
		jpeg_component_info *comp = &srcinfo.comp_info[ci];
		dst_coef_arrays[ci] = dstinfo.mem->request_virt_barray(
		    (j_common_ptr)&dstinfo,
		    /* pool_id */ JPOOL_IMAGE,
		    /* pre_zero */ TRUE,
		    /* blocksperrow */ comp->width_in_blocks*comp->h_samp_factor,
		    /* numrows */ comp->height_in_blocks*comp->v_samp_factor,
		    /* maxaccess */ comp->height_in_blocks*comp->v_samp_factor);
	}

	jpeg_copy_critical_parameters(&srcinfo, &dstinfo);

	jpeg_write_coefficients(&dstinfo, dst_coef_arrays);

	for (ci = 0; ci < srcinfo.num_components; ci++) {
		jpeg_component_info *comp;
		JBLOCKARRAY dstrow, srcrow;

		comp = &srcinfo.comp_info[ci];
		dstrow = dstinfo.mem->access_virt_barray(
		    (j_common_ptr)&dstinfo, dst_coef_arrays[ci],
		    /* start_row */ 0,
		    /* num_rows */ comp->height_in_blocks,
		    /* writable */ TRUE);

		for (int i = 0; i < comp->height_in_blocks; i++) {
			srcrow = srcinfo.mem->access_virt_barray(
			    (j_common_ptr)&srcinfo, src_coef_arrays[ci],
			    /* start_row */ i,
			    /* num_rows */ 1,
			    /* writable */ FALSE);

			jcopy_block_row(
			    &srcrow[0][0],
			    &dstrow[i][0],
			    comp->width_in_blocks);

//			for (int j = 0; j < comp->width_in_blocks; j++) {
//				for (int k = 0; k < DCTSIZE2; k++)
//					dstrow[i][j][k] *= 3;
//			}

//			qsort(
//			    &dstrow[i][0],
//			    comp->width_in_blocks,
//			    DCTSIZE2*sizeof(JCOEF),
//			    compare_dct);
		}

//		for (i = 0; i < comp->height_in_blocks; i++) {
//			for (int j = 0; j < comp->width_in_blocks; j++) {
//				for (int k = 0; k < DCTSIZE2; k++) {
//					dstrow[i][j][k] *= -1;
//				}
//			}
//		}

		qsort(
		    &dstrow[0][0],
		    (comp->width_in_blocks*
		     comp->height_in_blocks)*comp->v_samp_factor,
		    DCTSIZE2*sizeof(JCOEF),
		    compare_dct);
	}

	jpeg_finish_compress(&dstinfo);
	jpeg_destroy_compress(&dstinfo);
	jpeg_destroy_decompress(&srcinfo);
	fclose(infile);
	fclose(outfile);

	return 0;
}
