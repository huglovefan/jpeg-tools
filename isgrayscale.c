#include "isgrayscale.h"

#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <jpeglib.h>

#define U(x) (__builtin_expect(!!(x), 0))
#define L(x) (__builtin_expect(!!(x), 1))

__attribute__((cold))
static void isgrayscale_error_handler(j_common_ptr cinfo)
{
	char errmsg[JMSG_LENGTH_MAX];

	cinfo->err->format_message(cinfo, errmsg);
	fprintf(stderr, "isgrayscale: libjpeg error: %s\n", errmsg);
	longjmp(*(jmp_buf *)cinfo->client_data, 1);
}

enum grayscale_status isgrayscale(const char *path)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE *f;
	unsigned char **bufs;
	int output_height, output_width;
	unsigned error = 0;
	unsigned char *onebuf;
	jmp_buf catch;
	volatile enum {
		state_init = 0,
		decompress_created,
		decompress_started,
	} state = state_init;

	f = fopen(path, "r");
	if U (!f) {
		perror("isgrayscale: failed to open input file");
		return gss_error;
	}

	cinfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = isgrayscale_error_handler;

	cinfo.client_data = &catch;
	if U (setjmp(catch) != 0) {
		switch (state) {
		case decompress_started:
			state = decompress_created;
			if (cinfo.output_scanline < output_height)
				jpeg_abort_decompress(&cinfo);
			else
				jpeg_finish_decompress(&cinfo);
		case decompress_created:
			state = state_init;
			jpeg_destroy_decompress(&cinfo);
		case state_init:
			break;
		}
		fclose(f);
		return gss_error;
	}

	jpeg_create_decompress(&cinfo);
	state = decompress_created;

	jpeg_stdio_src(&cinfo, f);
	jpeg_read_header(&cinfo, TRUE);

	if U (cinfo.jpeg_color_space == JCS_GRAYSCALE) {
		fclose(f);
		return gss_yes;
	}

	if U (cinfo.out_color_space != JCS_RGB) {
		fprintf(stderr, "isgrayscale: unsupported color space\n");
		fclose(f);
		return gss_error;
	}

	cinfo.out_color_space = JCS_EXT_RGBX;
//	cinfo.dct_method = JDCT_IFAST; // causes disagreements with imagemagick
	cinfo.do_fancy_upsampling = FALSE;
	cinfo.do_block_smoothing = FALSE;

	jpeg_start_decompress(&cinfo);
	state = decompress_started;

	output_width = cinfo.output_width;
	output_height = cinfo.output_height;

	__builtin_assume(output_width > 0);

	bufs = alloca(cinfo.rec_outbuf_height*sizeof(void *));
	onebuf = __builtin_alloca_with_align(cinfo.rec_outbuf_height*(output_width*4), 16*8);
	for (int i = 0; i < cinfo.rec_outbuf_height; i++)
		bufs[i] = &onebuf[i*(output_width*4)];

	while (cinfo.output_scanline < output_height) {
		int lines;

		lines = jpeg_read_scanlines(&cinfo, bufs, cinfo.rec_outbuf_height);
		for (int j = 0; j < lines*output_width*4; j += 4) {
			unsigned r, g, b;
			r = onebuf[j];
			g = onebuf[j+1];
			b = onebuf[j+2];
			error |= r^g;
			error |= r^b;
		}
		if U (error)
			break;
	}

	state = decompress_created;
	if (cinfo.output_scanline < output_height)
		jpeg_abort_decompress(&cinfo);
	else
		jpeg_finish_decompress(&cinfo);

	state = state_init;
	jpeg_destroy_decompress(&cinfo);

	fclose(f);

	return (error == 0) ? gss_yes : gss_no;
}

__attribute__((weak))
int main(int argc, char **argv)
{
	if U (argc != 2) {
		fprintf(stderr, "usage: isgrayscale <file>\n");
		return gss_error;
	}

	return isgrayscale(argv[1]);
}
