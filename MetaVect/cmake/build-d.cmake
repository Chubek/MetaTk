# CMake does not ship a D language frontend. Compile these small source groups in
# one invocation, in the build tree, without introducing an external build tool.
#
# Contract: every argument is a repo-relative D source, and the first one must be a
# module with a main(). Each argument must be the only file defining its module.
function(metavect_d_executable name)
  set(sources)
  foreach(source IN LISTS ARGN)
    list(APPEND sources "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
  endforeach()
  get_filename_component(compiler_name "${METAVECT_D_COMPILER}" NAME)
  set(output "${CMAKE_CURRENT_BINARY_DIR}/${name}${CMAKE_EXECUTABLE_SUFFIX}")
  if(compiler_name MATCHES "gdc")
    set(output_flag -o "${output}")
  else()
    set(output_flag "-of=${output}")
  endif()
  add_custom_command(OUTPUT "${output}"
    COMMAND "${METAVECT_D_COMPILER}" "-I${CMAKE_CURRENT_SOURCE_DIR}" ${output_flag} ${sources}
    DEPENDS ${sources}
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Building ${name} with ${compiler_name}"
    VERBATIM)
  add_custom_target(${name}_d ALL DEPENDS "${output}")
endfunction()