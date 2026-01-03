# Main mousedamper sources
mousedamper_sources = files('mousedamper.c')

# Build common and platform-specific code from subdirectories
subdir('common')
subdir('platform')

# Build main mousedamper executable
# platform_sources and platform_deps are populated by platform/*/meson.build
if host_machine.system() == 'windows'
  # Windows: console subsystem for daemon
  mousedamper_exe = executable('mousedamper',
    mousedamper_sources + common_sources + platform_sources,
    dependencies: platform_deps,
    c_args: win7_args,
    install: true,
    install_dir: exec_path,
    win_subsystem: 'console'
  )
else
  # Linux: install with setuid bit for device access
  mousedamper_exe = executable('mousedamper',
    mousedamper_sources + common_sources + platform_sources,
    dependencies: platform_deps,
    install: true,
    install_dir: exec_path,
    install_mode: ['rwsr-xr-x', 'root', 'root']
  )
endif
