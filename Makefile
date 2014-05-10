LUA_PKGCONF ?= lua

MODULE = lpc
VERSION = 1.0.0

ifneq ($(shell pkg-config $(LUA_PKGCONF) || echo not-installed),)
$(error $(LUA_PKGCONF).pc not found)
endif

INSTALL_PREFIX = $(DESTDIR)$(shell pkg-config $(LUA_PKGCONF) --variable=INSTALL_CMOD)

CC	= gcc
TARGET	= lpc.so
OBJS	= lpc.o
LIBS	=
CFLAGS	= $(shell pkg-config $(LUA_PKGCONF) --cflags) -fPIC
LDFLAGS	= $(shell pkg-config $(LUA_PKGCONF) --libs) -shared -fPIC

default: $(TARGET)


install: default
	install -d $(INSTALL_PREFIX)
	install $(TARGET) $(INSTALL_PREFIX)

clean:
	rm -rf $(OBJS) $(TARGET) $(MODULE)-$(VERSION)

package: clean
	mkdir $(MODULE)-$(VERSION)
	cp lpc.c COPYING Makefile $(MODULE)-$(VERSION)
	tar cvzf $(MODULE)-$(VERSION).tar.gz $(MODULE)-$(VERSION)
	rm -rf $(MODULE)-$(VERSION)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
