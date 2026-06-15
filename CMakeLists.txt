cmake_minimum_required(VERSION 3.16)
project(parzek VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

add_library(parzek STATIC Parzek.cpp)
target_include_directories(parzek PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

add_executable(parzek_cli main.cpp)
target_link_libraries(parzek_cli PRIVATE parzek)
set_target_properties(parzek_cli PROPERTIES OUTPUT_NAME parzek)

enable_testing()
find_package(Catch2 QUIET)
if(Catch2_FOUND)
  add_executable(test_parzek tests/test_parzek.cpp tests/test_parzek_features.cpp)
  target_link_libraries(test_parzek PRIVATE parzek Catch2::Catch2WithMain)
  add_test(NAME parzek-tests COMMAND test_parzek)
endif()

install(TARGETS parzek parzek_cli
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES Parzek.hpp DSLUtils.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dslutils.pc.in
               ${CMAKE_CURRENT_BINARY_DIR}/dslutils.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dslutils.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
