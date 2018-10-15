project('CM4all Workshop', ['c', 'cpp'], version: '3.0.5',
  default_options: [
    'c_std=c99',
    'cpp_std=c++14'
  ],
)

compiler = meson.get_compiler('cpp')

# TODO: use get_option('b_ndebug'), but that doesn't work with Meson 0.37.1
if get_option('buildtype') == 'debug'
  debug = true
else
  debug = false
endif

compiler = meson.get_compiler('cpp')
c_compiler = meson.get_compiler('c')

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

test_common_flags = [
  '-fvisibility=hidden',
]

test_cxxflags = test_common_flags + [
  '-fno-threadsafe-statics',
  '-fmerge-all-constants',

  '-Wno-missing-field-initializers',
  '-Wno-non-virtual-dtor',

  # the only warnings we got from this are from formatted error
  # messages, and their truncation is harmless
  '-Wno-format-truncation',
]

test_cflags = test_common_flags + [
  '-Wmissing-prototypes', '-Wstrict-prototypes',
  '-Wbad-function-cast',
  '-Waggregate-return',
  '-Wnested-externs',
  '-pedantic',
]

add_global_arguments(common_flags, language: 'c')
add_global_arguments(common_flags, language: 'cpp')

foreach f: test_cxxflags
  if compiler.has_argument(f)
    add_global_arguments(f, language: 'cpp')
  endif
endforeach

foreach f: test_cflags
  if c_compiler.has_argument(f)
    add_global_arguments(f, language: 'c')
  endif
endforeach

gtest_compile_args = [
  '-Wno-undef',
]

if compiler.get_id() == 'gcc'
  gtest_compile_args += [
    '-Wno-suggest-attribute=format',
    '-Wno-suggest-attribute=noreturn',
    '-Wno-missing-declarations',

    # needed on Jessie for gtest's IsNullLiteralHelper
    '-Wno-conversion-null',
  ]
endif

gtest = declare_dependency(
  dependencies: [dependency('gtest', main: true)],
  compile_args: gtest_compile_args,
)

inc = include_directories(
  'src',
  'libcommon/src',
)

libcommon_enable_DefaultFifoBuffer = false

subdir('libcommon/src/util')

time = static_library('time',
  'libcommon/src/time/gmtime.c',
  'libcommon/src/time/Convert.cxx',
  'libcommon/src/time/ISO8601.cxx',
  'src/time/Math.cxx',
  include_directories: inc,
  dependencies: [
  ])
time_dep = declare_dependency(link_with: time)

subdir('libcommon/src/io')
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
subdir('libcommon/src/event/net')
subdir('libcommon/src/curl')
subdir('libcommon/src/pg')
subdir('libcommon/src/adata')
subdir('libcommon/src/odbus')
subdir('libcommon/src/spawn')
subdir('libcommon/src/translation')

translation2 = static_library(
  'translation2',
  'src/translation/CronClient.cxx',
  'src/translation/CronGlue.cxx',
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
  'src/workshop/MultiLibrary.cxx',
  'src/workshop/ProgressReader.cxx',
  'src/workshop/LogBridge.cxx',
  'src/workshop/Operator.cxx',
  'src/workshop/Workplace.cxx',
  'src/control/Server.cxx',
  include_directories: inc,
  dependencies: [
    event_net_dep,
    time_dep,
    system_dep,
    io_dep,
    translation_dep,
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

executable(
  'cm4all-workshop-control',
  'src/control/Client.cxx',
  include_directories: inc,
  dependencies: [
    net_dep,
    io_dep,
    util_dep,
  ]
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
)

subdir('t')
subdir('libcommon/test/util')
subdir('libcommon/test/net')
subdir('libcommon/test/pg')
subdir('libcommon/test/io')
subdir('doc')
