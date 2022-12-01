project('CM4all Workshop', ['c', 'cpp'], version: '6.0.10',
  meson_version: '>= 0.56',
  default_options: [
    'c_std=c11',
    'cpp_std=c++2a',
    'warning_level=3',
  ],
)

version_h = configuration_data()
version_h.set_quoted('PACKAGE', meson.project_name())
version_h.set_quoted('VERSION', meson.project_version())
configure_file(output: 'version.h', configuration: version_h)

debug = get_option('b_ndebug') != 'true'

compiler = meson.get_compiler('cpp')
c_compiler = meson.get_compiler('c')

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
  '-fno-omit-frame-pointer',
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

test_global_cflags = test_global_common_flags + [
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

  # the only warnings we got from this are from formatted error
  # messages, and their truncation is harmless
  '-Wno-format-truncation',

  # clang specific warning options:
  '-Wcomma',
  '-Wheader-hygiene',
  '-Winconsistent-missing-destructor-override',
]

if compiler.get_id() == 'gcc' and compiler.version().version_compare('>=11')
  # too many bogus warnings, e.g. for std::array::size()
  test_cxxflags += '-Wno-maybe-uninitialized'
endif

test_cflags = test_common_flags + [
  '-Wmissing-prototypes', '-Wstrict-prototypes',
  '-Wbad-function-cast',
  '-Waggregate-return',
  '-Wnested-externs',
]

add_global_arguments(common_flags, language: 'c')
add_global_arguments(common_flags, language: 'cpp')

add_global_arguments(compiler.get_supported_arguments(test_global_cxxflags), language: 'cpp')
add_global_arguments(c_compiler.get_supported_arguments(test_global_cflags), language: 'c')
add_project_arguments(compiler.get_supported_arguments(test_cxxflags), language: 'cpp')
add_project_arguments(c_compiler.get_supported_arguments(test_cflags), language: 'c')

inc = include_directories(
  'src',
  'libcommon/src',
  '.',
)

libcommon_enable_DefaultFifoBuffer = false
libcommon_enable_net_log = false

subdir('libcommon/src/util')
subdir('libcommon/src/time')
subdir('libcommon/src/io')
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
subdir('libcommon/src/event/systemd')
subdir('libcommon/src/lib/curl')
subdir('libcommon/src/lib/dbus')
subdir('libcommon/src/pg')
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
)

translation_dep = declare_dependency(
  link_with: translation2,
  dependencies: [
    translation_dep,
  ],
)

executable('cm4all-workshop',
  'src/main.cxx',
  'src/Config.cxx',
  'src/CommandLine.cxx',
  'src/CgroupAccounting.cxx',
  'src/AllocatorPtr.cxx',
  'src/SyslogClient.cxx',
  'src/CaptureBuffer.cxx',
  'src/PipeCaptureBuffer.cxx',
  'src/PipePondAdapter.cxx',
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
  'src/control/Server.cxx',
  include_directories: inc,
  dependencies: [
    event_net_dep,
    event_systemd_dep,
    time_dep,
    system_dep,
    io_dep,
    translation_dep,
    spawn_dep,
    pg_dep,
    curl_dep,
  ],
  install: true,
  install_dir: 'sbin',
)

executable('cm4all-workshop-migrate',
  'src/Migrate.cxx',
  include_directories: inc,
  dependencies: [
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
    net_dep,
    io_dep,
    util_dep,
  ],
  install: true,
)

executable(
  'cm4all-workshop-run-job',
  'src/workshop/RunJob.cxx',
  'src/workshop/ControlChannelServer.cxx',
  'src/workshop/ProgressReader.cxx',
  'src/AllocatorPtr.cxx',
  include_directories: inc,
  dependencies: [
    spawn_dep,
    event_net_dep,
  ],
  install: true,
)

subdir('t')
subdir('libcommon/test/util')
subdir('libcommon/test/net')
subdir('libcommon/test/pg')
subdir('libcommon/test/io/config')
subdir('doc')
