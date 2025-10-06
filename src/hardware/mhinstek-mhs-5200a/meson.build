mhinstek_mhs_5200a_sources = files([
  'api.c',
  'protocol.c',
])

mhinstek_mhs_5200a_lib = static_library('mhinstek-mhs-5200a',
  mhinstek_mhs_5200a_sources,
  include_directories: [inc, include_directories('../..')],
  c_args: compile_args,
)
