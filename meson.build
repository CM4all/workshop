project('CM4all Workshop', 'cpp', version: '6.2',
  meson_version: '>= 1.0',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3',

    'fmt:cpp_std=c++20',
  ],
)

version_h = configuration_data()
version_h.set_quoted('PACKAGE', meson.project_name())
version_h.set_quoted('VERSION', meson.project_version())
configure_file(output: 'version.h', configuration: version_h)

debug = get_option('b_ndebug') != 'true'

compiler = meson.get_compiler('cpp')

common_flags = [
  '-D_GNU_SOURCE',
]

test_common_flags = [
  '-Wcast-align',
  '-Wcast-qual',
  '-Wdouble-promotion',
  '-Wfloat-equal',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-noreturn',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wshadow',
  '-Wsign-compare',
  '-Wundef',
  '-Wunused',
  '-Wvla',
  '-Wwrite-strings',

  # clang specific warning options:
  '-Wunreachable-code-aggressive',
  '-Wused-but-marked-unused',
]

test_global_common_flags = [
  '-fvisibility=hidden',
]

add_global_link_arguments(
  compiler.get_supported_link_arguments(
    # make relocations read-only (hardening)
    '-Wl,-z,relro',

    # no lazy binding, please - not worth it for a daemon
    '-Wl,-z,now',
  ),
  language: 'cpp'
)

if not debug
  test_common_flags += [
    '-ffunction-sections',
    '-fdata-sections',
  ]

  add_global_link_arguments(
    compiler.get_supported_link_arguments(
      '-Wl,--gc-sections',
      '-Wl,--icf=all',
    ),
    language: 'cpp'
  )
endif

test_global_cxxflags = test_global_common_flags + [
]

test_cxxflags = test_common_flags + [
  '-fno-threadsafe-statics',
  '-fmerge-all-constants',

  '-Wcomma-subscript',
  '-Wextra-semi',
  '-Wmismatched-tags',
  '-Woverloaded-virtual',
  '-Wsign-promo',
  '-Wvolatile',
  '-Wvirtual-inheritance',

  '-Wno-missing-field-initializers',

  # a vtable without a dtor is just fine
  '-Wno-non-virtual-dtor',

  # clang specific warning options:
  '-Wcomma',
  '-Wheader-hygiene',
  '-Winconsistent-missing-destructor-override',
]

if compiler.get_id() == 'gcc' and compiler.version().version_compare('>=11')
  # too many bogus warnings, e.g. for std::array::size()
  test_cxxflags += '-Wno-maybe-uninitialized'
endif

add_global_arguments(common_flags, language: 'cpp')
add_global_arguments(compiler.get_supported_arguments(test_global_cxxflags), language: 'cpp')
add_project_arguments(compiler.get_supported_arguments(test_cxxflags), language: 'cpp')

inc = include_directories(
  'src',
  'libcommon/src',
  '.',
)

libsystemd = dependency('libsystemd', required: get_option('systemd'))

libcommon_enable_DefaultFifoBuffer = false
libcommon_enable_spawn_local = false
libcommon_require_cap = get_option('cap')
libcommon_enable_seccomp = get_option('seccomp')
libcommon_enable_libsystemd = libsystemd.found()

subdir('libcommon/src/util')
subdir('libcommon/src/uri')
subdir('libcommon/src/co')
subdir('libcommon/src/lib/cap')
subdir('libcommon/src/lib/fmt')
subdir('libcommon/src/time')
subdir('libcommon/src/io')
subdir('libcommon/src/io/linux')
subdir('libcommon/src/io/config')
subdir('libcommon/src/system')

system2 = static_library(
  'system2',
  'src/system/SetupProcess.cxx',
  include_directories: inc,
)
system_dep = declare_dependency(
  link_with: system2,
  dependencies: [
    system_dep,
  ],
)

subdir('libcommon/src/event')
subdir('libcommon/src/net')
subdir('libcommon/src/net/control')
subdir('libcommon/src/net/djb')

net2 = static_library(
  'net2',
  'libcommon/src/net/log/Send.cxx',
  include_directories: inc,
)
net_dep = declare_dependency(
  link_with: net2,
  dependencies: [
    net_dep,
  ],
)

subdir('libcommon/src/event/net')
subdir('libcommon/src/event/net/control')
subdir('libcommon/src/event/net/djb')
subdir('libcommon/src/lib/curl')

if libsystemd.found()
  subdir('libcommon/src/event/systemd')
  subdir('libcommon/src/lib/dbus')
else
  event_systemd_dep = dependency('', required: false)
endif

subdir('libcommon/src/pg')

alloc = static_library(
  'alloc',
  'src/AllocatorPtr.cxx',
  include_directories: inc,
)

alloc_dep = declare_dependency(
  link_with: alloc,
)

subdir('libcommon/src/adata')
subdir('libcommon/src/spawn')
subdir('libcommon/src/translation')

translation2 = static_library(
  'translation2',
  'src/translation/CronClient.cxx',
  'src/translation/CronGlue.cxx',
  'src/translation/Receive.cxx',
  'src/translation/Send.cxx',
  'src/translation/SpawnClient.cxx',
  include_directories: inc,
  dependencies: [
    coroutines_dep,
  ],
)

translation_dep = declare_dependency(
  link_with: translation2,
  dependencies: [
    coroutines_dep,
    translation_dep,
  ],
)

executable('cm4all-workshop',
  'src/main.cxx',
  'src/Config.cxx',
  'src/CommandLine.cxx',
  'src/CgroupAccounting.cxx',
  'src/CaptureBuffer.cxx',
  'src/PipeCaptureBuffer.cxx',
  'src/PipePondAdapter.cxx',
  'src/Expand.cxx',
  'src/Instance.cxx',
  'src/Hook.cxx',
  'src/EmailService.cxx',
  'src/NsQrelayConnect.cxx',
  'src/cron/Config.cxx',
  'src/cron/Schedule.cxx',
  'src/cron/Result.cxx',
  'src/cron/Notification.cxx',
  'src/cron/Queue.cxx',
  'src/cron/Workplace.cxx',
  'src/cron/SpawnOperator.cxx',
  'src/cron/CurlOperator.cxx',
  'src/cron/CalculateNextRun.cxx',
  'src/cron/Partition.cxx',
  'src/workshop/Config.cxx',
  'src/workshop/ControlChannelServer.cxx',
  'src/workshop/Partition.cxx',
  'src/workshop/Queue.cxx',
  'src/workshop/PGQueue.cxx',
  'src/workshop/Job.cxx',
  'src/workshop/PlanLoader.cxx',
  'src/workshop/PlanLibrary.cxx',
  'src/workshop/PlanUpdate.cxx',
  'src/workshop/RateLimit.cxx',
  'src/workshop/MultiLibrary.cxx',
  'src/workshop/ProgressReader.cxx',
  'src/workshop/LogBridge.cxx',
  'src/workshop/Operator.cxx',
  'src/workshop/Workplace.cxx',
  include_directories: inc,
  dependencies: [
    coroutines_dep,
    event_net_dep,
    event_net_djb_dep,
    event_systemd_dep,
    control_server_dep,
    time_dep,
    system_dep,
    io_dep,
    translation_dep,
    spawn_dep,
    pg_dep,
    curl_dep,
    fmt_dep,
    uri_dep,
  ],
  install: true,
  install_dir: 'sbin',
)

executable('cm4all-workshop-migrate',
  'src/Migrate.cxx',
  include_directories: inc,
  dependencies: [
    fmt_dep,
    pg_dep,
    util_dep,
  ],
  install: true,
)

executable(
  'cm4all-workshop-control',
  'src/control/Client.cxx',
  include_directories: inc,
  dependencies: [
    fmt_dep,
    control_client_dep,
    net_dep,
    util_dep,
  ],
  install: true,
)

executable(
  'cm4all-workshop-run-job',
  'src/workshop/RunJob.cxx',
  'src/workshop/ControlChannelServer.cxx',
  'src/workshop/ProgressReader.cxx',
  include_directories: inc,
  dependencies: [
    fmt_dep,
    spawn_dep,
    event_net_dep,
  ],
  install: true,
)

conf = configuration_data()
conf.set('HAVE_LIBCAP', cap_dep.found())
conf.set('HAVE_LIBSYSTEMD', libsystemd.found())
configure_file(output: 'config.h', configuration: conf)

subdir('t')
subdir('libcommon/test/util')
subdir('libcommon/test/net')
subdir('libcommon/test/pg')
subdir('libcommon/test/io/config')
subdir('doc')
