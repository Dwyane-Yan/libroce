PREFIX = /usr
VERSION = 0.1
LIB_NAME = libroce
SHARED_LIB = $(LIB_NAME).so
STATIC_LIB = $(LIB_NAME).a
CFLAGS = -fPIC -Wall -g -O2 -I include -D_GNU_SOURCE -Wshadow -Wformat=2 -Wwrite-strings -fstack-protector-strong -Wnull-dereference -Wunreachable-code
CC = gcc
AR = ar
FMT = clang-format-18
SPELLCHECK = codespell
OBJS = ah.o ctx.o cq.o crc.o dev.o netpkt.o pd.o port.o qp.o requester.o resource.o responder.o mr.o

ifneq ($(LLVM),)
CC = clang
AR = llvm-ar-18
else
CFLAGS += -fanalyzer -Wduplicated-branches -Wrestrict
endif

include pkg-config.mk

%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o
	@$(CC) -MM $(CFLAGS) -MF $*.d -MT $*.o $*.c


$(LIB_NAME): $(OBJS)
	$(CC) -shared $(CFLAGS) $(LIBS) $(OBJS) -o $(SHARED_LIB)
	$(AR) rcs $(STATIC_LIB) $(OBJS)

test: $(LIB_NAME)
	make -C test

valgrind: $(LIB_NAME)
	make -C test valgrind

format:
	$(FMT) -i *.c
	$(FMT) -i test/*.c
	$(FMT) -i include/*/*.h

spellcheck:
	git ls-files --cached --others --exclude-standard -- '*.c' '*.h' ':!include/private/list.h' | xargs $(SPELLCHECK)

clean:
	make -C test clean
	rm -f *.d *.o *.so *.a *.pc *.pc.in

pkgconfig:
	$(file > $(LIB_NAME).pc.in,$(PKG_DESCRIBE_CONFIG))
	$(file > $(LIB_NAME).pc,$(PKG_PATH_CONFIG))
	@cat $(LIB_NAME).pc.in >> $(LIB_NAME).pc
	@rm -f $(LIB_NAME).pc.in

rebuild: clean
	make -C . $(LIB_NAME)
	make -C . pkgconfig

install: $(LIB_NAME) pkgconfig
	mkdir -p $(PREFIX)/include/roce
	install include/public/roce.h $(PREFIX)/include/roce/roce.h
	install $(SHARED_LIB) $(PREFIX)/lib/`uname -m`-linux-gnu
	install $(STATIC_LIB) $(PREFIX)/lib/`uname -m`-linux-gnu
	install $(LIB_NAME).pc  $(PREFIX)/lib/`uname -m`-linux-gnu/pkgconfig
