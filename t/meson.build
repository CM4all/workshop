gtest = dependency('gtest', main: true,
                   include_type: 'system',
                   fallback: ['gtest', 'gtest_main_dep'])

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
    fmt_dep,
    gtest,
  ])

test('TestCronSchedule', TestCronSchedule,
    env: ['TZ=CET'])
