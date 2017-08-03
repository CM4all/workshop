executable('http',
  'http.cxx',
  install: false,
  include_directories: inc,
  dependencies: [
    declare_dependency(link_with: curl),
  ])

TestCronSchedule = executable('TestCronSchedule',
  'TestCronSchedule.cxx',
  '../src/cron/Schedule.cxx',
  include_directories: inc,
  install: false,
  dependencies: [
    declare_dependency(link_with: util),
    declare_dependency(link_with: time),
  ])

test('TestCronSchedule', TestCronSchedule)
