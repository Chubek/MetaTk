cmake_minimum_required(VERSION 3.20)
project(XBind VERSION 0.1.0 LANGUAGES C CXX)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../cmake/LibstoneBuild.cmake")
  include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/LibstoneBuild.cmake")
else()
  include(GNUInstallDirs)
  include(CTest)
  option(LIBSTONE_BUILD_TESTS "Build tests" "${BUILD_TESTING}")
  option(LIBSTONE_ENABLE_WARNINGS "Enable compiler warnings" ON)
  function(libstone_warnings target)
    if(LIBSTONE_ENABLE_WARNINGS)
      target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANG_AND_ID:C,MSVC>:/W4>
        $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>)
    endif()
  endfunction()
endif()

option(XBIND_BUILD_TESTS "Build XBind contract and generated-binding tests" "${LIBSTONE_BUILD_TESTS}")
option(XBIND_BUILD_DOMBIND "Build the DomBind runtime subproject" OFF)
option(XBIND_BUILD_QAMRPP_BINDING "Build the QaMRpp Lua binding for XBind" ON)

include(cmake/XBindQaMRpp.cmake)

add_library(xbind xbind.c)
set_target_properties(xbind PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(XBind::xbind ALIAS xbind)
target_compile_features(xbind PUBLIC c_std_11)
target_include_directories(xbind PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/xbind>)
target_compile_definitions(xbind PRIVATE XBIND_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
libstone_warnings(xbind)

add_library(xbind_qamrpp STATIC xbind_qamrpp.cpp xbind_codegen.cpp)
set_target_properties(xbind_qamrpp PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(XBind::QaMRppSpec ALIAS xbind_qamrpp)
target_compile_features(xbind_qamrpp PUBLIC cxx_std_20)
target_include_directories(xbind_qamrpp PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/xbind>)
target_link_libraries(xbind_qamrpp PUBLIC XBind::xbind XBind::QaMRpp)
target_compile_definitions(xbind_qamrpp PRIVATE XBIND_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
libstone_warnings(xbind_qamrpp)

add_executable(xbind_cli tools/xbind_tool.cpp)
set_target_properties(xbind_cli PROPERTIES OUTPUT_NAME xbind)
target_compile_features(xbind_cli PRIVATE cxx_std_20)
target_include_directories(xbind_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(xbind_cli PRIVATE XBIND_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(xbind_cli PRIVATE XBind::xbind XBind::QaMRppSpec)
libstone_warnings(xbind_cli)

if(XBIND_BUILD_QAMRPP_BINDING)
  add_library(qamrpp_xbind MODULE xbind_qamrpp.cpp lxbind/qamrpp_xbind.cpp)
  # QaMRpp.hpp defines its C plugin API (non-inline). Keep its users in one
  # translation unit while sharing the exact constructor/validation source.
  set_target_properties(qamrpp_xbind PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 2)
  target_compile_features(qamrpp_xbind PRIVATE cxx_std_20)
  target_include_directories(qamrpp_xbind PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(qamrpp_xbind PRIVATE XBind::xbind XBind::QaMRpp)
  set_target_properties(qamrpp_xbind PROPERTIES
    OUTPUT_NAME qamrpp_xbind
    PREFIX "lib")
  target_compile_definitions(qamrpp_xbind PRIVATE XBIND_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  libstone_warnings(qamrpp_xbind)
endif()

if(XBIND_BUILD_DOMBIND)
  add_subdirectory(dombind)
endif()

install(TARGETS xbind xbind_qamrpp EXPORT XBindTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS xbind_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(TARGET qamrpp_xbind)
  install(TARGETS qamrpp_xbind LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/qamrpp)
endif()
install(FILES xbind.h xbind.hpp xbind_qamrpp.hpp xbind_codegen.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xbind)
install(DIRECTORY lxbind/ DESTINATION ${CMAKE_INSTALL_DATADIR}/xbind/lxbind)
install(DIRECTORY stdlib/ DESTINATION ${CMAKE_INSTALL_DATADIR}/xbind/stdlib)
install(DIRECTORY typedefs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/xbind/typedefs)
install(FILES XFeats.schema.json DESTINATION ${CMAKE_INSTALL_DATADIR}/xbind)

if(XBIND_BUILD_TESTS)
  enable_testing()
  find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
  add_executable(xbind_core_tests tests/test_core.c)
  target_link_libraries(xbind_core_tests PRIVATE XBind::xbind)
  libstone_warnings(xbind_core_tests)
  add_test(NAME xbind.core
    COMMAND xbind_core_tests ${CMAKE_CURRENT_SOURCE_DIR}/examples/hello)
  add_test(NAME xbind.layout
    COMMAND xbind_cli --check-layout ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME xbind.regimen
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/regimen.py
      --root ${CMAKE_CURRENT_SOURCE_DIR}
      --cli $<TARGET_FILE:xbind_cli>)
  add_test(NAME xbind.spec
    COMMAND xbind_cli --check-spec hello ${CMAKE_CURRENT_SOURCE_DIR}/examples/hello)
  add_test(NAME xbind.codegen
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_codegen.py
      --root ${CMAKE_CURRENT_SOURCE_DIR} --cli $<TARGET_FILE:xbind_cli>
      --cc ${CMAKE_C_COMPILER} --cxx ${CMAKE_CXX_COMPILER}
      --work ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(xbind.codegen PROPERTIES TIMEOUT 180)
  if(TARGET qamrpp_xbind)
    add_executable(xbind_qamrpp_module_test tests/qamrpp_module.cpp)
    target_link_libraries(xbind_qamrpp_module_test PRIVATE XBind::QaMRpp)
    target_compile_features(xbind_qamrpp_module_test PRIVATE cxx_std_20)
    libstone_warnings(xbind_qamrpp_module_test)
    add_test(NAME xbind.qamrpp-module
      COMMAND xbind_qamrpp_module_test
        $<TARGET_FILE:qamrpp_xbind>
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/hello/hello.xbind)
  endif()
  set_tests_properties(xbind.layout xbind.regimen xbind.spec PROPERTIES TIMEOUT 30)
  if(TEST xbind.qamrpp-module)
    set_tests_properties(xbind.qamrpp-module PROPERTIES TIMEOUT 30)
  endif()
endif()
