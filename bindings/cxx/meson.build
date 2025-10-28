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
inc_cxx_build = include_directories('.')  # Build directory for generated enums.hpp
inc_root = include_directories('../..')

# Generate Doxygen XML documentation
doxygen = find_program('doxygen', required: true)
run_doxygen = find_program('run-doxygen.sh')
doxygen_xml = custom_target('doxygen_xml',
  output: 'doxy.stamp',
  input: ['Doxyfile'],
  command: [run_doxygen, '@INPUT0@', '@OUTPUT@'],
  build_by_default: true,
  build_always_stale: true,
  env: {'SRCDIR': meson.current_source_dir() + '/', 'BUILDDIR': meson.current_build_dir() + '/'}
)

# Generate enums.cpp (enums.py also generates enums.hpp and swig files)
python3 = import('python').find_installation('python3')
generate_enums_sh = find_program('generate-enums.sh')
enums_gen = custom_target('enums_gen',
  output: ['enums.cpp', 'enums.hpp'],
  input: ['enums.py'],
  depends: doxygen_xml,
  command: [generate_enums_sh, python3, '@INPUT0@', meson.current_build_dir() + '/doxy/xml/index.xml', meson.current_build_dir(), '@OUTPUT0@', '@OUTPUT1@']
)

# Install the generated enums.hpp (generated to source dir)
meson.add_install_script('sh', '-c', 
  'mkdir -p $MESON_INSTALL_DESTDIR_PREFIX/' + get_option('includedir') + '/libopentracecapturecxx && ' +
  'cp ' + meson.current_source_dir() + '/include/libopentracecapturecxx/enums.hpp ' +
  '$MESON_INSTALL_DESTDIR_PREFIX/' + get_option('includedir') + '/libopentracecapturecxx/ && ' +
  'cp ' + meson.current_source_dir() + '/include/libopentracecapturecxx/libopentracecapturecxx.hpp ' +
  '$MESON_INSTALL_DESTDIR_PREFIX/' + get_option('includedir') + '/libopentracecapturecxx/'
)

# Sources
cxx_sources = files(
  'classes.cpp'
)

# Library
libopentracecapturecxx = library('opentracecapturecxx',
  cxx_sources,
  dependencies: [glib_dep, glibmm_dep, all_deps, declare_dependency(sources: enums_gen)],
  include_directories: [inc_cxx, inc_cxx_build, inc_root, inc],
  link_with: lib,
  version: meson.project_version(),
  soversion: '0',
  install: true,
  override_options: ['cpp_std=c++17']
)

# pkg-config
pkg = import('pkgconfig')
pkg.generate(
  name: 'libopentracecapturecxx',
  description: 'OpenTraceCapture C++ bindings',
  version: meson.project_version(),
  libraries: libopentracecapturecxx,
  subdirs: 'libopentracecapturecxx',
  requires: ['glib-2.0', glibmm_dep]
)

# Dependency for other projects
libopentracecapturecxx_dep = declare_dependency(
  link_with: libopentracecapturecxx,
  include_directories: inc_cxx,
  dependencies: [glib_dep, glibmm_dep]
)
