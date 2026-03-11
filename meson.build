project('figpad', 'c',
  version: '0.8.18.1.11',
  meson_version: '>= 0.59',
  default_options: ['warning_level=1'],
)

cc = meson.get_compiler('c')

gtk_dep = dependency('gtk4', version: '>= 4.0')

conf = configuration_data()
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_NAME', 'Figpad')
conf.set_quoted('PACKAGE_STRING', 'Figpad ' + meson.project_version())
conf.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
conf.set_quoted('ICONDIR', join_paths(get_option('prefix'), get_option('datadir'), 'pixmaps'))

conf.set('ENABLE_EMACS', 0)
conf.set('ENABLE_XINPUT2', 0)

opt_print = get_option('enable_print')
conf.set('ENABLE_PRINT', opt_print ? 1 : 0)

opt_statistics = get_option('enable_statistics')
conf.set('ENABLE_STATISTICS', opt_statistics ? 1 : 0)

opt_search_history = get_option('enable_search_history')
conf.set('SEARCH_HISTORY', opt_search_history ? 1 : 0)

configure_file(output: 'config.h', configuration: conf)

config_inc = include_directories('.')

subdir('src')
subdir('data')
subdir('po')
