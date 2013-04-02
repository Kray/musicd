
BUILDDIR ?= ./build
PREFIX ?= /usr/local

CFLAGS += -g -Wall -Wextra

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
	src/session.c \
	src/server.c \
	src/stream.c \
	src/strings.c \
	src/protocol_http.c \
	src/protocol_musicd.c \
	src/protocol.c \
	src/task.c \
	src/track.c \
	src/url.c


ifdef HTTP_BUILTIN
	CFLAGS += -DHTTP_BUILTIN
	SRCS += ${BUILDDIR}/http_builtin.c
endif

OBJS +=  $(SRCS:%.c=${BUILDDIR}/%.o)
DEPS += $(OBJS)

LIBS += -lpthread -lm -lavutil -lavcodec -lavformat -lavresample -lsqlite3 -lfreeimage -lcurl


all: musicd

musicd: ${BUILDDIR}/musicd

clean:
	rm -rf ${BUILDDIR}

${BUILDDIR}/musicd: $(DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) -o $@ $(LIBS)

${BUILDDIR}/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@


# Compile builtin HTTP file packer
${BUILDDIR}/http_builtin.c: ${BUILDDIR}/http_builtin_pack
	@mkdir -p $(dir $@)
	${BUILDDIR}/http_builtin_pack $(HTTP_BUILTIN) > ${BUILDDIR}/http_builtin.c

# Pack builtin HTTP files
${BUILDDIR}/http_builtin_pack: tools/http_builtin_pack.c
	@mkdir -p $(dir $@)
	$(CC) tools/http_builtin_pack.c -o ${BUILDDIR}/http_builtin_pack


install: musicd
	install -d $(PREFIX)/bin/
	install -m 0775 ${BUILDDIR}/musicd $(PREFIX)/bin/
	install -d $(PREFIX)/share/doc/musicd/
	install -m 644 doc/* $(PREFIX)/share/doc/musicd/
	install -d $(PREFIX)/share/man/man1/
	install doc/musicd.1 $(PREFIX)/share/man/man1/

