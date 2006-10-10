CC = gcc
LD = ld
#CFLAGS = -O3
CFLAGS = -O0 -g
override CFLAGS += -Wall -W -Werror -std=gnu99 -Wmissing-prototypes -Wwrite-strings -Wcast-qual -Wfloat-equal -Wshadow -Wpointer-arith -Wbad-function-cast -Wsign-compare -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute -Wredundant-decls -Wnested-externs -Winline -Wdisabled-optimization -Wno-long-long -Wstrict-prototypes -Wundef
INCLUDES = -I/usr/include/cm4all/libfox-0

SOURCES = src/main.c src/config.c src/daemon.c src/poll.c src/queue.c src/plan.c src/operator.c src/syslog.c src/strarray.c src/strhash.c src/pgutil.c
HEADERS = src/workshop.h src/syslog.h src/strarray.h src/strhash.h
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))
LIBS = -lcm4all-fox -lpq

all: src/cm4all-workshop

clean:
	rm -f src/*.o src/cm4all-workshop

check: t/test-pg_decode_array
	./t/test-pg_decode_array

t/test-pg_decode_array: t/test-pg_decode_array.o src/pgutil.o src/strarray.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

src/cm4all-workshop: $(OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

$(OBJECTS): %.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS) $(INCLUDES) $(INCLUDES)
