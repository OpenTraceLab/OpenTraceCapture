# C++ bindings for OpenTraceCapture

cpp = meson.get_compiler('cpp')

# Dependencies
glib_dep = dependency('glib-2.0', version: '>= 2.32.0')

# Try different versions of glibmm
glibmm_dep = dependency('glibmm-2.4', version: '>= 2.32.0', required: false)
if not glibmm_dep.found()
  glibmm_dep = dependency('glibmm-2.68', version: '>= 2.68.0', required: false)
endif
if not glibmm_dep.found()
  error('glibmm not found. Install libglibmm-2.4-dev or libglibmm-2.68-dev')
endif

# Include directories
inc_cxx = include_directories('include')
inc_root = include_directories('../..')

# Generate Doxygen XML documentation
doxygen = find_program('doxygen', required: true)
doxygen_xml = custom_target('doxygen_xml',
  output: 'xml',
  input: ['Doxyfile'],
  command: [doxygen, '@INPUT0@'],
  build_by_default: true,
  env: {'SRCDIR': meson.current_source_dir() + '/', 'BUILDDIR': meson.current_build_dir() + '/'}
)

# Generate enums.cpp
python3 = import('python').find_installation('python3')
enums_cpp = custom_target('enums_cpp',
  output: 'enums.cpp',
  input: ['enums.py'],
  depends: doxygen_xml,
  command: [python3, '@INPUT0@', meson.current_build_dir() + '/doxy/xml/index.xml'],
  capture: true
)

# Sources
cxx_sources = files(
  'classes.cpp'
)

# Library
libopentracecapturecxx = library('opentracecapturecxx',
  cxx_sources,
  dependencies: [glib_dep, glibmm_dep, all_deps],
  include_directories: [inc_cxx, inc_root, inc],
  link_with: lib,
  version: meson.project_version(),
  soversion: '0',
  install: true,
  override_options: ['cpp_std=c++17']
)

# Headers
install_headers('include/libopentracecapturecxx/libopentracecapturecxx.hpp')
install_headers('include/libopentracecapturecxx/enums.hpp')

# pkg-config
pkg = import('pkgconfig')
pkg.generate(
  name: 'libopentracecapturecxx',
  description: 'OpenTraceCapture C++ bindings',
  version: meson.project_version(),
  libraries: libopentracecapturecxx,
  subdirs: 'libopentracecapturecxx',
  requires: ['glib-2.0', 'glibmm-2.4']
)

# Dependency for other projects
libopentracecapturecxx_dep = declare_dependency(
  link_with: libopentracecapturecxx,
  include_directories: inc_cxx,
  dependencies: [glib_dep, glibmm_dep]
)
