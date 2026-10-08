i18n = import('i18n')

app_id = 'com.github.runlevel5.Figpad'

# Desktop file
i18n.merge_file(
  input: app_id + '.desktop.in',
  output: app_id + '.desktop',
  type: 'desktop',
  po_dir: join_paths(meson.project_source_root(), 'po'),
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications'),
)

# Pixmaps
install_data('figpad.png', 'figpad.xpm',
  install_dir: join_paths(get_option('datadir'), 'pixmaps'),
)

# Hicolor icons
icon_sizes = ['16x16', '20x20', '22x22', '24x24', '32x32', '36x36',
              '40x40', '48x48', '64x64', '72x72', '96x96',
              '128x128', '192x192', '256x256']

foreach size : icon_sizes
  install_data(
    join_paths('icons', size, app_id + '.png'),
    install_dir: join_paths(get_option('datadir'), 'icons', 'hicolor', size, 'apps'),
  )
endforeach

install_data(
  join_paths('icons', 'scalable', app_id + '.svg'),
  install_dir: join_paths(get_option('datadir'), 'icons', 'hicolor', 'scalable', 'apps'),
)
