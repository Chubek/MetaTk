cmake_minimum_required(VERSION 3.20)

project(DSLtk VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(dsltk INTERFACE)
target_include_directories(dsltk
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(dsltk INTERFACE cxx_std_20)
add_library(DSLtk::dsltk ALIAS dsltk)
add_library(DSLtk::DSLtk ALIAS dsltk)

set(DSLTK_PUBLIC_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/DSLtk.hpp
)

install(TARGETS dsltk EXPORT DSLtkTargets)
install(FILES ${DSLTK_PUBLIC_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DSLtk
)
install(FILES ${DSLTK_PUBLIC_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

set(DSLTK_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/DSLtk)

install(EXPORT DSLtkTargets
  FILE DSLtkTargets.cmake
  NAMESPACE DSLtk::
  DESTINATION ${DSLTK_INSTALL_CMAKEDIR}
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/DSLtkConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/DSLtkConfig.cmake
  INSTALL_DESTINATION ${DSLTK_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/DSLtkConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/DSLtkConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/DSLtkConfigVersion.cmake
  DESTINATION ${DSLTK_INSTALL_CMAKEDIR}
)

if(METATK_BUILD_EXAMPLES)
  foreach(example_source
    01-basic-usage.cpp
    02-fixed-strings.cpp
    03-pattern-simple.cpp
    04-pattern-classes.cpp
    05-parser-basics.cpp
    06-parser-composition.cpp
    07-parser-optional-repeat-choice.cpp
    08-ast-creation.cpp
    09-ast-traversal.cpp
    10-rewrite-basics.cpp
    11-rewrite-composition.cpp
    12-expression-templates.cpp
    13-evaluation-flow.cpp
    14-small-calculator-dsl.cpp
    15-tokenize-parse-integration.cpp
    16-diagnostics-errors.cpp
    17-compiletime-runtime-tradeoffs.cpp
    18-mini-dsl.cpp
  )
    get_filename_component(example_name ${example_source} NAME_WE)
    add_executable(dsltk-example-${example_name} examples/${example_source})
    target_link_libraries(dsltk-example-${example_name} PRIVATE dsltk)
  endforeach()
endif()

if(BUILD_TESTING)
  add_executable(dsltk-tests
    ../third_party/Catch2/extras/catch_amalgamated.cpp
  )
  target_include_directories(dsltk-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/Catch2/extras
  )
  target_link_libraries(dsltk-tests PRIVATE dsltk)
  add_test(NAME dsltk-tests COMMAND dsltk-tests)
endif()
