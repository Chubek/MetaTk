# Extra @include files must be supplied as DEPENDS so build systems track them.
# This is a build-time tool; cross builds need a host-built preprocessor.
function(metajit_preprocess_source output_variable input)
  cmake_parse_arguments(MJ "" "" "DEPENDS" ${ARGN})
  if(MJ_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "Unknown metajit_preprocess_source arguments: ${MJ_UNPARSED_ARGUMENTS}")
  endif()
  get_filename_component(source "${input}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  get_filename_component(name "${input}" NAME_WE)
  string(SHA256 source_hash "${source}")
  string(SUBSTRING "${source_hash}" 0 12 source_hash)
  set(generated "${CMAKE_CURRENT_BINARY_DIR}/${name}-${source_hash}.metajit.cpp")
  add_custom_command(OUTPUT "${generated}"
    COMMAND metajit-preprocess -o "${generated}" "${source}"
    DEPENDS metajit-preprocess "${source}" ${MJ_DEPENDS}
    VERBATIM)
  set(${output_variable} "${generated}" PARENT_SCOPE)
endfunction()
