cmake_minimum_required(VERSION 3.20)

project(Parzek VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT TARGET DSLtk::dsltk)
  add_subdirectory(../DSLtk ${CMAKE_CURRENT_BINARY_DIR}/_deps/DSLtk)
endif()

add_library(parzek STATIC Parzek.cpp)
target_include_directories(parzek
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(parzek PUBLIC DSLtk::dsltk)
target_compile_features(parzek PUBLIC cxx_std_20)
add_library(Parzek::parzek ALIAS parzek)
add_library(Parzek::Parzek ALIAS parzek)

add_executable(parzek-cli main.cpp)
target_link_libraries(parzek-cli PRIVATE parzek)

set(PARZEK_PUBLIC_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/Parzek.hpp
)

install(TARGETS parzek
  EXPORT ParzekTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(TARGETS parzek-cli
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES ${PARZEK_PUBLIC_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Parzek
)
install(FILES ${PARZEK_PUBLIC_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

set(PARZEK_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/Parzek)

install(EXPORT ParzekTargets
  FILE ParzekTargets.cmake
  NAMESPACE Parzek::
  DESTINATION ${PARZEK_INSTALL_CMAKEDIR}
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ParzekConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ParzekConfig.cmake
  INSTALL_DESTINATION ${PARZEK_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/ParzekConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/ParzekConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/ParzekConfigVersion.cmake
  DESTINATION ${PARZEK_INSTALL_CMAKEDIR}
)

if(BUILD_TESTING)
  add_executable(parzek-tests
    tests/test_parzek.cpp
    tests/test_parzek_features.cpp
    tests/unit/test_parser.cpp
    ../third_party/Catch2/extras/catch_amalgamated.cpp
  )
  target_include_directories(parzek-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/Catch2/extras
  )
  target_link_libraries(parzek-tests PRIVATE parzek)
  add_test(NAME parzek-tests COMMAND parzek-tests)

endif()
