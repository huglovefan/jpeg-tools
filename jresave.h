#pragma once

#include <stdbool.h>

struct resave_opts {
	bool grayscale;
	bool optimize;
	bool progressive;
};

bool resave(const char *inpath, const char *outpath, const struct resave_opts *opts);
