#pragma once

#include <stdbool.h>

enum jc_special_idx {
	JC_SELF = -2,
};

struct jc_info_struct {
	unsigned width;
	unsigned height;

	unsigned data_width;
	unsigned data_height;

	unsigned short block_width;
	unsigned short block_height;
};

struct jc *jc_new(const char *savepath, int w, int h);
int jc_add_image(struct jc *self, const char *path);
bool jc_get_info(struct jc *self, int idx, struct jc_info_struct *info_out);
bool jc_drawimage(struct jc *self, int idx,
	unsigned destX, unsigned destY,
	unsigned srcX, unsigned srcY,
	int width, int height);
bool jc_save_and_free(struct jc *self);
