project('CM4all Workshop', ['c', 'cpp'], version: '2.0.13',
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

gtest = declare_dependency(
  dependencies: [dependency('gtest', main: true)],
  compile_args: [
    '-Wno-undef',
    '-Wno-suggest-attribute=format',
    '-Wno-suggest-attribute=noreturn',
    '-Wno-missing-declarations',

    # needed on Jessie for gtest's IsNullLiteralHelper
    '-Wno-conversion-null',
  ],
)

boost = dependency('boost', modules: ['system', 'filesystem'], version: '>= 1.54')
libevent = dependency('libevent', version: '>= 2.0.19')
libcurl = dependency('libcurl', version: '>= 7.38')
libsystemd = dependency('libsystemd')
libdbus = dependency('dbus-1')
libseccomp = dependency('libseccomp')
libpq = dependency('libpq', version: '>= 8.4')
libinline = dependency('libcm4all-inline', version: '>= 0.3.2')
libdaemon = dependency('libcm4all-daemon', version: '>= 0.4.2')
libsocket = dependency('libcm4all-socket', version: '>= 0.3.5')
libbp = dependency('libcm4all-beng-proxy', version: '>= 12.0.17')

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
  'libcommon/src/util/djbhash.c',
  include_directories: inc,
  dependencies: [
    libinline
  ])
util_dep = declare_dependency(link_with: util)

time = static_library('time',
  'libcommon/src/time/gmtime.c',
  'src/time/ISO8601.cxx',
  'src/time/Convert.cxx',
  'src/time/Math.cxx',
  include_directories: inc,
  dependencies: [
    libinline,
  ])
time_dep = declare_dependency(link_with: time)

system = static_library('system',
  'src/system/SetupProcess.cxx',
  'libcommon/src/system/BindMount.cxx',
  include_directories: inc,
  dependencies: [
    libinline,
  ])
system_dep = declare_dependency(link_with: system)

io = static_library('io',
  'libcommon/src/io/FileDescriptor.cxx',
  'libcommon/src/io/WriteFile.cxx',
  'libcommon/src/io/MultiWriteBuffer.cxx',
  'libcommon/src/io/LineParser.cxx',
  'libcommon/src/io/FileLineParser.cxx',
  'libcommon/src/io/ConfigParser.cxx',
  include_directories: inc,
  dependencies: [
    boost,
    libinline,
    declare_dependency(link_with: util),
  ])
io_dep = declare_dependency(link_with: io)

event = static_library('event',
  'libcommon/src/event/Loop.cxx',
  'libcommon/src/event/ShutdownListener.cxx',
  'libcommon/src/event/CleanupTimer.cxx',
  'libcommon/src/event/DeferEvent.cxx',
  'libcommon/src/event/SignalEvent.cxx',
  'libcommon/src/event/PipeLineReader.cxx',
  include_directories: inc,
  dependencies: [
    libinline,
    libevent,
    declare_dependency(link_with: util),
  ])
event_dep = declare_dependency(link_with: event)

net = static_library('net',
  'libcommon/src/net/SocketDescriptor.cxx',
  'libcommon/src/net/SocketAddress.cxx',
  'libcommon/src/net/AllocatedSocketAddress.cxx',
  'libcommon/src/net/Resolver.cxx',
  'libcommon/src/net/ConnectSocket.cxx',
  'libcommon/src/net/RConnectSocket.cxx',
  'libcommon/src/net/djb/NetstringHeader.cxx',
  'libcommon/src/net/djb/NetstringGenerator.cxx',
  'libcommon/src/net/djb/NetstringInput.cxx',
  'libcommon/src/net/djb/NetstringClient.cxx',
  'libcommon/src/net/djb/QmqpClient.cxx',
  include_directories: inc,
  dependencies: [
    libinline, libdaemon, libsocket,
    declare_dependency(link_with: event),
  ])

curl = static_library('curl',
  'libcommon/src/curl/Version.cxx',
  'libcommon/src/curl/Request.cxx',
  'libcommon/src/curl/Global.cxx',
  'libcommon/src/curl/Init.cxx',
  include_directories: inc,
  dependencies: [
    libcurl,
    libinline,
    util_dep,
    event_dep,
  ])

pg = static_library('pg',
  'libcommon/src/pg/Array.cxx',
  'libcommon/src/pg/Connection.cxx',
  'libcommon/src/pg/AsyncConnection.cxx',
  'libcommon/src/pg/Result.cxx',
  'libcommon/src/pg/Reflection.cxx',
  include_directories: inc,
  dependencies: [
    libpq,
    libinline,
  ])
pg_dep = declare_dependency(link_with: pg)

adata = static_library('adata',
  'libcommon/src/adata/ExpandableStringList.cxx',
  include_directories: inc,
  dependencies: [
    libinline,
  ])
adata_dep = declare_dependency(link_with: adata)

translation = static_library('translation',
  'src/translation/CronClient.cxx',
  'src/translation/CronGlue.cxx',
  'libcommon/src/translation/PReader.cxx',
  'libcommon/src/translation/Parser.cxx',
  'libcommon/src/translation/Response.cxx',
  include_directories: inc,
  dependencies: [
    libinline, libbp,
    declare_dependency(link_with: event),
    declare_dependency(link_with: net),
    adata_dep,
  ])

odbus = static_library('odbus',
  'libcommon/src/odbus/Message.cxx',
  include_directories: inc,
  dependencies: [
    libdbus,
    libinline,
  ])
odbus_dep = declare_dependency(link_with: odbus)

spawn = static_library('spawn',
  'libcommon/src/spawn/SeccompFilter.cxx',
  'libcommon/src/spawn/SyscallFilter.cxx',
  'libcommon/src/spawn/Systemd.cxx',
  'libcommon/src/spawn/Prepared.cxx',
  'libcommon/src/spawn/Registry.cxx',
  'libcommon/src/spawn/Direct.cxx',
  'libcommon/src/spawn/Interface.cxx',
  'libcommon/src/spawn/Local.cxx',
  'libcommon/src/spawn/NamespaceOptions.cxx',
  'libcommon/src/spawn/mount_list.cxx',
  'libcommon/src/spawn/JailConfig.cxx',
  'libcommon/src/spawn/JailParams.cxx',
  'libcommon/src/spawn/ChildOptions.cxx',
  'libcommon/src/spawn/CgroupOptions.cxx',
  'libcommon/src/spawn/UidGid.cxx',
  'libcommon/src/spawn/ResourceLimits.cxx',
  'libcommon/src/spawn/RefenceOptions.cxx',
  'libcommon/src/spawn/Server.cxx',
  'libcommon/src/spawn/Launch.cxx',
  'libcommon/src/spawn/Client.cxx',
  'libcommon/src/spawn/Glue.cxx',
  'libcommon/src/spawn/ConfigParser.cxx',
  include_directories: inc,
  dependencies: [
    libdbus,
    libseccomp,
    libsystemd,
    libinline, libdaemon,
    system_dep,
    adata_dep,
    io_dep,
    odbus_dep,
  ])

executable('cm4all-workshop',
  'src/main.cxx',
  'src/Config.cxx',
  'src/CommandLine.cxx',
  'src/AllocatorPtr.cxx',
  'src/SyslogClient.cxx',
  'src/CaptureBuffer.cxx',
  'src/PipeCaptureBuffer.cxx',
  'src/Expand.cxx',
  'src/Instance.cxx',
  'src/Hook.cxx',
  'src/EmailService.cxx',
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
  'src/workshop/ProgressReader.cxx',
  'src/workshop/LogBridge.cxx',
  'src/workshop/Operator.cxx',
  'src/workshop/Workplace.cxx',
  include_directories: inc,
  dependencies: [
    boost,
    libsystemd,
    libinline, libdaemon,
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

test('TestCronSchedule', TestCronSchedule)

subdir('libcommon/test/pg')
subdir('libcommon/test/io')
