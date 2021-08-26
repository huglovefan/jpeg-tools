#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
 #include <unistd.h>
#endif

#include <jansson.h>

#include "jcanvas.h"

static unsigned rflag, zeroflag, strict;
static int width = -1, height = -1;

#define sscanf2_full(s, fmt, p1, p2) \
	({ int n_; (sscanf((s), (fmt "%n"), (p1), (p2), &n_) == 2 && (s)[n_] == '\0'); })

int main(int argc, char **argv)
{
	json_t *data;
	json_error_t jsonerr;
	struct jc *canvas;
	int idx;
	int i, j;
	json_t *row, *n;
	int drawok;

	while (argc > 1) {
		int end = 0, wantarg = 0, ch;
		if (argv[1][0] != '-') break;
		else if (!((ch = argv[1][1]) && !argv[1][2])) goto unkopt;
		else if (ch == '0') zeroflag = 1;
		else if (ch == 'c' && (wantarg++, argc > 2)) {
			if (!sscanf2_full(argv[2], "%dx%d", &width, &height)) {
				fprintf(stderr, "scramble: failed to parse dimensions from \"%s\"\n", argv[2]);
				goto usage;
			}
		}
		else if (ch == 'r') rflag = 1;
		else if (ch == 's') strict = 1;
		else if (ch == '-') end = 1;
		else {
			if (wantarg)
				fprintf(stderr, "scramble: missing argument for option \"%s\"\n", argv[1]);
			else
unkopt:
				fprintf(stderr, "scramble: unknown option \"%s\"\n", argv[1]);
			goto usage;
		}
		argv += 1+wantarg;
		argc -= 1+wantarg;
		if (end)
			break;
	}

	if (argc != 3) {
usage:
		fprintf(stderr,
		    "usage: scramble [options] <infile> <outfile>\n"
		    "options:\n"
		    "    -c WIDTHxHEIGHT  set dimensions of the output image\n"
		    "    -r               apply the operations in reverse\n"
		    "    -s               strict - exit with status 1 if any drawimage calls fail\n"
		    "    -0               no operations, just copy the image (for benchmarking)\n"
		    );
		return 1;
	}

	if (zeroflag) {
		canvas = jc_new(argv[2], width, height);
		idx = jc_add_image(canvas, argv[1]);
		jc_drawimage(canvas, idx, 0, 0, 0, 0, -1, -1);
		if (!jc_save_and_free(canvas)) {
			fprintf(stderr, "scramble: jc_save_and_free failed\n");
			return 1;
		}
		return 0;
	}

#if !defined(_WIN32)
	if (isatty(STDIN_FILENO))
		fprintf(stderr, "(reading decode data from stdin)\n");
#endif

	data = json_loadf(stdin, 0, &jsonerr);
	if (!data) {
		fprintf(stderr, "scramble: json parse error: %s\n", jsonerr.text);
		return 1;
	}
	if (!json_is_array(data)) {
		fprintf(stderr, "scramble: input is not a json array\n");
		return 1;
	}

	canvas = jc_new(argv[2], width, height);
	if (!canvas) {
		fprintf(stderr, "scramble: jc_new failed\n");
		return 1;
	}

	idx = jc_add_image(canvas, argv[1]);
	if (idx == -1) {
		fprintf(stderr, "scramble: jc_add_image failed\n");
		return 1;
	}

	drawok = 1;
	json_array_foreach(data, i, row) {
		int nums[6] = {0};
		int i = 0;
		json_array_foreach(row, j, n) {
			if (j < 6 && json_is_integer(n))
				nums[i++] = json_integer_value(n);
			else
				goto typeerr;
		}
		if (i != 6)
			goto typeerr;

		if (rflag) {
			int tmp;
			tmp = nums[0]; nums[0] = nums[2]; nums[2] = tmp;
			tmp = nums[1]; nums[1] = nums[3]; nums[3] = tmp;
		}

		drawok &= jc_drawimage(canvas, idx,
		    nums[0], nums[1], nums[2], nums[3], nums[4], nums[5]);

		continue;
typeerr:
		fprintf(stderr, "scramble: bad decode data: item at index %d has wrong type or length\n", i);
		return 1;
	}
	if (json_array_size(data) == 0)
		drawok &= jc_drawimage(canvas, idx, 0, 0, 0, 0, -1, -1);

	if (!drawok)
		fprintf(stderr, "scramble: %s: one or more jc_drawimage calls failed\n",
		    (strict) ? "error" : "warning");

	if (!jc_save_and_free(canvas)) {
		fprintf(stderr, "scramble: jc_save_and_free failed\n");
		return 1;
	}

	json_decref(data);

	if (strict && !drawok)
		return 1;

	return 0;
}
