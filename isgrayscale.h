#pragma once

enum grayscale_status {
	gss_yes = 0,
	gss_no = 1,
	gss_error = 2,
};

enum grayscale_status isgrayscale(const char *path);
