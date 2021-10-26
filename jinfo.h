#pragma once

#include <stdbool.h>
#include <stddef.h>

struct jpeg_info_struct1
{
	int width;
	int height;
};

bool jinfo(const char *path, void *p, size_t psz);
bool jinfo_mem(const char *buf, size_t bufsz, void *p, size_t psz);
