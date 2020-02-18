gtest_compile_args = [
  '-Wno-undef',
]

if compiler.get_id() == 'gcc'
  gtest_compile_args += [
    '-Wno-suggest-attribute=format',
    '-Wno-suggest-attribute=noreturn',
    '-Wno-missing-declarations',
  ]
endif

gtest = declare_dependency(
  dependencies: [dependency('gtest', main: true)],
  compile_args: gtest_compile_args,
)

executable('http',
  'http.cxx',
  install: false,
  include_directories: inc,
  dependencies: [
    curl_dep,
  ])

TestCronSchedule = executable('TestCronSchedule',
  'TestCronSchedule.cxx',
  '../src/cron/Schedule.cxx',
  include_directories: inc,
  install: false,
  dependencies: [
    util_dep,
    time_dep,
    gtest,
  ])

test('TestCronSchedule', TestCronSchedule,
    env: ['TZ=CET'])
