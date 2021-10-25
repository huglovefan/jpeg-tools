#pragma once

#include <stdbool.h>
#include <stddef.h>

struct resave_opts {
	bool grayscale;
	bool optimize;
	bool progressive;
};

bool resave(const char *inpath, const char *outpath, const struct resave_opts *opts);
bool resave_mem(const char *buf, size_t sz, const char *outpath, const struct resave_opts *opts);
