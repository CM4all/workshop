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

test(
  'TestWorkshop',
  executable(
    'TestWorkshop',
    'TestExpand.cxx',
    'TestCronSchedule.cxx',
    '../src/Expand.cxx',
    '../src/cron/Schedule.cxx',
    include_directories: inc,
    install: false,
    dependencies: [
      util_dep,
      time_dep,
      fmt_dep,
      gtest,
    ],
  ),
  env: ['TZ=CET'],
)
