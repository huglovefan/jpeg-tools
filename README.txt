isgrayscale.c	fastest way to determine if an image contains no color
jcanvas.c	lossless drawImage() for jpgs
jsort.c		mess up an image
resave.c	"jpegtran -optimize" as a library
scramble.c	example command-line tool using jcanvas
scranble.py	non-lossless clone of scramble.c using PIL
scranblw.py	scranble.py using jcanvas through python-cffi

igs_verify.sh	check that isgrayscale.c and imagemagick agree about a file
test.lua	tests for jcanvas.c (run using "make test")

system requirements:
- clang C compiler
- libjpeg-turbo
- scramble.c: libjansson
- scranble.py: python 3 + PIL module
- scranblw.py: python 3 + cffi module
- test.lua: luajit, imagemagick
- igs_verify.sh: imagemagick

---

msys2 howto:
- get it from msys2.org, make sure to launch it from the shortcut that says "MSYS2 MinGW 64-bit"
- run the following commands:
  $ pacman -S git make pkg-config mingw-w64-x86_64-clang mingw-w64-x86_64-libjpeg-turbo mingw-w64-x86_64-jansson
  $ git clone https://github.com/huglovefan/jpeg-tools
  $ cd jpeg-tools/
  $ git checkout extra
  $ make
  $ cp -t. /mingw64/bin/libjpeg-8.dll /mingw64/bin/libjansson-4.dll /mingw64/bin/libwinpthread-1.dll
- test if it works (should create out.jpg):
  $ cat data/decode.json | ./scramble.exe data/enc.jpg out.jpg
- same command in cmd.exe:
  > type data\decode.json | scramble.exe data\enc.jpg out.jpg

the python cffi example should work in python for windows (outside msys) using the dll that was just compiled
to test it, do one of the above commands but with "python scranblw.py" instead of scramble.exe

the system DLLs copied in the howto need to be in the same directory when using them outside an msys shell
