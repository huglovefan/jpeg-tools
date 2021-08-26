CC := clang
CFLAGS ?= -O2 -g3 -march=native -mno-tbm -mno-xop

LIBJPEG_PKG    ?= libjpeg
LIBJPEG_CFLAGS ?= $(shell pkg-config --cflags $(LIBJPEG_PKG))
LIBJPEG_LIBS   ?= $(shell pkg-config --libs   $(LIBJPEG_PKG))
CFLAGS += $(LIBJPEG_CFLAGS)
LDLIBS += $(LIBJPEG_LIBS)

CFLAGS += -std=gnu90
CFLAGS += -fPIC

CPPFLAGS += -D_FORTIFY_SOURCE=3
CFLAGS += -fstack-protector-strong

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

all: jcanvas.so scramble isgrayscale jresave jsort

# ---

isgrayscale.o: isgrayscale.c isgrayscale.h
jcanvas.o: jcanvas.c jcanvas.h
scramble.o: scramble.c jcanvas.h

# ---

scramble: LDLIBS += -ljansson
scramble: jcanvas.o scramble.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

isgrayscale: isgrayscale.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

jresave: jresave.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

jsort: jsort.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# ---

jcanvas.so: jcanvas.o
	$(CC) -shared $(LDFLAGS) $^ -o $@ $(LDLIBS)

# ---

.c.o:
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

# ---

clean:
	@rm -fv -- *.o *.so *.profdata *.profraw scramble isgrayscale jresave

watch:
	ls jcanvas.[ch] isgrayscale.[ch] jresave.[ch] scramble.c | entr -c make

test:
	luajit test.lua
autotest:
	ls jcanvas.so test.lua | entr -cr make test
