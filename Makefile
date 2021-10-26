CC := clang
CFLAGS ?= -O2 -g3

OSNAME := $(shell uname -o)

LIBJPEG_PKG    ?= libjpeg
LIBJPEG_CFLAGS ?= $(shell pkg-config --cflags $(LIBJPEG_PKG))
LIBJPEG_LIBS   ?= $(shell pkg-config --libs   $(LIBJPEG_PKG))
CFLAGS += $(LIBJPEG_CFLAGS)
LDLIBS += $(LIBJPEG_LIBS)

CFLAGS += -std=gnu90
CFLAGS += -D_GNU_SOURCE

EXEEXT :=
DLLEXT := .so
ifneq (,$(findstring Msys,$(OSNAME)))
 EXEEXT := .exe
 DLLEXT := .dll
endif

ifeq (,$(findstring Msys,$(OSNAME)))
 CFLAGS += -fPIC
 CPPFLAGS += -D_FORTIFY_SOURCE=3
 CFLAGS += -fstack-protector-strong
endif

CFLAGS += -ffunction-sections
CFLAGS += -fdata-sections

CFLAGS += \
	-Wall \
	-Wdeclaration-after-statement \
	-Wno-gcc-compat \
	-Wno-bool-operation \
	-Werror=format \
	-Werror=implicit-function-declaration \
	-Werror=incompatible-pointer-types \
	-Werror=int-conversion \
	-Werror=return-type \
	-Werror=uninitialized \

ifeq ($(PGO),1)
 CFLAGS  += -fprofile-generate
 LDFLAGS += -fprofile-generate
else ifeq ($(PGO),2)
 $(shell llvm-profdata merge -output=default.profdata $$(ls default_*.profraw ../booksaver/default_*.profraw 2>/dev/null))
 CFLAGS  += -fprofile-use
 LDFLAGS += -fprofile-use
endif

ifneq ($(D),)
 CPPFLAGS += -DWITH_D=1
endif

# ---

default: jpegtools$(DLLEXT)

all: jcanvas$(DLLEXT) jpegtools$(DLLEXT) scramble$(EXEEXT) jsort$(EXEEXT)

ifeq (,$(findstring Msys,$(OSNAME)))
 all: isgrayscale$(EXEEXT) jresave$(EXEEXT)
endif

# ---

isgrayscale.o: isgrayscale.c isgrayscale.h
jcanvas.o: jcanvas.c jcanvas.h
jinfo.o: jinfo.c jinfo.h
scramble.o: scramble.c jcanvas.h

# ---

scramble$(EXEEXT): LDLIBS += -ljansson
scramble$(EXEEXT): jcanvas.o scramble.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

isgrayscale$(EXEEXT): isgrayscale.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

jresave$(EXEEXT): jresave.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

jsort$(EXEEXT): jsort.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# ---

jcanvas$(DLLEXT): jcanvas.o
	$(CC) -shared $(LDFLAGS) $^ -o $@ $(LDLIBS)

jpegtools$(DLLEXT): jcanvas.o isgrayscale.o jresave.o jinfo.o
	$(CC) -shared $(LDFLAGS) $^ -o $@ $(LDLIBS)

# ---

.c.o:
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

# ---

clean:
	@rm -fv -- *.o *.so *.profdata *.profraw scramble$(EXEEXT) isgrayscale$(EXEEXT) jresave$(EXEEXT) jsort$(EXEEXT) jcanvas$(DLLEXT)

watch:
	ls jcanvas.[ch] isgrayscale.[ch] jresave.[ch] jinfo.[ch] scramble.c | entr -c make

test:
	luajit test.lua
autotest:
	ls jcanvas$(DLLEXT) test.lua | entr -cr make test
