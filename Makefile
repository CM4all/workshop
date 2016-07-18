DEBUG ?= y
NOOPT ?= $(DEBUG)

CC = clang
CXX = clang++
LD = ld

CXXFLAGS = -g

ifeq ($(NOOPT),y)
CXXFLAGS += -O0
else
CXXFLAGS += -O2 -ffunction-sections
endif

ifneq ($(DEBUG),y)
CXXFLAGS += -DNDEBUG
endif

override CXXFLAGS += -Wall -W -Werror -std=gnu++0x -Wwrite-strings -Wcast-qual -Wfloat-equal -Wshadow -Wpointer-arith -Wsign-compare -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute -Wredundant-decls -Winline -Wdisabled-optimization -Wno-long-long -Wundef

INCLUDES = -Isrc

LIBINLINE_CFLAGS := $(shell pkg-config --cflags libcm4all-inline)

LIBDAEMON_CFLAGS := $(shell pkg-config --cflags libcm4all-daemon)
LIBDAEMON_LIBS := $(shell pkg-config --libs libcm4all-daemon)

INCLUDES += $(LIBINLINE_CFLAGS) $(LIBDAEMON_CFLAGS)

CXX_SOURCES = src/main.cxx \
	src/CommandLine.cxx \
	src/SyslogClient.cxx \
	src/util/StringUtil.cxx \
	src/util/Tokenizer.cxx \
	src/util/Error.cxx \
	src/io/TextFile.cxx \
	src/Instance.cxx \
	src/event/Loop.cxx \
	src/event/DeferEvent.cxx \
	src/Queue.cxx src/PGQueue.cxx src/Job.cxx \
	src/pg/Connection.cxx src/pg/Result.cxx \
	src/pg/AsyncConnection.cxx \
	src/pg/Array.cxx \
	src/Library.cxx \
	src/PlanLoader.cxx src/PlanLibrary.cxx src/PlanUpdate.cxx \
	src/Operator.cxx src/Workplace.cxx

CXX_OBJECTS = $(patsubst %.cxx,%.o,$(CXX_SOURCES))
LIBS = -lstdc++ -levent -lpq $(LIBDAEMON_LIBS)
LDFLAGS = -Wl,-gc-sections

all: src/cm4all-workshop doc/workshop.html

doc/workshop.html: doc/workshop.xml
	xsltproc -o $@ /usr/share/sgml/docbook/stylesheet/xsl/nwalsh/xhtml/docbook.xsl $<

clean:
	rm -f src/*.o t/*.o src/cm4all-workshop doc/workshop.html

check: t/test-pg_decode_array t/test-pg_encode_array
	./t/test-pg_decode_array
	./t/test-pg_encode_array

t/test-pg_decode_array.o t/test-pg_encode_array.o: %.o: %.cxx
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(INCLUDES)

t/test-pg_decode_array: t/test-pg_decode_array.o src/pg/Array.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

t/test-pg_encode_array: t/test-pg_encode_array.o src/pg/Array.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

src/cm4all-workshop: $(CXX_OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

$(CXX_OBJECTS): %.o: %.cxx $(wildcard src/*.h) $(wildcard src/*.hxx) $(wildcard src/pg/*.hxx)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(INCLUDES)
