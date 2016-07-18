AUTOMAKE_OPTIONS = foreign 1.11 subdir-objects

AM_CPPFLAGS += -I$(srcdir)/src

bin_PROGRAMS = cm4all-workshop
cm4all_workshop_SOURCES = \
	src/main.cxx \
	src/debug.h \
	src/version.h \
	src/CommandLine.cxx src/CommandLine.hxx \
	src/SyslogClient.cxx src/SyslogClient.hxx \
	src/Instance.cxx src/Instance.hxx \
	src/Queue.cxx src/Queue.hxx \
	src/PGQueue.cxx src/PGQueue.hxx \
	src/Job.cxx src/Job.hxx \
	src/Library.cxx src/Library.hxx \
	src/PlanLoader.cxx src/PlanLibrary.cxx src/PlanUpdate.cxx \
	src/Plan.hxx \
	src/Operator.cxx src/Operator.hxx \
	src/Workplace.cxx src/Workplace.hxx
cm4all_workshop_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS) \
	$(LIBCM4ALL_DAEMON_CFLAGS) \
	$(LIBEVENT_CFLAGS) \
	$(LIBPQ_CPPFLAGS)
cm4all_workshop_LDADD = $(AM_LDADD) \
	libpg.a libevent.a libio.a libutil.a \
	$(LIBCM4ALL_DAEMON_LIBS) \
	$(LIBEVENT_LIBS) \
	$(LIBPQ_LIBS)

noinst_LIBRARIES = libutil.a libio.a libevent.a libpg.a

libutil_a_SOURCES = \
	src/util/BindMethod.hxx \
	src/util/CharUtil.hxx \
	src/util/ConstBuffer.hxx \
	src/util/Domain.hxx \
	src/util/Error.cxx src/util/Error.hxx \
	src/util/StringUtil.cxx src/util/StringUtil.hxx \
	src/util/Tokenizer.cxx src/util/Tokenizer.hxx \
	src/util/ScopeExit.hxx
libutil_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS)

libio_a_SOURCES = \
	src/io/FileDescriptor.cxx src/io/FileDescriptor.hxx \
	src/io/UniqueFileDescriptor.hxx \
	src/io/TextFile.cxx src/io/TextFile.hxx
libio_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS)

libevent_a_SOURCES = \
	src/event/Loop.cxx src/event/Loop.hxx \
	src/event/Callback.hxx \
	src/event/Duration.hxx \
	src/event/Event.hxx \
	src/event/DeferEvent.cxx src/event/DeferEvent.hxx \
	src/event/SignalEvent.hxx src/event/SocketEvent.hxx \
	src/event/TimerEvent.hxx
libevent_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS) \
	$(LIBEVENT_CFLAGS)

libpg_a_SOURCES = \
	src/pg/Array.cxx src/pg/Array.hxx \
	src/pg/BinaryValue.hxx \
	src/pg/Connection.cxx src/pg/Connection.hxx \
	src/pg/AsyncConnection.cxx src/pg/AsyncConnection.hxx \
	src/pg/Error.hxx \
	src/pg/Notify.hxx \
	src/pg/ParamWrapper.hxx \
	src/pg/DynamicParamWrapper.hxx \
	src/pg/Result.cxx src/pg/Result.hxx
libpg_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS) \
	$(LIBPQ_CPPFLAGS)

#
# Unit tests
#

TESTS = t/test-pg_decode_array t/test-pg_encode_array

check_PROGRAMS = t/test-pg_decode_array t/test-pg_encode_array

t_test_pg_decode_array_SOURCES = t/test-pg_decode_array.cxx
t_test_pg_decode_array_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS) \
	$(LIBPQ_CPPFLAGS)
t_test_pg_decode_array_LDADD = $(AM_LDADD) \
	libpg.a \
	$(LIBPQ_LIBS)

t_test_pg_encode_array_SOURCES = t/test-pg_encode_array.cxx
t_test_pg_encode_array_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS) \
	$(LIBPQ_CPPFLAGS)
t_test_pg_encode_array_LDADD = $(AM_LDADD) \
	libpg.a \
	$(LIBPQ_LIBS)

#
# documentation
#

MOSTLYCLEANFILES = doc/workshop.html

if DOCUMENTATION
doc/workshop.html: doc/workshop.xml
	xsltproc -o $@ /usr/share/sgml/docbook/stylesheet/xsl/nwalsh/xhtml/docbook.xsl $<

all-local: doc/workshop.html

doc_DATA = doc/workshop.html
endif
