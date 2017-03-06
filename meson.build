project('CM4all Workshop', ['c', 'cpp'], version: '2.0.8',
  default_options: [
    'c_std=c99',
    'cpp_std=c++14'
  ],
)

add_global_arguments('-D_REENTRANT', '-D_GNU_SOURCE',
  '-DPACKAGE="' + meson.project_name() + '"',
  '-DVERSION="' + meson.project_version() + '"',
  '-Wall',
  '-Wextra',
  '-Wmissing-prototypes', '-Wstrict-prototypes',
  '-Wwrite-strings', '-Wcast-qual', '-Wcast-align', '-Wfloat-equal',
  '-Wbad-function-cast',
  '-Wshadow', '-Wpointer-arith', '-Wsign-compare',
  '-Waggregate-return',
  '-Wmissing-declarations', '-Wmissing-noreturn', '-Wmissing-format-attribute',
  '-Wredundant-decls', '-Wno-long-long', '-Wundef',
  '-Wnested-externs',
  '-Wunused',
  '-Wundef',
  '-pedantic',
  '-Werror', '-pedantic-errors',
  language: 'c')

add_global_arguments('-D_REENTRANT', '-D_GNU_SOURCE',
  '-DPACKAGE="' + meson.project_name() + '"',
  '-DVERSION="' + meson.project_version() + '"',
  '-Wall',
  '-Wextra',
  '-Wwrite-strings', '-Wcast-qual', '-Wcast-align', '-Wfloat-equal',
  '-Wshadow', '-Wpointer-arith', '-Wsign-compare',
  '-Wmissing-declarations', '-Wmissing-noreturn', '-Wmissing-format-attribute',
  '-Wredundant-decls', '-Wno-long-long', '-Wundef',
  '-Wno-missing-field-initializers',
  '-Wno-non-virtual-dtor',
  '-Werror',
  language: 'cpp')

boost = dependency('boost', modules: ['system', 'filesystem'], version: '>= 1.54')
libevent = dependency('libevent', version: '>= 2.0.19')
libcurl = dependency('libcurl', version: '>= 7.38')
libsystemd = dependency('libsystemd')
libseccomp = dependency('libseccomp')
libpq = dependency('libpq', version: '>= 8.4')
libinline = dependency('libcm4all-inline', version: '>= 0.3.2')
libdaemon = dependency('libcm4all-daemon', version: '>= 0.4.2')
libsocket = dependency('libcm4all-socket', version: '>= 0.3.5')
libbp = dependency('libcm4all-beng-proxy', version: '>= 12.0.3')

inc = include_directories('src', 'libcommon/src')

util = static_library('util',
  'libcommon/src/util/AllocatedString.cxx',
  'libcommon/src/util/Exception.cxx',
  'libcommon/src/util/LeakDetector.cxx',
  'libcommon/src/util/PrintException.cxx',
  'libcommon/src/util/StringBuilder.cxx',
  'libcommon/src/util/StringParser.cxx',
  'libcommon/src/util/StringUtil.cxx',
  'libcommon/src/util/StringView.cxx',
  'src/util/djbhash.c',
  'src/util/Tokenizer.cxx',
  include_directories: inc,
  dependencies: [
    libinline
  ])

time = static_library('time',
  'src/time/ISO8601.cxx',
  'src/time/Convert.cxx',
  'src/time/Math.cxx',
  include_directories: inc,
  dependencies: [
    libinline,
  ])

system = static_library('system',
  'src/system/SetupProcess.cxx',
  'src/system/bind_mount.c',
  include_directories: inc,
  dependencies: [
    libinline,
  ])

io = static_library('io',
  'src/io/FileDescriptor.cxx',
  'src/io/LineParser.cxx',
  'src/io/ConfigParser.cxx',
  include_directories: inc,
  dependencies: [
    boost,
    libinline,
    declare_dependency(link_with: util),
  ])

event = static_library('event',
  'libcommon/src/event/Loop.cxx',
  'libcommon/src/event/ShutdownListener.cxx',
  'libcommon/src/event/CleanupTimer.cxx',
  'libcommon/src/event/DeferEvent.cxx',
  'libcommon/src/event/SignalEvent.cxx',
  include_directories: inc,
  dependencies: [
    libinline,
    libevent,
    declare_dependency(link_with: util),
  ])

net = static_library('net',
  'libcommon/src/net/SocketAddress.cxx',
  'libcommon/src/net/AllocatedSocketAddress.cxx',
  include_directories: inc,
  dependencies: [
    libinline, libdaemon, libsocket,
    declare_dependency(link_with: event),
  ])

curl = static_library('curl',
  'src/curl/Version.cxx',
  'src/curl/Request.cxx',
  'src/curl/Global.cxx',
  'src/curl/Init.cxx',
  include_directories: inc,
  dependencies: [
    libinline,
    libcurl,
    declare_dependency(link_with: event),
  ])

pg = static_library('pg',
  'src/pg/Array.cxx',
  'src/pg/Connection.cxx',
  'src/pg/AsyncConnection.cxx',
  'src/pg/Result.cxx',
  include_directories: inc,
  dependencies: [
    libpq,
    libinline,
  ])

translation = static_library('translation',
  'src/translation/CronClient.cxx',
  'src/translation/CronGlue.cxx',
  'src/translation/PReader.cxx',
  'src/translation/Parser.cxx',
  'src/translation/Response.cxx',
  include_directories: inc,
  dependencies: [
    libinline, libbp,
    declare_dependency(link_with: event),
    declare_dependency(link_with: net),
  ])

spawn = static_library('spawn',
  'src/spawn/CgroupOptions.cxx',
  'src/spawn/ChildOptions.cxx',
  'src/spawn/Client.cxx',
  'src/spawn/SeccompFilter.cxx',
  'src/spawn/Direct.cxx',
  'src/spawn/Glue.cxx',
  'src/spawn/Interface.cxx',
  'src/spawn/Launch.cxx',
  'src/spawn/Local.cxx',
  'src/spawn/mount_list.cxx',
  'src/spawn/NamespaceOptions.cxx',
  'src/spawn/Prepared.cxx',
  'src/spawn/RefenceOptions.cxx',
  'src/spawn/Registry.cxx',
  'src/spawn/ResourceLimits.cxx',
  'src/spawn/Server.cxx',
  'src/spawn/UidGid.cxx',
  include_directories: inc,
  dependencies: [
    libseccomp,
    libinline, libdaemon,
    declare_dependency(link_with: util),
    declare_dependency(link_with: system),
  ])

executable('cm4all-workshop',
  'src/main.cxx',
  'src/Config.cxx',
  'src/CommandLine.cxx',
  'src/AllocatorPtr.cxx',
  'src/ExpandableStringList.cxx',
  'src/SyslogClient.cxx',
  'src/CaptureBuffer.cxx',
  'src/PipeCaptureBuffer.cxx',
  'src/Expand.cxx',
  'src/Instance.cxx',
  'src/cron/Config.cxx',
  'src/cron/Schedule.cxx',
  'src/cron/Queue.cxx',
  'src/cron/Workplace.cxx',
  'src/cron/Operator.cxx',
  'src/cron/SpawnOperator.cxx',
  'src/cron/CurlOperator.cxx',
  'src/cron/CalculateNextRun.cxx',
  'src/cron/Partition.cxx',
  'src/workshop/Config.cxx',
  'src/workshop/Partition.cxx',
  'src/workshop/Queue.cxx',
  'src/workshop/PGQueue.cxx',
  'src/workshop/Job.cxx',
  'src/workshop/Library.cxx',
  'src/workshop/PlanLoader.cxx',
  'src/workshop/PlanLibrary.cxx',
  'src/workshop/PlanUpdate.cxx',
  'src/workshop/MultiLibrary.cxx',
  'src/workshop/Operator.cxx',
  'src/workshop/Workplace.cxx',
  include_directories: inc,
  dependencies: [
    boost,
    libsystemd,
    libinline, libdaemon, libsocket,
    declare_dependency(link_with: time),
    declare_dependency(link_with: system),
    declare_dependency(link_with: io),
    declare_dependency(link_with: translation),
    declare_dependency(link_with: spawn),
    declare_dependency(link_with: pg),
    declare_dependency(link_with: curl),
  ])

executable('http',
  't/http.cxx',
  install: false,
  include_directories: inc,
  dependencies: [
    libinline,
    declare_dependency(link_with: curl),
  ])

test_pg_decode_array = executable('test_pg_decode_array',
  't/test-pg_decode_array.cxx',
  install: false,
  dependencies: [
    declare_dependency(link_with: pg),
  ])

test_pg_encode_array = executable('test_pg_encode_array',
  't/test-pg_encode_array.cxx',
  install: false,
  dependencies: [
    declare_dependency(link_with: pg),
  ])

TestCronSchedule = executable('TestCronSchedule',
  't/TestCronSchedule.cxx',
  'src/cron/Schedule.cxx',
  include_directories: inc,
  install: false,
  dependencies: [
    libinline,
    declare_dependency(link_with: util),
    declare_dependency(link_with: time),
  ])

test('test_pg_decode_array', test_pg_decode_array)
test('test_pg_encode_array', test_pg_encode_array)
test('TestCronSchedule', TestCronSchedule)
