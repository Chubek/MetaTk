cmake_minimum_required(VERSION 3.20)

project(MetaTk VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(METATK_ENABLE_DSLTK "Configure the DSLtk package" ON)
option(METATK_ENABLE_PARZEK "Configure the Parzek package" ON)
option(METATK_ENABLE_EKIPPX "Configure the EkippX package" ON)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  include(CTest)
  option(METATK_BUILD_EXAMPLES "Build MetaTk examples" ON)
else()
  option(METATK_BUILD_EXAMPLES "Build MetaTk examples" OFF)
  if(NOT DEFINED BUILD_TESTING)
    set(BUILD_TESTING OFF CACHE BOOL "Build the testing tree." FORCE)
  endif()
endif()

if(METATK_ENABLE_EKIPPX AND NOT METATK_ENABLE_DSLTK)
  message(FATAL_ERROR "EkippX requires METATK_ENABLE_DSLTK=ON.")
endif()

if(METATK_ENABLE_PARZEK AND NOT METATK_ENABLE_DSLTK)
  message(FATAL_ERROR "Parzek requires METATK_ENABLE_DSLTK=ON.")
endif()

if(METATK_ENABLE_DSLTK)
  add_subdirectory(DSLtk)
endif()

if(METATK_ENABLE_PARZEK)
  add_subdirectory(Parzek)
endif()

if(METATK_ENABLE_EKIPPX)
  add_subdirectory(EkippX)
endif()

set(METATK_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/MetaTk)

set(METATK_AVAILABLE_COMPONENTS)
if(METATK_ENABLE_DSLTK)
  list(APPEND METATK_AVAILABLE_COMPONENTS DSLtk)
endif()
if(METATK_ENABLE_PARZEK)
  list(APPEND METATK_AVAILABLE_COMPONENTS Parzek)
endif()
if(METATK_ENABLE_EKIPPX)
  list(APPEND METATK_AVAILABLE_COMPONENTS EkippX)
endif()

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MetaTkConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/MetaTkConfig.cmake
  INSTALL_DESTINATION ${METATK_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/MetaTkConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/MetaTkConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/MetaTkConfigVersion.cmake
  DESTINATION ${METATK_INSTALL_CMAKEDIR}
)
