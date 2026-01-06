# Icon handling for both platforms

if host_machine.system() == 'windows'
  ############# Windows: Generate .ico from SVG

  rsvg_convert = find_program('rsvg-convert', required: false)
  imagemagick = find_program('convert', required: false)

  if rsvg_convert.found() and imagemagick.found()
    # Generate PNG files at different resolutions
    icon_16 = custom_target('icon-16',
      input: 'mousedamper.svg',
      output: 'mousedamper-16.png',
      command: [rsvg_convert, '-w', '16', '-h', '16', '@INPUT@', '-o', '@OUTPUT@']
    )
    icon_32 = custom_target('icon-32',
      input: 'mousedamper.svg',
      output: 'mousedamper-32.png',
      command: [rsvg_convert, '-w', '32', '-h', '32', '@INPUT@', '-o', '@OUTPUT@']
    )
    icon_48 = custom_target('icon-48',
      input: 'mousedamper.svg',
      output: 'mousedamper-48.png',
      command: [rsvg_convert, '-w', '48', '-h', '48', '@INPUT@', '-o', '@OUTPUT@']
    )
    icon_256 = custom_target('icon-256',
      input: 'mousedamper.svg',
      output: 'mousedamper-256.png',
      command: [rsvg_convert, '-w', '256', '-h', '256', '@INPUT@', '-o', '@OUTPUT@']
    )

    # Combine PNGs into multi-resolution .ico
    icon_ico = custom_target('mousedamper-icon',
      input: [icon_16, icon_32, icon_48, icon_256],
      output: 'mousedamper.ico',
      command: [imagemagick, '@INPUT@', '@OUTPUT@']
    )
  else
    warning('rsvg-convert or ImageMagick convert not found - Windows icon will not be generated')
    warning('Install librsvg2-bin and imagemagick to enable icon generation')
  endif

else
  ############# Linux: Install SVG icons

  # Symbolic icon for system tray
  install_data('mousedamper-symbolic.svg',
    install_dir: join_paths(get_option('datadir'), 'icons', 'hicolor', 'symbolic', 'apps')
  )

  # Regular icon for application windows
  install_data('mousedamper.svg',
    install_dir: join_paths(get_option('datadir'), 'icons', 'hicolor', 'scalable', 'apps')
  )

  meson.add_install_script('meson_update_icon_cache.py')
endif
