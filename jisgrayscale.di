extern (C):

// (filename starts with j because it can't have the same name as the function)

enum grayscale_status {
	gss_yes = 0,
	gss_no = 1,
	gss_error = 2,
};

grayscale_status isgrayscale(const(char)* path);
