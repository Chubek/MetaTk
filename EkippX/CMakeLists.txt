cmake_minimum_required(VERSION 3.20)

project(EkippX VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ekippx INTERFACE)
target_include_directories(ekippx
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../DSLtk>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../third_party/Klyspec>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../third_party/SerdeTk/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../third_party/PikoRL>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../third_party/PikoRL/third_party>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(ekippx INTERFACE cxx_std_20)
add_library(EkippX::ekippx ALIAS ekippx)

add_executable(ekippx-cli EkippX-CLI.cpp KlyspecBridge.cpp PikoRLBridge.cpp)
target_link_libraries(ekippx-cli PRIVATE ekippx)
target_include_directories(ekippx-cli PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../DSLtk
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/Klyspec
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/SerdeTk/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/PikoRL
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/PikoRL/third_party
)

set(EKIPPX_PUBLIC_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/EkippX.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/EkippX-Batteries.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/EkippX-Plugin.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/EkippX-PluginAPI.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/EkippX.syntax
)

set(EKIPPX_PLUGIN_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/plugins/DateTimePlugin.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/plugins/EnvironmentPlugin.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/plugins/FilesystemPlugin.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/plugins/RandomPlugin.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/plugins/TextPlugin.hpp
)

install(TARGETS ekippx-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS ekippx EXPORT EkippXTargets)
install(FILES ${EKIPPX_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/EkippX)
install(FILES ${EKIPPX_PLUGIN_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/EkippX/plugins)
install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/../DSLtk/DSLtk.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DSLtk
)
install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/../DSLtk/DSLtk.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/SerdeTk/include/SerdeTk.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/PikoRL/PikoRL.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/PikoRL
)
install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/Klyspec/Klyspec.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/Klyspec/Klyspec-IPC.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/Klyspec/Klyspec-Plugin.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/Klyspec/Klyspec-Profiles.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/Klyspec/Klyspec-Subcommand.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Klyspec
)

set(EKIPPX_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/EkippX)

install(EXPORT EkippXTargets
  FILE EkippXTargets.cmake
  NAMESPACE EkippX::
  DESTINATION ${EKIPPX_INSTALL_CMAKEDIR}
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EkippXConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/EkippXConfig.cmake
  INSTALL_DESTINATION ${EKIPPX_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/EkippXConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/EkippXConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/EkippXConfigVersion.cmake
  DESTINATION ${EKIPPX_INSTALL_CMAKEDIR}
)

if(BUILD_TESTING)
  add_executable(ekippx-tests
    tests/test_ekippx.cpp
    ../third_party/Catch2/extras/catch_amalgamated.cpp
  )
  target_include_directories(ekippx-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../DSLtk
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/Catch2/extras
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/Klyspec
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/SerdeTk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/PikoRL
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/PikoRL/third_party
  )
  target_link_libraries(ekippx-tests PRIVATE ekippx)
  add_test(NAME ekippx-tests COMMAND ekippx-tests)
endif()
