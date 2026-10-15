cmake_minimum_required(VERSION 3.20)
project(MetaVect VERSION 0.1.0 LANGUAGES C CXX)

option(METAVECT_BUILD_C_API "Build the small C API library" ON)
option(METAVECT_BUILD_TESTS "Build tests when available" ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(METAVECT_BUILD_C_API)
  add_library(metavect STATIC MetaVect.c)
  target_include_directories(metavect PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_compile_features(metavect PUBLIC c_std_11)
endif()

include(CTest)
if(BUILD_TESTING AND METAVECT_BUILD_TESTS)
  if(METAVECT_BUILD_C_API)
    add_executable(test_c_api tests/test_c_api.c)
    target_link_libraries(test_c_api PRIVATE metavect)
    add_test(NAME c_api_contract COMMAND test_c_api)
  endif()
  add_test(NAME source_tree_has_readme
    COMMAND ${CMAKE_COMMAND} -E env "METAVECT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-source.cmake")
endif()
