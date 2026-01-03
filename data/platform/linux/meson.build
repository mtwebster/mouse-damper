# Linux-specific data files

############# GSettings schema

install_data('org.mtw.mousedamper.gschema.xml',
  install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas')
)

# Compile schemas after installation
meson.add_install_script('meson_install_schemas.py')

############# Desktop files

# Autostart launcher
conf = configuration_data()
conf.set('exec', join_paths(bindir, 'mousedamper-launch'))

desktop_file = configure_file(
  input : 'mousedamper.desktop.in',
  output: 'mousedamper.desktop',
  configuration: conf,
  install: true,
  install_dir: join_paths(sys_data_dir, 'applications')
)

# Application menu entry for configuration
prefs_conf = configuration_data()
prefs_conf.set('prefs_path', join_paths(exec_path, 'mousedamper-config.py'))

desktop_file = configure_file(
  input : 'mousedamper-config.desktop.in',
  output: 'mousedamper-config.desktop',
  configuration: prefs_conf,
  install: true,
  install_dir: join_paths(sys_data_dir, 'applications')
)
