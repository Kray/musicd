SRCS =  src/cache.c \
	src/client.c \
	src/config.c \
	src/cue.c \
	src/db.c \
	src/format.c \
	src/image.c \
	src/json.c \
	src/libav.c \
	src/library.c \
	src/log.c \
	src/lyrics.c \
	src/musicd.c \
	src/query.c \
	src/scan.c \
	src/server.c \
	src/stream.c \
	src/strings.c \
	src/protocol_http.c \
	src/protocol_musicd.c \
	src/protocol.c \
	src/task.c \
	src/track.c \
	src/transcoder.c \
	src/url.c

DEPS = $(SRCS)

CFLAGS += -g -Wall -Wextra

LIBS = -lpthread -lm -lavutil -lavcodec -lavformat -lsqlite3 -lfreeimage -lcurl


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
