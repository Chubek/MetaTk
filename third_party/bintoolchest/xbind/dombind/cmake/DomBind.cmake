include_guard(GLOBAL)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

function(dombind_library language package)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(target "dom${language}bind")
  set(source "${CMAKE_CURRENT_SOURCE_DIR}")
  set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
  set(header "${CMAKE_CURRENT_BINARY_DIR}/include/${target}/capi.h")
  file(GLOB modules CONFIGURE_DEPENDS "${source}/*.q")
  file(GLOB compiler CONFIGURE_DEPENDS "${root}/../domqlib/q/*.py")
  add_custom_command(OUTPUT "${header}"
    COMMAND "${Python3_EXECUTABLE}" "${root}/generate.py" "${language}" "${header}"
    DEPENDS "${root}/generate.py" ${modules} ${compiler}
    VERBATIM)
  add_custom_target(${target}_headers ALL DEPENDS "${header}")
  add_library(${target} INTERFACE)
  set_target_properties(${target} PROPERTIES EXPORT_NAME ${package})
  add_library(${package}::${package} ALIAS ${target})
  add_dependencies(${target} ${target}_headers)
  target_compile_features(${target} INTERFACE c_std_99 cxx_std_17)
  target_include_directories(${target} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${source}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

  set(DOMBIND_DEPENDENCIES "")
  if(language STREQUAL "lua")
    find_package(Lua 5.4 REQUIRED)
    target_include_directories(${target} SYSTEM INTERFACE "$<BUILD_INTERFACE:${LUA_INCLUDE_DIR}>")
    target_link_libraries(${target} INTERFACE "$<BUILD_INTERFACE:${LUA_LIBRARIES}>")
    set(DOMBIND_DEPENDENCIES "find_dependency(Lua 5.4)\nset_property(TARGET ${package}::${package} APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES \"\$\{LUA_INCLUDE_DIR\}\")\nset_property(TARGET ${package}::${package} PROPERTY INTERFACE_LINK_LIBRARIES \"\$\{LUA_LIBRARIES\}\")")
  elseif(language STREQUAL "ruby")
    find_package(Ruby 3.0 REQUIRED)
    target_include_directories(${target} SYSTEM INTERFACE "$<BUILD_INTERFACE:${Ruby_INCLUDE_DIRS}>")
    target_link_libraries(${target} INTERFACE "$<BUILD_INTERFACE:${Ruby_LIBRARY}>")
    set(DOMBIND_DEPENDENCIES "find_dependency(Ruby 3.0)\nset_property(TARGET ${package}::${package} APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES \"\$\{Ruby_INCLUDE_DIRS\}\")\nset_property(TARGET ${package}::${package} PROPERTY INTERFACE_LINK_LIBRARIES \"\$\{Ruby_LIBRARY\}\")")
  elseif(language STREQUAL "java")
    find_package(JNI REQUIRED)
    target_include_directories(${target} SYSTEM INTERFACE "$<BUILD_INTERFACE:${JNI_INCLUDE_DIRS}>")
    target_link_libraries(${target} INTERFACE "$<BUILD_INTERFACE:${JNI_LIBRARIES}>")
    set(DOMBIND_DEPENDENCIES "find_dependency(JNI)\nset_property(TARGET ${package}::${package} APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES \"\$\{JNI_INCLUDE_DIRS\}\")\nset_property(TARGET ${package}::${package} PROPERTY INTERFACE_LINK_LIBRARIES \"\$\{JNI_LIBRARIES\}\")")
  elseif(language STREQUAL "node")
    find_path(DOMBIND_NODE_INCLUDE node_api.h PATH_SUFFIXES node REQUIRED)
    target_include_directories(${target} SYSTEM INTERFACE "$<BUILD_INTERFACE:${DOMBIND_NODE_INCLUDE}>")
    target_compile_definitions(${target} INTERFACE NAPI_VERSION=6)
    install(FILES "${root}/cmake/DomNodeBindModule.cmake" DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${package}")
    set(DOMBIND_DEPENDENCIES "find_path(DOMBIND_NODE_INCLUDE node_api.h PATH_SUFFIXES node REQUIRED)\nset_property(TARGET ${package}::${package} APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES \"\$\{DOMBIND_NODE_INCLUDE\}\")")
  elseif(language STREQUAL "ocaml")
    find_program(OCAMLC ocamlc REQUIRED)
    execute_process(COMMAND "${OCAMLC}" -where OUTPUT_VARIABLE OCAML_INCLUDE
      OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    target_include_directories(${target} SYSTEM INTERFACE "$<BUILD_INTERFACE:${OCAML_INCLUDE}>")
    set(DOMBIND_DEPENDENCIES "find_program(OCAMLC ocamlc REQUIRED)\nexecute_process(COMMAND \"\$\{OCAMLC\}\" -where OUTPUT_VARIABLE OCAML_INCLUDE OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)\nset_property(TARGET ${package}::${package} APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES \"\$\{OCAML_INCLUDE\}\")")
  endif()
  install(FILES "${header}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${target}")
  if(EXISTS "${source}/include")
    install(DIRECTORY "${source}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  endif()
  install(TARGETS ${target} EXPORT ${package}Targets)
  install(EXPORT ${package}Targets NAMESPACE ${package}:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${package}")
  if(language STREQUAL "node")
    string(APPEND DOMBIND_DEPENDENCIES "\ninclude(\"\$\{CMAKE_CURRENT_LIST_DIR\}/DomNodeBindModule.cmake\")")
  endif()
  set(DOMBIND_PACKAGE "${package}")
  configure_package_config_file("${root}/cmake/DomBindConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${package}Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${package}")
  write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/${package}ConfigVersion.cmake"
    VERSION 1.0.0 COMPATIBILITY SameMajorVersion)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${package}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${package}ConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${package}")
endfunction()

include("${CMAKE_CURRENT_LIST_DIR}/DomNodeBindModule.cmake")
