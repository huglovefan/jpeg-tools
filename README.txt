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
  $ pacman -S git make pkg-config mingw-w64-x86_64-clang mingw-w64-x86_64-libjpeg-turbo
  $ git clone https://github.com/huglovefan/jpeg-tools
  $ cd jpeg-tools/
  $ git checkout extra
  $ make
  $ cp -t. /mingw64/bin/libjpeg-8.dll /mingw64/bin/libwinpthread-1.dll

there should now be a file named jpegtools.dll. test if it works by running "python jpegtools.py" using python for windows (outside msys). it should print "done" and create a file named out.jpg on success

the system DLLs copied in the howto need to be present in the current directory when using jpegtools outside an msys shell
