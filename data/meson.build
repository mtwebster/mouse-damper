install_data('org.mtw.mouse-damper.gschema.xml',
    install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas')
)
meson.add_install_script('meson_install_schemas.py')

############# Desktop file

conf = configuration_data()
conf.set('exec', join_paths(get_option('prefix'), get_option('bindir'), 'mouse-damper-launch'))

desktop_file = configure_file(
  input : 'mouse-damper.desktop.in',
  output: 'mouse-damper.desktop',
  configuration: conf,
  install: true,
  install_dir: join_paths(get_option('sysconfdir'), 'xdg', 'autostart')
)

install_data('mouse-damper-config.desktop',
    install_dir: join_paths(get_option('datadir'), 'applications')
)