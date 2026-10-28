include_guard(GLOBAL)

# juniper_compile(name INPUT file OUTPUT file MODE emit|metail|optimize|select|schedule
#                 [INCLUDE_DIRECTORIES ...] [DEFINES NAME=value ...] [DEPENDS ...])
# Dependencies from the real macro expansion are supplied to Make/Ninja through
# a depfile, including conditional and nested includes and the standard library.
function(juniper_compile name)
  cmake_parse_arguments(J "" "INPUT;OUTPUT;MODE" "INCLUDE_DIRECTORIES;DEFINES;DEPENDS" ${ARGN})
  if(J_UNPARSED_ARGUMENTS OR NOT J_INPUT OR NOT J_OUTPUT)
    message(FATAL_ERROR "juniper_compile requires INPUT and OUTPUT; unknown arguments: ${J_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT J_MODE)
    set(J_MODE emit)
  endif()
  if(NOT J_MODE MATCHES "^(emit|metail|optimize|select|schedule)$")
    message(FATAL_ERROR "Unknown Juniper output mode: ${J_MODE}")
  endif()
  get_filename_component(input "${J_INPUT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  get_filename_component(output "${J_OUTPUT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
  get_filename_component(output_directory "${output}" DIRECTORY)
  set(arguments)
  foreach(directory IN LISTS J_INCLUDE_DIRECTORIES)
    get_filename_component(directory "${directory}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    list(APPEND arguments -I "${directory}")
  endforeach()
  foreach(definition IN LISTS J_DEFINES)
    list(APPEND arguments -D "${definition}")
  endforeach()
  add_custom_command(OUTPUT "${output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_directory}"
    COMMAND Juniper::juniper "${J_MODE}" "${input}" -o "${output}"
      --depfile "${output}.d" ${arguments}
    DEPENDS Juniper::juniper "${input}" ${J_DEPENDS}
    DEPFILE "${output}.d" VERBATIM COMMENT "Juniper ${J_MODE}: ${J_INPUT}")
  add_custom_target(${name} DEPENDS "${output}")
endfunction()
