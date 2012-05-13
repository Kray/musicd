SRCS =  src/client.c \
	src/config.c \
	src/cue.c \
	src/db.c \
	src/format.c \
	src/libav.c \
	src/library.c \
	src/log.c \
	src/musicd.c \
	src/scan.c \
	src/server.c \
	src/stream.c \
	src/strings.c \
	src/track.c \
	src/transcoder.c

DEPS = $(SRCS)

CFLAGS += -g -Wall -Wextra

LIBS = -lpthread -lm -lavutil -lavcodec -lavformat -lsqlite3 -lfreeimage

PREFIX ?= /usr/local

musicd: $(DEPS)
	$(CC) $(CFLAGS) $(SRCS) -o musicd $(LIBS)

install: musicd
	install -d $(PREFIX)/bin/
	install -m 0775 musicd $(PREFIX)/bin/
	install -d $(PREFIX)/share/doc/musicd/
	install -m 644 doc/* $(PREFIX)/share/doc/musicd/
	install -d $(PREFIX)/share/man/man1/
	install doc/musicd.1 $(PREFIX)/share/man/man1/
