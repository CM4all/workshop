CC = gcc
LD = ld
#CFLAGS = -O3
CFLAGS = -O0 -g
override CFLAGS += -Wall -W -Werror -std=gnu99 -Wmissing-prototypes -Wwrite-strings -Wcast-qual -Wfloat-equal -Wshadow -Wpointer-arith -Wbad-function-cast -Wsign-compare -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute -Wredundant-decls -Wnested-externs -Winline -Wdisabled-optimization -Wno-long-long -Wstrict-prototypes -Wundef
INCLUDES =

LIBDAEMON_CFLAGS := $(shell pkg-config --cflags libcm4all-daemon)
LIBDAEMON_LIBS := $(shell pkg-config --libs libcm4all-daemon)

INCLUDES += $(LIBDAEMON_CFLAGS)

SOURCES = src/main.c src/config.c \
	src/syslog.c \
	src/queue.c src/pg-queue.c \
	src/plan.c src/plan-loader.c src/plan-library.c src/plan-update.c \
	src/operator.c \
	src/pg-util.c \
	src/strarray.c src/strhash.c

HEADERS = src/workshop.h src/version.h src/syslog.h src/strarray.h src/strhash.h src/pg-util.h src/pg-queue.h
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))
LIBS = -levent -lpq $(LIBDAEMON_LIBS)

all: src/cm4all-workshop doc/workshop.html

doc/workshop.html: doc/workshop.xml
	xsltproc -o $@ /usr/share/sgml/docbook/stylesheet/xsl/nwalsh/xhtml/docbook.xsl $<

clean:
	rm -f src/*.o src/cm4all-workshop doc/workshop.html

check: t/test-pg_decode_array t/test-pg_encode_array
	./t/test-pg_decode_array
	./t/test-pg_encode_array

t/test-pg_decode_array: t/test-pg_decode_array.o src/pg-util.o src/strarray.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

t/test-pg_encode_array: t/test-pg_encode_array.o src/pg-util.o src/strarray.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

src/cm4all-workshop: $(OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

$(OBJECTS): %.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS) $(INCLUDES) $(INCLUDES)
