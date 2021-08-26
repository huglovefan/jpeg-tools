extern (C):

struct resave_opts {
	bool grayscale;
	bool optimize;
	bool progressive;
};

bool resave(const(char)* inpath, const(char)* outpath, const(resave_opts)* opts);
