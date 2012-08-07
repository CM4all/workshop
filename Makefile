DEBUG ?= y
NOOPT ?= $(DEBUG)

CC = clang
CXX = clang++
LD = ld

CFLAGS = -g
CXXFLAGS = -g

ifeq ($(NOOPT),y)
CFLAGS += -O0
CXXFLAGS += -O0
else
CFLAGS += -O2 -ffunction-sections
CXXFLAGS += -O2 -ffunction-sections
endif

ifneq ($(DEBUG),y)
CFLAGS += -DNDEBUG
endif

override CFLAGS += -Wall -W -Werror -std=gnu99 -Wmissing-prototypes -Wwrite-strings -Wcast-qual -Wfloat-equal -Wshadow -Wpointer-arith -Wbad-function-cast -Wsign-compare -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute -Wredundant-decls -Wnested-externs -Winline -Wdisabled-optimization -Wno-long-long -Wstrict-prototypes -Wundef

override CXXFLAGS += -Wall -W -Werror -std=gnu++0x -Wmissing-prototypes -Wwrite-strings -Wcast-qual -Wfloat-equal -Wshadow -Wpointer-arith -Wbad-function-cast -Wsign-compare -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute -Wredundant-decls -Wnested-externs -Winline -Wdisabled-optimization -Wno-long-long -Wstrict-prototypes -Wundef

INCLUDES =

LIBINLINE_CFLAGS := $(shell pkg-config --cflags libcm4all-inline)

LIBDAEMON_CFLAGS := $(shell pkg-config --cflags libcm4all-daemon)
LIBDAEMON_LIBS := $(shell pkg-config --libs libcm4all-daemon)

INCLUDES += $(LIBINLINE_CFLAGS) $(LIBDAEMON_CFLAGS)

C_SOURCES = src/cmdline.c \
	src/syslog.c \
	src/pg-queue.c

CXX_SOURCES = src/main.cxx \
	src/Instance.cxx \
	src/Event.cxx \
	src/pg_array.cxx \
	src/Queue.cxx src/Job.cxx \
	src/Library.cxx \
	src/PlanLoader.cxx src/PlanLibrary.cxx src/PlanUpdate.cxx \
	src/Operator.cxx src/Workplace.cxx

C_OBJECTS = $(patsubst %.c,%.o,$(C_SOURCES))
CXX_OBJECTS = $(patsubst %.cxx,%.o,$(CXX_SOURCES))
LIBS = -lstdc++ -levent -lpq $(LIBDAEMON_LIBS)
LDFLAGS = -Wl,-gc-sections

all: src/cm4all-workshop doc/workshop.html

doc/workshop.html: doc/workshop.xml
	xsltproc -o $@ /usr/share/sgml/docbook/stylesheet/xsl/nwalsh/xhtml/docbook.xsl $<

clean:
	rm -f src/*.o src/cm4all-workshop doc/workshop.html

check: t/test-pg_decode_array t/test-pg_encode_array
	./t/test-pg_decode_array
	./t/test-pg_encode_array

t/test-pg_decode_array.o t/test-pg_encode_array.o: %.o: %.cxx
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(INCLUDES)

t/test-pg_decode_array: t/test-pg_decode_array.o src/pg_array.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

t/test-pg_encode_array: t/test-pg_encode_array.o src/pg_array.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

src/cm4all-workshop: $(C_OBJECTS) $(CXX_OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

$(C_OBJECTS): %.o: %.c $(wildcard src/*.h)
	$(CC) -c -o $@ $< $(CFLAGS) $(INCLUDES) $(INCLUDES)

$(CXX_OBJECTS): %.o: %.cxx $(wildcard src/*.h) $(wildcard src/*.hxx)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(INCLUDES)
