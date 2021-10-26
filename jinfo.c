#include "jinfo.h"

#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <jpeglib.h>

#define U(x) (__builtin_expect(!!(x), 0))
#define L(x) (__builtin_expect(!!(x), 1))

__attribute__((cold))
static void jinfo_error_handler(j_common_ptr cinfo)
{
	char errmsg[JMSG_LENGTH_MAX];

	cinfo->err->format_message(cinfo, errmsg);
	fprintf(stderr, "jinfo: libjpeg error: %s\n", errmsg);
	longjmp(*(jmp_buf *)cinfo->client_data, 1);
}

struct input {
	enum {
		input_path,
		input_buf,
	} which;
	union {
		const char *path;
		struct {
			const char *p;
			size_t sz;
		} buf;
	} u;
};

static bool jinfo_real(const struct input *in, void *p, size_t psz);

bool jinfo(const char *path, void *p, size_t psz)
{
	return jinfo_real(
		&(struct input){.which = input_path, .u = {.path = path}},
		p,
		psz
	);
}

bool jinfo_mem(const char *buf, size_t bufsz, void *p, size_t psz)
{
	return jinfo_real(
		&(struct input){.which = input_buf, .u = {.buf = {.p = buf, .sz = bufsz}}},
		p,
		psz
	);
}

union any
{
	struct jpeg_info_struct1 *jis1;
	void *p;
};

static bool jinfo_real(const struct input *in, void *p, size_t psz)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE *f = NULL;
	jmp_buf catch;
	volatile enum {
		state_init = 0,
		decompress_created,
	} state = state_init;
	union any out = {.p = p};
	bool ok = false;

	switch (in->which) {
	case input_path:
		f = fopen(in->u.path, "rb");
		if U (!f) {
			perror("jinfo: failed to open input file");
			return false;
		}
		break;
	case input_buf:
		break;
	}

	cinfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = jinfo_error_handler;

	cinfo.client_data = &catch;
	if U (setjmp(catch) != 0) {
		switch (state) {
		case decompress_created:
			state = state_init;
			jpeg_destroy_decompress(&cinfo);
		case state_init:
			break;
		}
		if (f)
			fclose(f);
		return false;
	}

	jpeg_create_decompress(&cinfo);
	state = decompress_created;

	switch (in->which) {
	case input_path:
		jpeg_stdio_src(&cinfo, f);
		break;
	case input_buf:
		jpeg_mem_src(&cinfo, (const unsigned char *)in->u.buf.p, in->u.buf.sz);
		break;
	}

	jpeg_read_header(&cinfo, /* require_image */ FALSE);

	ok = true;
	switch (psz)
	{
	case sizeof(struct jpeg_info_struct1):
		out.jis1->width = cinfo.image_width;
		out.jis1->height = cinfo.image_height;
		break;
	default:
		fprintf(stderr, "jinfo: unknown struct size %zu\n", psz);
		ok = false;
		break;
	}

	state = state_init;
	jpeg_destroy_decompress(&cinfo);

	if (f)
		fclose(f);

	return ok;
}
