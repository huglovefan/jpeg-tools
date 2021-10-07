import sys
import os
import traceback
import json

from cffi import FFI

ffi = FFI()

ffi.cdef("""
//
// jcanvas.h
//
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
int jc_add_image_from_memory(struct jc *self, const unsigned char *buf, size_t sz);
bool jc_get_info(struct jc *self, int idx, struct jc_info_struct *info_out);
bool jc_drawimage(struct jc *self, int idx,
	unsigned destX, unsigned destY,
	unsigned srcX, unsigned srcY,
	int width, int height);
bool jc_save_and_free(struct jc *self);

//
// isgrayscale.h
//
enum grayscale_status {
	gss_yes = 0,
	gss_no = 1,
	gss_error = 2,
};

enum grayscale_status isgrayscale(const char *path);

//
// jresave.h
//
struct resave_opts {
	bool grayscale;
	bool optimize;
	bool progressive;
};

bool resave(const char *inpath, const char *outpath, const struct resave_opts *opts);
""")

DLLEXT = ".dll" if sys.platform == "win32" else ".so"

libjpegtools = ffi.dlopen(os.path.realpath(".")+"/jpegtools"+DLLEXT)

class JPEGCanvas:

	# out_path: path where to save the output image
	# width, height: dimensions of the output image, or -1 to copy them from the first added image
	def __init__(self, out_path, width = -1, height = -1):
		# too-big ones will fail to allocate in add_image()
		assert width == -1 or width >= 1 and width < 0xffff
		assert height == -1 or height >= 1 and height < 0xffff
		self.canvas = libjpegtools.jc_new(out_path.encode("utf-8"), width, height)
		if self.canvas == ffi.NULL:
			# - allocation failed
			# - opening the output file failed
			raise Exception("jc_new failed")

	# adds an input image to this JPEGCanvas instance
	# returns a numeric id that can be used to refer to it in the other methods
	# in_path: path to the image to add
	def add_image(self, in_path):
		idx = libjpegtools.jc_add_image(self.canvas, in_path.encode("utf-8"))
		if idx == -1:
			# - opening or reading the input image failed
			# - image isn't compatible with previously added ones
			# - allocating the output image failed
			raise Exception("jc_add_image failed")
		return idx

	# adds an input image to this JPEGCanvas instance
	# returns a numeric id that can be used to refer to it in the other methods
	# buf: buffer containing the image file (bytes or bytearray)
	# sz: size of buf in bytes
	def add_image_from_memory(self, buf, sz):
		idx = libjpegtools.jc_add_image_from_memory(self.canvas, buf, sz)
		if idx == -1:
			# - reading the input image failed
			# - image isn't compatible with previously added ones
			# - allocating the output image failed
			raise Exception("jc_add_image failed")
		return idx

	# gets details about an input or output image
	# idx: index of an image previously added with add_image(), or JC_SELF (-2) for the output image
	def get_info(self, idx = libjpegtools.JC_SELF):
		info = ffi.new("struct jc_info_struct[1]")
		if not libjpegtools.jc_get_info(self.canvas, idx, info):
			# - image with this idx doesn't exist
			raise Exception("jc_get_info failed")
		return {
			"width": info[0].width,
			"height": info[0].height,
			# width/height rounded up to block_width/height
			"data_width": info[0].data_width,
			"data_height": info[0].data_height,
			# MCU block size, smallest unit that can be copied using drawimage()
			# (for multi-channel images, this is the largest value among all channels)
			"block_width": info[0].block_width,
			"block_height": info[0].block_height,
		}

	# idx: index of an image previously added with add_image()
	# dx, dy: coordinates in the output image
	# sx, sy: coordinates in the source image
	# w, h: width and height of the block
	def drawimage(self, idx, dx, dy, sx, sy, w, h):
		if not libjpegtools.jc_drawimage(self.canvas, idx, dx, dy, sx, sy, w, h):
			# - image with this idx doesn't exist
			# - coordinates or dimensions aren't multiples of the block size
			# - coordinates or dimensions are out of range
			raise Exception("jc_drawimage failed")

	# just paste the image in the top left corner
	# idx: index of an image previously added with add_image()
	def just_paste(self, idx):
		# -1 width/height copies them from the input image
		self.drawimage(idx, 0, 0, 0, 0, -1, -1)

	# writes the output image to disk
	# the JPEGCanvas instance can't be used anymore after this
	def save(self):
		success = libjpegtools.jc_save_and_free(self.canvas)
		self.canvas = ffi.NULL
		if not success:
			# - image contains uninitialized blocks that had nothing copied to them
			# - disk is full and the image couldn't be saved
			raise Exception("jc_save_and_free failed")

# perform "jpegtran -optimize -progressive" on an image
# this also converts it to a grayscale jpg if possible
def resave_optimized(path):
	opts = ffi.new("struct resave_opts[1]")
	opts[0].optimize = True
	opts[0].progressive = True
	if libjpegtools.isgrayscale(path.encode("utf-8")) == libjpegtools.gss_yes:
		opts[0].grayscale = True
	if not libjpegtools.resave(path.encode("utf-8"), path.encode("utf-8"), opts):
		# - failed to read input image
		# - failed to write output image
		raise Exception("resave failed")

# infile: path to input file
# outfile: path to output file (must be different from the input file)
# ops: list of lists containing drawimage() operations in the form (dx, dy, sx, sy, w, h)
# width/height: size of the output image (default: same as input)
def scramble(infile, outfile, ops, width = -1, height = -1, print_error = False):
	try:
		jc = JPEGCanvas(outfile, width, height)
		ixd = -1
		if isinstance(infile, str):
			idx = jc.add_image(infile)
		elif isinstance(infile, bytes) or isinstance(infile, bytearray):
			idx = jc.add_image_from_memory(infile, len(infile))
		else:
			raise Exception("invalid input file")
		for (dx, dy, sx, sy, w, h) in ops:
			jc.drawimage(idx, dx, dy, sx, sy, w, h)
			# PIL equivalent: dst.paste(src.crop((sx, sy, sx+w, sy+h)), (dx, dy))
		# if the image doesn't need decoding, then copy it to the output using paste() so saving works
		if len(decode) == 0:
			jc.just_paste(idx)
		jc.save()
		# optimize the output image
		resave_optimized(outfile)
		return True
	except Exception as e:
		if print_error:
			traceback.print_exc(file=sys.stderr)
		# the output image is created in the JPEGCanvas constructor and isn't deleted on error, so clean it up here
		try:
			os.remove(outfile)
		except Exception:
			pass
		return False

if __name__ == "__main__":
	# do the example image in the jpeg-tools repo
	decode = json.load(open("data/decode.json"))
	#if not scramble("data/enc.jpg", "out.jpg", decode, 700, 394, True):
	if not scramble(open("data/enc.jpg", "rb").read(), "out.jpg", decode, 700, 394, True):
		print("something happened :(")
		sys.exit(1)
	print("done")
