#include "jresave.h"

#include <assert.h>
#include <errno.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <jpeglib.h>

#define U(x) (__builtin_expect(!!(x), 0))
#define L(x) (__builtin_expect(!!(x), 1))

__attribute__((cold))
static void resave_error_handler(j_common_ptr cinfo)
{
	char errmsg[JMSG_LENGTH_MAX];

	cinfo->err->format_message(cinfo, errmsg);
	fprintf(stderr, "resave: libjpeg error: %s\n", errmsg);
	longjmp(*(jmp_buf *)cinfo->client_data, 1);
}

#define TMPSUF ".tmp"

bool resave(const char *inpath, const char *outpath, const struct resave_opts *opts)
{
	struct jpeg_decompress_struct srcinfo;
	struct jpeg_compress_struct dstinfo;
	struct jpeg_error_mgr jerr;
	jvirt_barray_ptr *src_coef_arrays;
	jmp_buf catch;
	FILE *infile, *outfile;
	size_t outpathlen;
	char *tmpoutpath;
	volatile enum {
		state_init = 0,
		created_decompress_only,
		created_decompress_and_compress,
	} state = state_init;

	outpathlen = strlen(outpath);
	tmpoutpath = alloca(outpathlen+strlen(TMPSUF)+1);
	memcpy(tmpoutpath, outpath, outpathlen);
	memcpy(tmpoutpath+outpathlen, TMPSUF, sizeof(TMPSUF));

	infile = fopen(inpath, "r");
	outfile = fopen(tmpoutpath, "w");

	if U (!infile) {
		perror("resave: failed to open input file");
		return false;
	}
	if U (!outfile) {
		perror("resave: failed to open output file");
		fclose(infile);
		return false;
	}

	dstinfo.err = jpeg_std_error(&jerr);
	srcinfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = resave_error_handler;

	dstinfo.client_data = &catch;
	srcinfo.client_data = &catch;
	if U (setjmp(catch) != 0) {
		switch (state) {
		case created_decompress_and_compress:
			state = created_decompress_only;
			jpeg_destroy_compress(&dstinfo);
		case created_decompress_only:
			state = state_init;
			jpeg_destroy_decompress(&srcinfo);
		case state_init:
			break;
		}
		fclose(infile);
		fclose(outfile);
		return false;
	}

	jpeg_create_decompress(&srcinfo);
	state = created_decompress_only;
	jpeg_stdio_src(&srcinfo, infile);
	jpeg_read_header(&srcinfo, TRUE);
	src_coef_arrays = jpeg_read_coefficients(&srcinfo);

	jpeg_create_compress(&dstinfo);
	state = created_decompress_and_compress;
	jpeg_stdio_dest(&dstinfo, outfile);
	jpeg_copy_critical_parameters(&srcinfo, &dstinfo);

	dstinfo.optimize_coding = !!opts->optimize;
	if (opts->grayscale) {
		dstinfo.jpeg_color_space = JCS_GRAYSCALE;
		dstinfo.num_components = 1;
		dstinfo.max_h_samp_factor = 1;
		dstinfo.max_v_samp_factor = 1;
		dstinfo.comp_info[0].h_samp_factor = 1;
		dstinfo.comp_info[0].v_samp_factor = 1;
	}
	if (opts->progressive)
		jpeg_simple_progression(&dstinfo);

	jpeg_write_coefficients(&dstinfo, src_coef_arrays);
	jpeg_finish_compress(&dstinfo);
	jpeg_destroy_compress(&dstinfo);
	state = created_decompress_only;

	jpeg_destroy_decompress(&srcinfo);
	state = state_init;

	fclose(infile);
	fclose(outfile);

	if (rename(tmpoutpath, outpath) == -1) {
		perror("resave: rename");
		if (unlink(tmpoutpath) == -1)
			perror("resave: unlink");
		return false;
	}

	return true;
}

__attribute__((weak))
int main(int argc, char **argv)
{
	struct resave_opts opts = {
		.optimize = 0,
		.progressive = 0,
		.grayscale = 0,
	};

	while (argc > 1) {
		if (argv[1][0] != '-') break;
		else if (strcmp(argv[1], "-grayscale") == 0) opts.grayscale = 1;
		else if (strcmp(argv[1], "-optimize") == 0) opts.optimize = 1;
		else if (strcmp(argv[1], "-progressive") == 0) opts.progressive = 1;
		else {
			fprintf(stderr, "jresave: unknown option \"%s\"\n", argv[1]);
			goto usage;
		}
		argc--;
		argv++;
	}

	if (argc != 3) {
usage:
		fprintf(stderr,
		    "usage: jresave [options] <infile> <outfile>\n"
		    "options:\n"
		    "    -grayscale    drop color channels from the image\n"
		    "    -optimize     save with optimized huffman tables\n"
		    "    -progressive  save as progressive jpeg\n"
		    );
		return 1;
	}

	if (!resave(argv[1], argv[2], &opts))
		return 1;

	return 0;
}
