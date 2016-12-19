ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign 1.11 subdir-objects

AM_CPPFLAGS += -I$(srcdir)/src \
	$(BOOST_CPPFLAGS)

bin_PROGRAMS = cm4all-workshop

cm4all_workshop_SOURCES = \
	src/main.cxx \
	src/debug.h \
	src/Config.cxx src/Config.hxx \
	src/CommandLine.cxx src/CommandLine.hxx \
	src/AllocatorPtr.cxx src/AllocatorPtr.hxx \
	src/ExpandableStringList.cxx src/ExpandableStringList.hxx \
	src/SyslogClient.cxx src/SyslogClient.hxx \
	src/Expand.cxx src/Expand.hxx \
	src/Instance.cxx src/Instance.hxx \
	src/cron/Config.cxx src/cron/Config.hxx \
	src/cron/Schedule.cxx src/cron/Schedule.hxx \
	src/cron/Queue.cxx src/cron/Queue.hxx \
	src/cron/Job.hxx \
	src/cron/Workplace.cxx src/cron/Workplace.hxx \
	src/cron/Operator.cxx src/cron/Operator.hxx \
	src/cron/SpawnOperator.cxx src/cron/SpawnOperator.hxx \
	src/cron/CalculateNextRun.cxx src/cron/CalculateNextRun.hxx \
	src/cron/Partition.cxx src/cron/Partition.hxx \
	src/workshop/Config.cxx src/workshop/Config.hxx \
	src/workshop/Partition.cxx src/workshop/Partition.hxx \
	src/workshop/Queue.cxx src/workshop/Queue.hxx \
	src/workshop/PGQueue.cxx src/workshop/PGQueue.hxx \
	src/workshop/Job.cxx src/workshop/workshop/Job.hxx \
	src/workshop/Library.cxx src/workshop/Library.hxx \
	src/workshop/PlanLoader.cxx src/workshop/PlanLoader.hxx \
	src/workshop/PlanLibrary.cxx src/workshop/PlanUpdate.cxx \
	src/workshop/Plan.hxx \
	src/workshop/MultiLibrary.cxx src/workshop/MultiLibrary.hxx \
	src/workshop/Operator.cxx src/workshop/Operator.hxx \
	src/workshop/Workplace.cxx src/workshop/Workplace.hxx
cm4all_workshop_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS) \
	$(LIBCM4ALL_DAEMON_CFLAGS) \
	$(LIBCM4ALL_SOCKET_CFLAGS) \
	$(LIBSYSTEMD_CFLAGS) \
	$(LIBEVENT_CFLAGS) \
	$(CURL_CFLAGS) \
	$(LIBPQ_CPPFLAGS)
cm4all_workshop_LDADD = $(AM_LDADD) \
	libpg.a libspawn.a \
	libtranslation.a \
	libnet.a \
	libcurl.a \
	libevent.a \
	libio.a \
	libsystem.a libtime.a libutil.a \
	$(LIBCM4ALL_DAEMON_LIBS) \
	$(LIBCM4ALL_SOCKET_LIBS) \
	$(LIBSYSTEMD_LIBS) \
	$(LIBEVENT_LIBS) \
	$(CURL_LIBS) \
	$(BOOST_FILESYSTEM_LIB) \
	$(BOOST_SYSTEM_LIB) \
	$(LIBPQ_LIBS)

noinst_LIBRARIES = \
	libutil.a \
	libtime.a \
	libsystem.a \
	libnet.a \
	libevent.a \
	libio.a \
	libcurl.a \
	libtranslation.a \
	libpg.a \
	libspawn.a

libtime_a_SOURCES = \
	src/time/ISO8601.cxx src/time/ISO8601.hxx \
	src/time/Convert.cxx src/time/Convert.hxx \
	src/time/Math.cxx src/time/Math.hxx \
	src/time/Calendar.hxx
libtime_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS)

libutil_a_SOURCES = \
	src/util/BindMethod.hxx \
	src/util/CharUtil.hxx \
	src/util/ConstBuffer.hxx \
	src/util/DeleteDisposer.hxx \
	src/util/djbhash.c src/util/djbhash.h \
	src/util/PrintException.cxx src/util/PrintException.hxx \
	src/util/RangeBitSet.hxx \
	src/util/RuntimeError.hxx \
	src/util/TrivialArray.hxx \
	src/util/ShallowCopy.hxx \
	src/util/StaticArray.hxx \
	src/util/StaticFifoBuffer.hxx \
	src/util/StringUtil.cxx src/util/StringUtil.hxx \
	src/util/StringView.cxx src/util/StringView.hxx \
	src/util/StringParser.cxx src/util/StringParser.hxx \
	src/util/Tokenizer.cxx src/util/Tokenizer.hxx \
	src/util/WritableBuffer.hxx \
	src/util/ScopeExit.hxx
libutil_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS)

libio_a_SOURCES = \
	src/io/FileDescriptor.cxx src/io/FileDescriptor.hxx \
	src/io/UniqueFileDescriptor.hxx \
	src/io/LineParser.cxx src/io/LineParser.hxx \
	src/io/ConfigParser.cxx src/io/ConfigParser.hxx
libio_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS)

libevent_a_SOURCES = \
	src/event/Loop.cxx src/event/Loop.hxx \
	src/event/ShutdownListener.cxx src/event/ShutdownListener.hxx \
	src/event/Callback.hxx \
	src/event/Duration.hxx \
	src/event/Event.hxx \
	src/event/DeferEvent.cxx src/event/DeferEvent.hxx \
	src/event/SignalEvent.hxx src/event/SocketEvent.hxx \
	src/event/TimerEvent.hxx
libevent_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_DAEMON_CFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS) \
	$(LIBEVENT_CFLAGS)

libtranslation_a_SOURCES = \
	src/translation/CronClient.cxx src/translation/CronClient.hxx \
	src/translation/CronGlue.cxx src/translation/CronGlue.hxx \
	src/translation/Features.hxx \
	src/translation/PReader.cxx src/translation/PReader.hxx \
	src/translation/Parser.cxx src/translation/Parser.hxx \
	src/translation/Handler.hxx \
	src/translation/Request.hxx \
	src/translation/Response.cxx src/translation/Response.hxx
libtranslation_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_BENG_PROXY_CFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS)

libcurl_a_SOURCES = \
	src/curl/Easy.hxx \
	src/curl/Multi.hxx \
	src/curl/Request.cxx src/curl/Request.hxx \
	src/curl/Global.cxx src/curl/Global.hxx \
	src/curl/Init.cxx src/curl/Init.hxx
libcurl_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS) \
	$(CURL_CFLAGS)

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

libspawn_a_SOURCES = \
	src/spawn/Builder.hxx \
	src/spawn/CgroupOptions.cxx src/spawn/CgroupOptions.hxx \
	src/spawn/CgroupState.hxx \
	src/spawn/ChildOptions.cxx src/spawn/ChildOptions.hxx \
	src/spawn/Client.cxx src/spawn/Client.hxx \
	src/spawn/Config.hxx \
	src/spawn/Direct.cxx src/spawn/Direct.hxx \
	src/spawn/ExitListener.hxx \
	src/spawn/Glue.cxx src/spawn/Glue.hxx \
	src/spawn/Interface.cxx src/spawn/Interface.hxx \
	src/spawn/Launch.cxx src/spawn/Launch.hxx \
	src/spawn/Local.cxx src/spawn/Local.hxx \
	src/spawn/mount_list.cxx src/spawn/mount_list.hxx \
	src/spawn/NamespaceOptions.cxx src/spawn/NamespaceOptions.hxx \
	src/spawn/Parser.hxx \
	src/spawn/Prepared.cxx src/spawn/Prepared.hxx \
	src/spawn/Protocol.hxx \
	src/spawn/RefenceOptions.cxx src/spawn/RefenceOptions.hxx \
	src/spawn/Registry.cxx src/spawn/Registry.hxx \
	src/spawn/ResourceLimits.cxx src/spawn/ResourceLimits.hxx \
	src/spawn/Server.cxx src/spawn/Server.hxx \
	src/spawn/UidGid.cxx src/spawn/UidGid.hxx
libspawn_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS) \
	$(LIBCM4ALL_DAEMON_CFLAGS) \
	$(LIBSYSTEMD_CFLAGS)

libsystem_a_SOURCES = \
	src/system/Error.hxx \
	src/system/SetupProcess.cxx src/system/SetupProcess.hxx \
	src/system/bind_mount.c src/system/bind_mount.h \
	src/system/pivot_root.h \
	src/system/sigutil.h

libnet_a_SOURCES = \
	src/net/SocketAddress.cxx src/net/SocketAddress.hxx \
	src/net/AllocatedSocketAddress.cxx src/net/AllocatedSocketAddress.hxx \
	src/net/Features.hxx
libnet_a_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS)

#
# Debug programs
#

noinst_PROGRAMS = t/http

t_http_SOURCES = \
	t/http.cxx
t_http_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS) \
	$(LIBEVENT_CFLAGS) \
	$(CURL_CFLAGS)
t_http_LDADD = $(AM_LDADD) \
	libcurl.a \
	libevent.a \
	libutil.a \
	$(LIBEVENT_LIBS) \
	$(CURL_LIBS)

#
# Unit tests
#

TESTS = \
	t/test-pg_decode_array t/test-pg_encode_array \
	t/TestCronSchedule

check_PROGRAMS = \
	t/test-pg_decode_array t/test-pg_encode_array \
	t/TestCronSchedule

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

t_TestCronSchedule_SOURCES = \
	src/cron/Schedule.cxx src/cron/Schedule.hxx \
	t/TestCronSchedule.cxx
t_TestCronSchedule_CPPFLAGS = $(AM_CPPFLAGS) \
	$(LIBCM4ALL_INLINE_CFLAGS)
t_TestCronSchedule_LDADD = $(AM_LDADD) \
	libtime.a libutil.a

#
# documentation
#

clean-local:
	rm -rf doc/_build

doc/_build/html/.buildinfo: doc/*.rst
	$(MAKE) -C doc html

if DOCUMENTATION
all-local: doc/_build/html/.buildinfo
endif
