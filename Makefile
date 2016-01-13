CC = gcc
CFLAGS += -Wall
LDLIBS = -lwiringX -lconfuse

DESTDIR ?=
bindir = /usr/bin/
confdir = /etc/
systemdir = /usr/lib/systemd/system/

RM = rm -rf
MKDIR = mkdir -p
INSTALL = install -c -m755
INSTALLDATA = install -c -m644

SOURCE = src/wittypid.c
TARGET = wittypid

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(SOURCE) $(CFLAGS) -o $(TARGET) $(LDFLAGS) $(LDLIBS)

install: all
	$(MKDIR) $(DESTDIR)$(bindir)
	$(INSTALL) $(TARGET) $(DESTDIR)$(bindir)
	$(MKDIR) $(DESTDIR)$(confdir)
	$(INSTALLDATA) wittypid.conf $(DESTDIR)$(confdir)

install_all: install
	$(MKDIR) $(DESTDIR)$(systemdir)
	$(INSTALLDATA) systemd/wittypid.service $(DESTDIR)$(systemdir)

clean:
	$(RM) $(TARGET)

help:
	@echo "Targets:"
	@echo "  all:                   Build wittypid"
	@echo "  clean:                 Clean all generated files"
	@echo "  install:               Install necessary files (without wittypid.service)"
	@echo "  install_all:           Install all necessary files"
