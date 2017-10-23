project('CM4all Workshop', ['c', 'cpp'], version: '2.0.31',
  default_options: [
    'c_std=c99',
    'cpp_std=c++14'
  ],
)

# TODO: use get_option('b_ndebug'), but that doesn't work with Meson 0.37.1
if get_option('buildtype') == 'debug'
  debug = true
else
  debug = false
endif

common_flags = [
  '-D_REENTRANT', '-D_GNU_SOURCE',
  '-DPACKAGE="' + meson.project_name() + '"',
  '-DVERSION="' + meson.project_version() + '"',
  '-Wall',
  '-Wextra',
  '-Wwrite-strings', '-Wcast-qual', '-Wcast-align', '-Wfloat-equal',
  '-Wshadow', '-Wpointer-arith', '-Wsign-compare',
  '-Wmissing-declarations', '-Wmissing-noreturn', '-Wmissing-format-attribute',
  '-Wredundant-decls', '-Wno-long-long', '-Wundef',
  '-Wunused',
  '-Wundef',
]

if debug
  common_flags += ['-DPOISON']
endif

add_global_arguments(common_flags,
  '-Wmissing-prototypes', '-Wstrict-prototypes',
  '-Wbad-function-cast',
  '-Waggregate-return',
  '-Wredundant-decls', '-Wno-long-long', '-Wundef',
  '-Wnested-externs',
  '-pedantic',
  language: 'c')

add_global_arguments(common_flags,
  '-Wno-missing-field-initializers',
  '-Wno-non-virtual-dtor',
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
libcap = dependency('libcap')
libseccomp = dependency('libseccomp')
libpq = dependency('libpq', version: '>= 8.4')
libdaemon = dependency('libcm4all-daemon', version: '>= 0.4.2')

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
  ])
util_dep = declare_dependency(link_with: util)

time = static_library('time',
  'libcommon/src/time/gmtime.c',
  'libcommon/src/time/Convert.cxx',
  'libcommon/src/time/ISO8601.cxx',
  'src/time/Math.cxx',
  include_directories: inc,
  dependencies: [
  ])
time_dep = declare_dependency(link_with: time)

system = static_library('system',
  'src/system/SetupProcess.cxx',
  'libcommon/src/system/ProcessName.cxx',
  'libcommon/src/system/BindMount.cxx',
  'libcommon/src/system/CapabilityState.cxx',
  include_directories: inc,
  dependencies: [
    libcap,
  ])
system_dep = declare_dependency(link_with: system)

io = static_library('io',
  'libcommon/src/io/FileDescriptor.cxx',
  'libcommon/src/io/WriteFile.cxx',
  'libcommon/src/io/MultiWriteBuffer.cxx',
  'libcommon/src/io/LineParser.cxx',
  'libcommon/src/io/FileLineParser.cxx',
  'libcommon/src/io/ConfigParser.cxx',
  'libcommon/src/io/Logger.cxx',
  include_directories: inc,
  dependencies: [
    boost,
    util_dep,
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
    libevent,
    util_dep,
  ])
event_dep = declare_dependency(link_with: event)

net = static_library('net',
  'libcommon/src/net/SocketDescriptor.cxx',
  'libcommon/src/net/SocketAddress.cxx',
  'libcommon/src/net/AllocatedSocketAddress.cxx',
  'libcommon/src/net/IPv4Address.cxx',
  'libcommon/src/net/IPv6Address.cxx',
  'libcommon/src/net/HostParser.cxx',
  'libcommon/src/net/AddressInfo.cxx',
  'libcommon/src/net/Resolver.cxx',
  'libcommon/src/net/RConnectSocket.cxx',
  'libcommon/src/net/djb/NetstringHeader.cxx',
  'libcommon/src/net/djb/NetstringGenerator.cxx',
  'libcommon/src/net/djb/NetstringInput.cxx',
  include_directories: inc,
  dependencies: [
    event_dep,
  ])
net_dep = declare_dependency(link_with: net)

event_net = static_library('event_net',
  'libcommon/src/event/net/ConnectSocket.cxx',
  'libcommon/src/event/net/djb/NetstringClient.cxx',
  'libcommon/src/event/net/djb/QmqpClient.cxx',
  include_directories: inc,
  dependencies: [
    libevent,
    event_dep,
    net_dep,
    util_dep,
  ])
event_net_dep = declare_dependency(link_with: event_net)

curl = static_library('curl',
  'libcommon/src/curl/Version.cxx',
  'libcommon/src/curl/Request.cxx',
  'libcommon/src/curl/Global.cxx',
  'libcommon/src/curl/Init.cxx',
  include_directories: inc,
  dependencies: [
    libcurl,
    util_dep,
    event_dep,
  ])
curl_dep = declare_dependency(link_with: curl)

pg = static_library('pg',
  'libcommon/src/pg/Array.cxx',
  'libcommon/src/pg/Interval.cxx',
  'libcommon/src/pg/Timestamp.cxx',
  'libcommon/src/pg/Connection.cxx',
  'libcommon/src/pg/AsyncConnection.cxx',
  'libcommon/src/pg/Result.cxx',
  'libcommon/src/pg/Reflection.cxx',
  include_directories: inc,
  dependencies: [
    libpq,
  ])
pg_dep = declare_dependency(link_with: pg)

adata = static_library('adata',
  'libcommon/src/adata/ExpandableStringList.cxx',
  include_directories: inc,
  dependencies: [
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
    event_dep,
    net_dep,
    adata_dep,
  ])

odbus = static_library('odbus',
  'libcommon/src/odbus/Connection.cxx',
  'libcommon/src/odbus/Message.cxx',
  'libcommon/src/odbus/Error.cxx',
  'libcommon/src/odbus/ScopeMatch.cxx',
  include_directories: inc,
  dependencies: [
    libdbus,
  ])
odbus_dep = declare_dependency(link_with: odbus)

spawn = static_library('spawn',
  'libcommon/src/spawn/SeccompFilter.cxx',
  'libcommon/src/spawn/SyscallFilter.cxx',
  'libcommon/src/spawn/Systemd.cxx',
  'libcommon/src/spawn/Prepared.cxx',
  'libcommon/src/spawn/Registry.cxx',
  'libcommon/src/spawn/Direct.cxx',
  'libcommon/src/spawn/Init.cxx',
  'libcommon/src/spawn/Interface.cxx',
  'libcommon/src/spawn/Local.cxx',
  'libcommon/src/spawn/UserNamespace.cxx',
  'libcommon/src/spawn/NetworkNamespace.cxx',
  'libcommon/src/spawn/NamespaceOptions.cxx',
  'libcommon/src/spawn/MountList.cxx',
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
    system_dep,
    adata_dep,
    io_dep,
    odbus_dep,
  ])
spawn_dep = declare_dependency(link_with: spawn)

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
    libdaemon,
    event_net_dep,
    time_dep,
    system_dep,
    io_dep,
    declare_dependency(link_with: translation),
    spawn_dep,
    pg_dep,
    curl_dep,
  ])

executable('cm4all-workshop-migrate',
  'src/Migrate.cxx',
  include_directories: inc,
  dependencies: [
    pg_dep,
    util_dep,
  ])

subdir('t')
subdir('libcommon/test/util')
subdir('libcommon/test/pg')
subdir('libcommon/test/io')
