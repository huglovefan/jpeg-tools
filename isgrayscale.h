#pragma once

#include <stddef.h>

enum grayscale_status {
	gss_yes = 0,
	gss_no = 1,
	gss_error = 2,
};

enum grayscale_status isgrayscale(const char *path);
enum grayscale_status isgrayscale_mem(const char *buf, size_t sz);
