rohde_schwarz_nrpxsn_sources = files([
  'api.c',
  'protocol.c',
])

rohde_schwarz_nrpxsn_lib = static_library('rohde-schwarz-nrpxsn',
  rohde_schwarz_nrpxsn_sources,
  include_directories: [inc, include_directories('../..')],
  c_args: compile_args,
)
