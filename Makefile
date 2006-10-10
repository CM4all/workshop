CC = gcc
LD = ld
#CFLAGS = -O3
CFLAGS = -O0 -g
override CFLAGS += -Wall -W -Werror -std=gnu99 -Wmissing-prototypes -Wwrite-strings -Wcast-qual -Wfloat-equal -Wshadow -Wpointer-arith -Wbad-function-cast -Wsign-compare -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute -Wredundant-decls -Wnested-externs -Winline -Wdisabled-optimization -Wno-long-long -Wstrict-prototypes -Wundef
INCLUDES = -I/usr/include/cm4all/libfox-0

SOURCES = src/main.c src/config.c src/daemon.c src/poll.c src/queue.c src/plan.c src/operator.c src/syslog.c
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))
LIBS = -lcm4all-fox -lpq

all: src/cm4all-workshop

clean:
	rm -f src/*.o src/cm4all-workshop

src/cm4all-workshop: $(OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

$(OBJECTS): %.o: %.c src/workshop.h src/syslog.h
	$(CC) -c -o $@ $< $(CFLAGS) $(INCLUDES) $(INCLUDES)
