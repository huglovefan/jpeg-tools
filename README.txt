isgrayscale.c	fastest way to determine if an image contains no color
jcanvas.c	lossless drawImage() for jpgs
jsort.c		mess up an image
resave.c	"jpegtran -optimize" as a library
scramble.c	example command-line tool using jcanvas
scranble.py	non-lossless clone of scramble.c using PIL

igs_verify.sh	check that isgrayscale.c and imagemagick agree about a file
test.lua	tests for jcanvas.c (run using "make test")

system requirements:
- clang C compiler
- libjpeg-turbo
- scramble.c: libjansson
- scranble.py: python 3, PIL
- test.lua: luajit, imagemagick
- igs_verify.sh: imagemagick
