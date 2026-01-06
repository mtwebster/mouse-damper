# Windows platform-specific code

# Platform sources
platform_sources = files('platform_windows.c')

# Platform dependencies (minimal - just math library if available)
math_dep = meson.get_compiler('c').find_library('m', required: false)
platform_deps = math_dep.found() ? [math_dep] : []

# Windows-specific dependencies
# Note: MSYS2/MinGW gettext doesn't provide libintl.pc, so use find_library
libintl_dep = meson.get_compiler('c').find_library('intl', required: true)

# Target Windows 7 and above
win7_args = ['-D_WIN32_WINNT=0x0601', '-DWINVER=0x0601']

# Windows module for resource compilation
windows_mod = import('windows')

############ Configure build-time config header

config_conf = configuration_data()
config_conf.set('VERSION', meson.project_version())

# Convert version "0.1.0" to "0,1,0,0" for .rc files
version_parts = meson.project_version().split('.')
version_csv = ','.join(version_parts + ['0'])
config_conf.set('VERSION_CSV', version_csv)

# Executable paths
config_conf.set('DAEMON_PATH', exec_path / 'mousedamper.exe')
config_conf.set('CONFIG_PATH', exec_path / 'mousedamper-config.exe')

configure_file(
  input : 'config.h.in',
  output: 'config.h',
  configuration: config_conf,
)

############ Windows-specific settings backend sources

windows_config_sources = files('settings-backend.c')

############ Resource files with icon dependency

# Launcher resource
launcher_res = windows_mod.compile_resources(
  'mousedamper-launcher.rc',
  depend_files: files('mousedamper-launcher_resource.h'),
  depends: icon_ico,
  include_directories: [include_directories('.'), include_directories('../..')]
)

# Config GUI resource
config_gui_res = windows_mod.compile_resources(
  'mousedamper-config.rc',
  depend_files: files('mousedamper-config_resource.h'),
  depends: icon_ico,
  include_directories: [include_directories('.'), include_directories('../..')]
)

############ Build Windows executables

# Launcher executable (reads config, launches daemon, system tray)
windows_launch_exe = executable('mousedamper-launch',
  files('mousedamper-launcher.c') + windows_config_sources + launcher_res,
  dependencies: [libintl_dep],
  c_args: win7_args,
  link_args: ['-mwindows', '-lshell32', '-lshlwapi', '-lcomctl32'],
  install: true,
  install_dir: exec_path,
  win_subsystem: 'windows'
)

# Configuration GUI (dialog-based Win32 app)
windows_config_exe = executable('mousedamper-config',
  files('mousedamper-config.c') + windows_config_sources + config_gui_res,
  dependencies: [libintl_dep],
  c_args: win7_args,
  link_args: ['-mwindows', '-lcomctl32', '-lshlwapi', '-lshell32'],
  install: true,
  install_dir: exec_path,
  win_subsystem: 'windows'
)
