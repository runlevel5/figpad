glib_dep = dependency('glib-2.0')

test_encoding = executable('test-encoding',
  ['test-encoding.c', '../src/encoding.c'],
  include_directories: include_directories('../src'),
  dependencies: glib_dep,
)

test('encoding', test_encoding)
