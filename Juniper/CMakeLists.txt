cmake_minimum_required(VERSION 3.24)
project(Juniper VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
option(JUNIPER_BUILD_TESTS "Build Juniper tests" ${PROJECT_IS_TOP_LEVEL})
option(JUNIPER_BUILD_EXAMPLES "Build the native profile example" ${PROJECT_IS_TOP_LEVEL})
option(JUNIPER_WITH_METAARCH "Build the checked MetaArch encoding adapter" OFF)
set(JUNIPER_GPPX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../GPPX" CACHE PATH "GPPX source checkout")
set(JUNIPER_METAIL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../MetaToolchain/MetaIL" CACHE PATH "MetaIL source checkout")
set(JUNIPER_METAARCH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../MetaToolchain" CACHE PATH "MetaArch source checkout")

if(NOT UNIX)
  message(FATAL_ERROR "Juniper's bounded GPPX process driver currently requires POSIX")
endif()
if(NOT TARGET GPPX::preprocessor)
  find_package(GPPXPreprocessor CONFIG QUIET)
  if(NOT TARGET GPPX::preprocessor)
    add_subdirectory("${JUNIPER_GPPX_ROOT}" gppx)
  endif()
endif()
if(NOT TARGET MetaIL::MetaIL)
  find_package(MetaIL CONFIG QUIET)
  if(NOT TARGET MetaIL::MetaIL)
    set(METAIL_BUILD_EXAMPLES OFF)
    if(NOT DEFINED METAIL_BUILD_TESTS)
      set(METAIL_BUILD_TESTS ${JUNIPER_BUILD_TESTS})
    endif()
    # Build the enabled dependency tests as well, so registered CTest cases are
    # never left referring to executables excluded from the normal build.
    add_subdirectory("${JUNIPER_METAIL_ROOT}" metail)
  endif()
endif()

add_library(juniper_core src/IR.cpp src/Frontend.cpp src/Process.cpp src/Selection.cpp src/Schedule.cpp)
add_library(Juniper::Juniper ALIAS juniper_core)
set_target_properties(juniper_core PROPERTIES EXPORT_NAME Juniper CXX_EXTENSIONS NO
  OUTPUT_NAME Juniper VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
target_compile_features(juniper_core PUBLIC cxx_std_20)
target_include_directories(juniper_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(juniper_core PUBLIC MetaIL::MetaIL)
target_compile_definitions(juniper_core PRIVATE
  JUNIPER_BUILD_GPPX="$<TARGET_FILE:GPPX::preprocessor>"
  JUNIPER_SOURCE_STDLIB="${CMAKE_CURRENT_SOURCE_DIR}/stdlib/Juniper.xpp"
  JUNIPER_INSTALL_GPPX="${CMAKE_INSTALL_FULL_BINDIR}/gppx"
  JUNIPER_INSTALL_STDLIB="${CMAKE_INSTALL_FULL_DATADIR}/juniper/stdlib/Juniper.xpp")
add_dependencies(juniper_core GPPX::preprocessor)
add_executable(juniper tools/juniper.cpp)
add_executable(Juniper::juniper ALIAS juniper)
target_link_libraries(juniper PRIVATE Juniper::Juniper)
file(RELATIVE_PATH JUNIPER_BIN_TO_LIB "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_LIBDIR}")
if(APPLE)
  set_target_properties(juniper PROPERTIES INSTALL_RPATH "@loader_path/${JUNIPER_BIN_TO_LIB}")
  set_target_properties(juniper_core PROPERTIES INSTALL_RPATH "@loader_path")
elseif(UNIX)
  set_target_properties(juniper PROPERTIES INSTALL_RPATH "$ORIGIN/${JUNIPER_BIN_TO_LIB}")
  set_target_properties(juniper_core PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(juniper_core PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(juniper PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(JUNIPER_WITH_METAARCH)
  if(NOT TARGET MetaArch::dsl)
    find_package(MetaArch CONFIG QUIET)
  endif()
  if(NOT TARGET MetaArch::dsl)
    set(ARCHDB_BUILD_DSL ON)
    set(ARCHDB_BUILD_TESTS OFF)
    set(ARCHDB_BUILD_TOOLS OFF)
    set(ARCHDB_BUILD_ARCHAS OFF)
    set(ARCHDB_BUILD_METAAS OFF)
    set(ARCHDB_BUILD_METALD OFF)
    set(ARCHDB_BUILD_METAJIT OFF)
    add_subdirectory("${JUNIPER_METAARCH_ROOT}" metaarch EXCLUDE_FROM_ALL)
  endif()
  add_library(juniper_metaarch INTERFACE)
  add_library(Juniper::MetaArch ALIAS juniper_metaarch)
  target_link_libraries(juniper_metaarch INTERFACE Juniper::Juniper MetaArch::dsl)
  # MetaArch's current DSL target does not publish its header directory.
  target_include_directories(juniper_metaarch INTERFACE
    $<BUILD_INTERFACE:${JUNIPER_METAARCH_ROOT}>)
endif()

include(cmake/JuniperCompile.cmake)
if(JUNIPER_BUILD_TESTS)
  enable_testing()
  add_executable(juniper_test tests/test_juniper.cpp)
  target_link_libraries(juniper_test PRIVATE Juniper::Juniper)
  add_test(NAME juniper-core COMMAND juniper_test "${CMAKE_CURRENT_SOURCE_DIR}")
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_test(NAME juniper-cli COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.py" $<TARGET_FILE:juniper>
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/cli-test")
  add_test(NAME juniper-cmake COMMAND ${CMAKE_COMMAND}
    -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cmake-test
    -DTOOL=$<TARGET_FILE:juniper> -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake.cmake")
  if(JUNIPER_WITH_METAARCH)
    add_executable(juniper_test_metaarch tests/test_metaarch.cpp)
    target_link_libraries(juniper_test_metaarch PRIVATE Juniper::MetaArch)
    add_test(NAME juniper-metaarch COMMAND juniper_test_metaarch)
  endif()
endif()
if(JUNIPER_BUILD_EXAMPLES)
  add_executable(juniper_profile_example examples/profile.cpp)
  target_link_libraries(juniper_profile_example PRIVATE Juniper::Juniper)
  juniper_compile(juniper_example_ir INPUT "${CMAKE_CURRENT_SOURCE_DIR}/examples/Juniperfile"
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/example.junir" MODE emit)
  add_custom_target(juniper-example ALL DEPENDS juniper_example_ir)
  if(JUNIPER_BUILD_TESTS)
    add_test(NAME juniper-profile COMMAND juniper_profile_example)
  endif()
endif()

install(TARGETS juniper juniper_core EXPORT JuniperTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY stdlib/ DESTINATION ${CMAKE_INSTALL_DATADIR}/juniper/stdlib)
install(DIRECTORY examples/ DESTINATION ${CMAKE_INSTALL_DATADIR}/juniper/examples)
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/juniper)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/juniper/docs)
install(EXPORT JuniperTargets NAMESPACE Juniper:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Juniper)
configure_package_config_file(cmake/JuniperConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/JuniperConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Juniper)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/JuniperConfigVersion.cmake"
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/JuniperConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/JuniperConfigVersion.cmake" cmake/JuniperCompile.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Juniper)
