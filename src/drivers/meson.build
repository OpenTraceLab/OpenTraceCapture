driver_sources = []

# Helper function pattern for gated drivers
# Always-available drivers (USB/Serial/HID with standard deps)
always_available = [
  'fx2lafw', 'demo', 'saleae-logic16', 'kingst-la2016', 'dreamsourcelab-dslogic',
  'chronovu-la', 'hantek-6xxx', 'hantek-4032l', 'hantek-dso', 'kecheng-kc-330b',
  'lascar-el-usb', 'microchip-pickit2', 'asix-sigma', 'ikalogic-scanalogic2',
  'ikalogic-scanaplus', 'pipistrello-ols', 'atten-pps3xxx', 'korad-kaxxxxp',
  'manson-hcs-3xxx', 'openbench-logic-sniffer', 'gmc-mh-1x-2x', 'serial-lcr',
  'mastech-ms6514', 'atorch', 'kern-scale', 'serial-dmm',
  'agilent-dmm', 'center-3xx', 'uni-t-ut32x', 'rigol-ds', 'hameg-hmo',
  'siglent-sds', 'asix-omega-rtm-cli', 'rdtech-dps',
  'raspberrypi-pico', 'arachnid-labs-re-load-pro', 'pce-322a', 'hp-59306a',
  'teleinfo', 'baylibre-acme', 'rigol-dg',
  'norma-dmm', 'cem-dt-885x', 'rdtech-um', 'devantech-eth008',
  'motech-lps-30x', 'zketech-ebd-usb', 'tondaj-sl-814', 'icstation-usbrelay',
  'conrad-digi-35-cpu', 'bkprecision-1856d', 'juntek-jds6600', 'gwinstek-gpd',
  'colead-slm', 'mic-985xx'
]

# Always-available drivers (USB/Serial/HID with standard deps)
always_available = [
  'fx2lafw', 'demo', 'saleae-logic16', 'kingst-la2016', 'dreamsourcelab-dslogic',
  'chronovu-la', 'hantek-6xxx', 'hantek-4032l', 'hantek-dso', 'kecheng-kc-330b',
  'lascar-el-usb', 'microchip-pickit2', 'asix-sigma', 'ikalogic-scanalogic2',
  'ikalogic-scanaplus', 'pipistrello-ols', 'atten-pps3xxx', 'korad-kaxxxxp',
  'manson-hcs-3xxx', 'openbench-logic-sniffer', 'gmc-mh-1x-2x', 'serial-lcr',
  'mastech-ms6514', 'atorch', 'appa-55ii', 'appa-dmm', 'kern-scale', 'serial-dmm',
  'agilent-dmm', 'center-3xx', 'uni-t-ut32x', 'rigol-ds', 'hameg-hmo',
  'siglent-sds', 'asix-omega-rtm-cli', 'rdtech-dps',
  'raspberrypi-pico', 'arachnid-labs-re-load-pro', 'pce-322a', 'hp-59306a',
  'teleinfo', 'rigol-dg',
  'norma-dmm', 'cem-dt-885x', 'rdtech-um', 'devantech-eth008',
  'motech-lps-30x', 'zketech-ebd-usb', 'tondaj-sl-814', 'icstation-usbrelay',
  'conrad-digi-35-cpu', 'bkprecision-1856d', 'juntek-jds6600', 'gwinstek-gpd',
  'colead-slm', 'mic-985xx'
]

# Add always-available drivers
foreach driver : always_available
  if fs.is_dir('../hardware/' + driver)
    driver_sources += files('../hardware/' + driver + '/api.c')
    if fs.is_file('../hardware/' + driver + '/protocol.c')
      driver_sources += files('../hardware/' + driver + '/protocol.c')
    endif
    enabled_drivers += driver
  endif
endforeach

# libserialport-specific drivers (need libserialport, not just serial_comm)
if dep_libserialport.found()
  libserialport_drivers = ['link-mso19', 'uni-t-ut181a', 'appa-55ii', 'gwinstek-psp', 'itech-it8500', 'saleae-logic-pro']
  foreach driver : libserialport_drivers
    if fs.is_dir('../hardware/' + driver)
      driver_sources += files('../hardware/' + driver + '/api.c')
      if fs.is_file('../hardware/' + driver + '/protocol.c')
        driver_sources += files('../hardware/' + driver + '/protocol.c')
      endif
      conf.set10('HAVE_DRIVER_' + driver.underscorify().to_upper(), true)
      enabled_drivers += driver
    endif
  endforeach
else
  disabled_drivers += 'link-mso19 (needs libserialport)'
  disabled_drivers += 'uni-t-ut181a (needs libserialport)'
  disabled_drivers += 'appa-55ii (needs libserialport)'
  disabled_drivers += 'gwinstek-psp (needs libserialport)'
  disabled_drivers += 'itech-it8500 (needs libserialport)'
  disabled_drivers += 'saleae-logic-pro (needs libserialport)'
endif

# Linux-specific drivers (need GPIO/sysfs)
if host_machine.system() == 'linux'
  linux_drivers = ['baylibre-acme']
  foreach driver : linux_drivers
    if fs.is_dir('../hardware/' + driver)
      driver_sources += files('../hardware/' + driver + '/api.c', '../hardware/' + driver + '/protocol.c')
      # Add GPIO support for baylibre-acme
      if driver == 'baylibre-acme'
        driver_sources += files('../hardware/' + driver + '/gpio.c')
      endif
      enabled_drivers += driver
    endif
  endforeach
else
  disabled_drivers += 'baylibre-acme (needs Linux GPIO/sysfs)'
endif

# Nettle-dependent drivers (crypto)
if have_nettle and not opt_nettle.disabled()
  nettle_drivers = ['rdtech-tc']
  foreach driver : nettle_drivers
    if fs.is_dir('../hardware/' + driver)
      driver_sources += files('../hardware/' + driver + '/api.c', '../hardware/' + driver + '/protocol.c')
      conf.set10('HAVE_DRIVER_' + driver.underscorify().to_upper(), true)
      enabled_drivers += driver
    endif
  endforeach
else
  disabled_drivers += 'rdtech-tc (needs nettle/aes.h via libnettle)'
endif

# IEEE1284-dependent drivers (parallel port)
if have_ieee1284 and not opt_ieee1284.disabled()
  ieee1284_drivers = ['hung-chang-dso-2100xl']
  foreach driver : ieee1284_drivers
    if fs.is_dir('../hardware/' + driver)
      driver_sources += files('../hardware/' + driver + '/api.c', '../hardware/' + driver + '/protocol.c')
      conf.set10('HAVE_DRIVER_' + driver.underscorify().to_upper(), true)
      enabled_drivers += driver
    endif
  endforeach
else
  disabled_drivers += 'hung-chang-dso-2100xl (needs libieee1284 / ieee1284.h)'
endif

# GPIB-dependent drivers
if have_gpib and not opt_gpib.disabled()
  gpib_drivers = ['lecroy-xstream', 'rohde-schwarz-hmo', 'tektronix-tds', 'yokogawa-dlm']
  foreach driver : gpib_drivers
    if fs.is_dir('../hardware/' + driver)
      driver_sources += files('../hardware/' + driver + '/api.c', '../hardware/' + driver + '/protocol.c')
      conf.set10('HAVE_DRIVER_' + driver.underscorify().to_upper(), true)
      enabled_drivers += driver
    endif
  endforeach
else
  disabled_drivers += 'lecroy-xstream (needs linux-gpib: gpib/ib.h)'
  disabled_drivers += 'rohde-schwarz-hmo (needs linux-gpib: gpib/ib.h)'
  disabled_drivers += 'tektronix-tds (needs linux-gpib: gpib/ib.h)'
  disabled_drivers += 'yokogawa-dlm (needs linux-gpib: gpib/ib.h)'
endif

# HID-dependent drivers
if dep_hidapi.found()
  hid_drivers = ['saleae-logic-pro']
  foreach driver : hid_drivers
    if fs.is_dir('../hardware/' + driver)
      driver_sources += files('../hardware/' + driver + '/api.c', '../hardware/' + driver + '/protocol.c')
      conf.set10('HAVE_DRIVER_' + driver.underscorify().to_upper(), true)
      enabled_drivers += driver
    endif
  endforeach
else
  disabled_drivers += 'saleae-logic-pro (needs HIDAPI)'
endif

# Platform-specific drivers (always disabled by default)
platform_drivers = ['beaglelogic', 'ipdbg-la']
foreach driver : platform_drivers
  disabled_drivers += driver + ' (platform-specific, requires special hardware/kernel)'
endforeach

# Vendor SDK drivers (disabled by default)
if get_option('with_vendor_sdks').enabled()
  # Placeholder for vendor drivers - would need actual SDK paths
  disabled_drivers += 'vendor_drivers (proprietary SDKs not configured)'
else
  disabled_drivers += 'vendor_drivers (proprietary SDK disabled by default)'
endif

# Bluetooth-dependent drivers (if missing)
bt_drivers = ['mooshimeter-dmm']
foreach driver : bt_drivers
  disabled_drivers += driver + ' (needs Bluetooth support - not implemented)'
endforeach
