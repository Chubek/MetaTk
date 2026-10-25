if(NOT EXISTS "$ENV{METAVECT_SOURCE_DIR}/README.md")
  message(FATAL_ERROR "README.md is missing")
endif()

# Guard the D module layout discovered while implementing the syntax frontend:
# D emits no code for an interface (.di) module, and when a module has both a
# .d and a .di the compiler silently uses the .d and ignores the .di. A stale
# .di then looks like the contract while never being compiled.
file(GLOB d_sources "$ENV{METAVECT_SOURCE_DIR}/*.d" "$ENV{METAVECT_SOURCE_DIR}/*/*.d")
foreach(d_source IN LISTS d_sources)
  get_filename_component(base "${d_source}" NAME_WE)
  get_filename_component(directory "${d_source}" DIRECTORY)
  if(EXISTS "${directory}/${base}.di")
    message(FATAL_ERROR
      "${d_source} and ${directory}/${base}.di define the same module. "
      "Keep exactly one file per D module (see frontend/README.md).")
  endif()
endforeach()

message(STATUS "MetaVect source tree check passed")