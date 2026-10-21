cmake_minimum_required(VERSION 3.20)
project(TourchstoneDynload VERSION 0.1.0 LANGUAGES C CXX)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../cmake/LibstoneBuild.cmake")
  include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/LibstoneBuild.cmake")
else()
  include(GNUInstallDirs)
  include(CTest)
  option(LIBSTONE_BUILD_TESTS "Build tests" "${BUILD_TESTING}")
  option(LIBSTONE_BUILD_TOOLS "Build tools" ON)
  option(LIBSTONE_ENABLE_WARNINGS "Enable compiler warnings" ON)
  function(libstone_warnings target)
    if(LIBSTONE_ENABLE_WARNINGS)
      target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANG_AND_ID:C,MSVC>:/W4>
        $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W4>
        $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>)
    endif()
  endfunction()
endif()

option(DYNLOAD_BUILD_TOOLS "Build the tdynload command-line tool" "${LIBSTONE_BUILD_TOOLS}")
option(DYNLOAD_BUILD_TESTS "Build dynload tests" "${LIBSTONE_BUILD_TESTS}")

if(NOT TARGET qbfd::qbfd)
  if(NOT DEFINED QBFD_BUILD_TESTS)
    set(QBFD_BUILD_TESTS "${DYNLOAD_BUILD_TESTS}" CACHE BOOL
      "Build qBFD regression tests")
  endif()
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../yellowstone/qbfd"
                   "${CMAKE_CURRENT_BINARY_DIR}/yellowstone-qbfd")
endif()

add_library(tdynload dynload.cpp)
add_library(Tourchstone::dynload ALIAS tdynload)
target_compile_features(tdynload PUBLIC cxx_std_20)
target_include_directories(tdynload PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(tdynload PRIVATE qbfd::qbfd)
target_compile_definitions(tdynload PUBLIC
  $<$<BOOL:${BUILD_SHARED_LIBS}>:TDYNLOAD_SHARED>)
if(CMAKE_DL_LIBS)
  target_link_libraries(tdynload PRIVATE ${CMAKE_DL_LIBS})
endif()
set_target_properties(tdynload PROPERTIES POSITION_INDEPENDENT_CODE ON)
libstone_warnings(tdynload)

if(DYNLOAD_BUILD_TOOLS)
  add_executable(tdynload_cli tools/tdynload.cpp)
  set_target_properties(tdynload_cli PROPERTIES OUTPUT_NAME tdynload)
  target_compile_features(tdynload_cli PRIVATE cxx_std_20)
  target_include_directories(tdynload_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../../yellowstone/common)
  target_link_libraries(tdynload_cli PRIVATE Tourchstone::dynload)
  libstone_warnings(tdynload_cli)
endif()

install(TARGETS tdynload
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(TARGET tdynload_cli)
  install(TARGETS tdynload_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(FILES dynload.h dynload.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dynload)

if(DYNLOAD_BUILD_TESTS)
  enable_testing()
  add_library(tdynload_fixture SHARED tests/fixture.c)
  target_compile_definitions(tdynload_fixture PRIVATE TDYNLOAD_FIXTURE_BUILD)
  set_target_properties(tdynload_fixture PROPERTIES OUTPUT_NAME tdynload_fixture)

  add_executable(tdynload_tests tests/test_dynload.cpp)
  target_compile_features(tdynload_tests PRIVATE cxx_std_20)
  target_link_libraries(tdynload_tests PRIVATE Tourchstone::dynload)
  libstone_warnings(tdynload_tests)

  add_test(NAME dynload.open
    COMMAND tdynload_tests $<TARGET_FILE:tdynload_fixture>)
  if(TARGET tdynload_cli)
    add_test(NAME dynload.cli-help COMMAND tdynload_cli --help)
    add_test(NAME dynload.cli-inspect
      COMMAND tdynload_cli --inspect --needed $<TARGET_FILE:tdynload_fixture>)
  endif()
endif()
