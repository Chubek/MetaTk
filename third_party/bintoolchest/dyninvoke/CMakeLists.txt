cmake_minimum_required(VERSION 3.20)
project(TourchstoneDynInvoke VERSION 0.1.0 LANGUAGES C CXX ASM)

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
  get_filename_component(LIBSTONE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

option(DYNINVOKE_BUILD_TOOLS "Build the dinvoke command-line tool" "${LIBSTONE_BUILD_TOOLS}")
option(DYNINVOKE_BUILD_TESTS "Build dyninvoke tests" "${LIBSTONE_BUILD_TESTS}")

if(NOT TARGET qbfd::qbfd)
  if(NOT DEFINED QBFD_BUILD_TESTS)
    set(QBFD_BUILD_TESTS OFF CACHE BOOL "qBFD tests follow dyninvoke")
  endif()
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../yellowstone/qbfd"
                   "${CMAKE_CURRENT_BINARY_DIR}/yellowstone-qbfd")
endif()

set(DYNINVOKE_ABI_SOURCES "")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|x64")
  list(APPEND DYNINVOKE_ABI_SOURCES abi/win64.S)
  if(NOT WIN32)
    list(APPEND DYNINVOKE_ABI_SOURCES abi/amd64_sysv.S abi/closure_amd64_sysv.S)
  endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
  list(APPEND DYNINVOKE_ABI_SOURCES abi/aarch64.S)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  list(APPEND DYNINVOKE_ABI_SOURCES abi/riscv64.S)
endif()

add_library(tdyninvoke dyninvoke.cpp ${DYNINVOKE_ABI_SOURCES})
add_library(Tourchstone::dyninvoke ALIAS tdyninvoke)
target_compile_features(tdyninvoke PUBLIC cxx_std_20)
target_include_directories(tdyninvoke PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_include_directories(tdyninvoke PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../sandstone/isa)
target_link_libraries(tdyninvoke PUBLIC qbfd::qbfd)
target_compile_definitions(tdyninvoke PUBLIC
  $<$<BOOL:${BUILD_SHARED_LIBS}>:TDYNINVOKE_SHARED>)
set_target_properties(tdyninvoke PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(DEFINED LIBSTONE_SOURCE_DIR)
  target_compile_definitions(tdyninvoke PRIVATE
    DINV_ISA_DIR="${LIBSTONE_SOURCE_DIR}/sandstone/isa")
endif()
libstone_warnings(tdyninvoke)

if(DYNINVOKE_BUILD_TOOLS)
  if(NOT TARGET Tourchstone::dynload)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../dynload"
                     "${CMAKE_CURRENT_BINARY_DIR}/tourchstone-dynload")
  endif()
  add_executable(dinvoke_cli tools/dinvoke.cpp)
  set_target_properties(dinvoke_cli PROPERTIES OUTPUT_NAME dinvoke)
  target_compile_features(dinvoke_cli PRIVATE cxx_std_20)
  target_include_directories(dinvoke_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../dynload
    ${CMAKE_CURRENT_LIST_DIR}/../../yellowstone/common)
  target_link_libraries(dinvoke_cli PRIVATE
    Tourchstone::dyninvoke Tourchstone::dynload)
  if(DEFINED LIBSTONE_SOURCE_DIR)
    target_compile_definitions(dinvoke_cli PRIVATE
      DINV_ISA_DIR="${LIBSTONE_SOURCE_DIR}/sandstone/isa")
  endif()
  libstone_warnings(dinvoke_cli)
endif()

install(TARGETS tdyninvoke
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(TARGET dinvoke_cli)
  install(TARGETS dinvoke_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(FILES dyninvoke.h dyninvoke.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dyninvoke)

if(DYNINVOKE_BUILD_TESTS)
  enable_testing()
  add_library(dyninvoke_fixture SHARED tests/fixture.c)
  target_compile_definitions(dyninvoke_fixture PRIVATE DYNINVOKE_FIXTURE_BUILD)
  set_target_properties(dyninvoke_fixture PROPERTIES OUTPUT_NAME dyninvoke_fixture)

  add_executable(dyninvoke_execute_tests tests/test_dyninvoke.cpp)
  target_compile_features(dyninvoke_execute_tests PRIVATE cxx_std_20)
  target_link_libraries(dyninvoke_execute_tests PRIVATE Tourchstone::dyninvoke)
  if(CMAKE_DL_LIBS)
    target_link_libraries(dyninvoke_execute_tests PRIVATE ${CMAKE_DL_LIBS})
  endif()
  libstone_warnings(dyninvoke_execute_tests)

  add_executable(dyninvoke_layout_tests tests/test_layout.cpp)
  target_compile_features(dyninvoke_layout_tests PRIVATE cxx_std_20)
  target_link_libraries(dyninvoke_layout_tests PRIVATE Tourchstone::dyninvoke)
  libstone_warnings(dyninvoke_layout_tests)

  if(DEFINED LIBSTONE_SOURCE_DIR)
    target_compile_definitions(dyninvoke_execute_tests PRIVATE
      DINV_ISA_DIR="${LIBSTONE_SOURCE_DIR}/sandstone/isa")
    target_compile_definitions(dyninvoke_layout_tests PRIVATE
      DINV_ISA_DIR="${LIBSTONE_SOURCE_DIR}/sandstone/isa")
  endif()

  add_test(NAME dyninvoke.execute
    COMMAND dyninvoke_execute_tests $<TARGET_FILE:dyninvoke_fixture>)
  add_test(NAME dyninvoke.layout COMMAND dyninvoke_layout_tests)
  if(TARGET dinvoke_cli)
    add_test(NAME dyninvoke.cli-help COMMAND dinvoke_cli --help)
    add_test(NAME dyninvoke.cli-layout
      COMMAND dinvoke_cli --layout --abi sysv_amd64 --ret i32
              --arg i32:1 --arg i32:2
              $<TARGET_FILE:dyninvoke_fixture> dinvk_add2)
  endif()
endif()
