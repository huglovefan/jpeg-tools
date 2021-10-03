#!/usr/bin/env python3

import json
import os
import sys

from os.path import dirname, realpath

from cffi import FFI

ffi = FFI()

# contents of the header file
ffi.cdef("""
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
""")

DLL = ".dll" if sys.platform == "win32" else ".so"

libjcanvas = ffi.dlopen(dirname(realpath(__file__))+"/jcanvas"+DLL)

# wrapper class for the C API
class JPEGCanvas:

	def __init__(self, out_path, width, height):
		self.canvas = libjcanvas.jc_new(out_path.encode("utf-8"), width, height)
		if self.canvas == ffi.NULL:
			raise Exception("jc_new failed")

	def add_image(self, in_path):
		idx = libjcanvas.jc_add_image(self.canvas, in_path.encode("utf-8"))
		if idx == -1:
			# (this thing really doesn't have proper error reporting)
			raise Exception("jc_add_image failed")
		return idx

	def get_info(self, idx):
		info = ffi.new("struct jc_info_struct[1]")
		if not libjcanvas.jc_get_info(self.canvas, idx, info):
			raise Exception("jc_get_info failed")
		return {
			"width": info[0].width,
			"height": info[0].height,
			"data_width": info[0].data_width,
			"data_height": info[0].data_height,
			"block_width": info[0].block_width,
			"block_height": info[0].block_height,
		}

	def drawimage(self, idx, dx, dy, sx, sy, w, h):
		if not libjcanvas.jc_drawimage(self.canvas, idx, dx, dy, sx, sy, w, h):
			raise Exception("jc_drawimage failed")

	def paste(self, idx):
		self.drawimage(idx, 0, 0, 0, 0, -1, -1)

	def save(self):
		success = libjcanvas.jc_save_and_free(self.canvas)
		self.canvas = ffi.NULL
		return success

rflag = False
vflag = False
width = -1
height = -1

while len(sys.argv) > 1:
	n = 1
	if sys.argv[1] == "-r":
		rflag = True
	elif sys.argv[1] == "-v":
		vflag = True
	elif sys.argv[1] == "-c" and len(sys.argv) > 2:
		parts = sys.argv[2].split("x")
		width = int(parts[0])
		height = int(parts[1])
		if not (len(parts) == 2 and width > 0 and height > 0):
			break
		n = 2
	else:
		break
	while n > 0:
		sys.argv.pop(0)
		n -= 1

if len(sys.argv) != 3:
	print("usage: scranblw.py <infile> <outfile>", file=sys.stderr)
	sys.exit(1)

jc = JPEGCanvas(sys.argv[2], width, height)

idx = jc.add_image(sys.argv[1])

if vflag:
	info = jc.get_info(idx)
	print("width: "+str(info["width"]))
	print("height: "+str(info["height"]))
	print("data_width: "+str(info["data_width"]))
	print("data_height: "+str(info["data_height"]))
	print("block_width: "+str(info["block_width"]))
	print("block_height: "+str(info["block_height"]))

data = None
try:
	data = json.load(sys.stdin)
except json.JSONDecodeError as e:
	print("json parse error:", e, file=sys.stderr)
	sys.exit(1)

lastop = None
try:
	if type(data) is not list:
		raise Exception("json data is not an array")
	for op in data:
		lastop = op
		(destX, destY, srcX, srcY, blkwidth, blkheight) = op
		if len(op) != 6:
			raise Exception("operation has excess parameters")
		if rflag is True:
			(destX, destY, srcX, srcY) = (srcX, srcY, destX, destY)
		jc.drawimage(idx, destX, destY, srcX, srcY, blkwidth, blkheight)
except Exception as e:
	print("error applying transforms:", e, file=sys.stderr)
	if lastop is not None:
		print("the last operation was:", lastop, file=sys.stderr)
	sys.exit(1)

if len(data) == 0:
	jc.paste(idx)

jc.save()
