extern (C):

enum jc_special_idx {
	JC_SELF = -2,
};

struct jc_info_struct {
	uint width;
	uint height;

	uint data_width;
	uint data_height;

	ushort block_width;
	ushort block_height;
};

struct jc;
jc* jc_new(const(char)* savepath, int w, int h);
int jc_add_image(jc* self, const(char)* path);
bool jc_get_info(jc* self, int idx, jc_info_struct* info_out);
bool jc_drawimage(jc* self, int idx,
	uint destX, uint destY,
	uint srcX, uint srcY,
	int width, int height);
bool jc_save_and_free(jc* self);
