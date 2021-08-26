#!/usr/bin/env python3

import json
import os
import sys

from PIL import Image, ImageOps

rflag = False
width = -1
height = -1

while len(sys.argv) > 1:
	n = 1
	if sys.argv[1] == "-r":
		rflag = True
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
	print("usage: scranble.py <infile> <outfile>", file=sys.stderr)
	sys.exit(1)

img0 = None
try:
	img0 = Image.open(sys.argv[1])
except Exception as e:
	print("failed to open input file:", e, file=sys.stderr)
	sys.exit(1)

if width == -1:
	width = img0.width;
if height == -1:
	height = img0.height;

out = None
if (width, height) != (img0.width, img0.height):
	out = ImageOps.pad(img0, (width, height), method=Image.NEAREST, color=(0, 0, 0), centering=(0, 0))
else:
	out = img0.copy()

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
		out.paste(img0.crop((srcX, srcY, srcX+blkwidth, srcY+blkheight)), (destX, destY))
except Exception as e:
	print("error applying transforms:", e, file=sys.stderr)
	if lastop is not None:
		print("the last operation was:", lastop, file=sys.stderr)
	sys.exit(1)

if len(data) == 0:
	out.paste(img0, (0, 0))

try:
	ext = os.path.splitext(sys.argv[2])[1].lower()
	if ext in (".jpg", ".jpeg"):
		out.save(sys.argv[2], quality=90, subsampling="4:4:4", optimize=True)
	elif ext in (".png"):
		out.save(sys.argv[2], compress_level=1)
	else:
		out.save(sys.argv[2])
except Exception as e:
	print("failed to save file:", e, file=sys.stderr)
	sys.exit(1)
