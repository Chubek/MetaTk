cmake_minimum_required(VERSION 3.20)
project(MetaVect VERSION 0.1.0 LANGUAGES C CXX)

option(METAVECT_BUILD_C_API "Build the small C API library" ON)
option(METAVECT_BUILD_TESTS "Build tests when available" ON)
option(METAVECT_BUILD_FRONTEND "Build the D syntax frontend when a D compiler is available" ON)
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
if(METAVECT_BUILD_FRONTEND)
  find_program(METAVECT_D_COMPILER NAMES ldc2 dmd gdc DOC "D compiler for the syntax frontend")
  if(METAVECT_D_COMPILER)
    # One file per D module. An interface (.di) module is never compiled here:
    # D emits no code for it, and a sibling .d would make it dead weight.
    include(cmake/build-d.cmake)
    set(METAVECT_FRONTEND_SOURCES
      MetaVect.d frontend/Input.d frontend/Token.d frontend/SyntaxError.d
      frontend/Lexer.d frontend/AST.d frontend/Parser.d frontend/Symtbl.d
      frontend/Semacheck-Visitor.d)
    metavect_d_executable(mvect driver/mvect-cli.d driver/mvect.d ${METAVECT_FRONTEND_SOURCES})
    if(BUILD_TESTING AND METAVECT_BUILD_TESTS)
      metavect_d_executable(test_frontend tests/test_frontend.d driver/mvect.d ${METAVECT_FRONTEND_SOURCES})
      add_test(NAME frontend_contract COMMAND "${CMAKE_CURRENT_BINARY_DIR}/test_frontend${CMAKE_EXECUTABLE_SUFFIX}"
        "${CMAKE_CURRENT_SOURCE_DIR}")
      add_test(NAME frontend_cli COMMAND ${CMAKE_COMMAND}
        "-DMVECT=${CMAKE_CURRENT_BINARY_DIR}/mvect${CMAKE_EXECUTABLE_SUFFIX}"
        "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-cli.cmake")
    endif()
  else()
    message(STATUS "No D compiler found; skipping mvect and frontend tests (C API is still available)")
  endif()
endif()

if(BUILD_TESTING AND METAVECT_BUILD_TESTS)
  if(METAVECT_BUILD_C_API)
    add_executable(test_c_api tests/test_c_api.c)
    target_link_libraries(test_c_api PRIVATE metavect)
    add_test(NAME c_api_contract COMMAND test_c_api)
  endif()
  add_test(NAME source_tree_contract
    COMMAND ${CMAKE_COMMAND} -E env "METAVECT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-source.cmake")
  find_program(METAVECT_VIM_EXECUTABLE NAMES vim)
  if(METAVECT_VIM_EXECUTABLE)
    add_test(NAME editor_vim_syntax COMMAND ${CMAKE_COMMAND} -E env
      "METAVECT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      "${METAVECT_VIM_EXECUTABLE}" -Nu NONE -n -i NONE -V1 -es
      -S "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-vim.vim")
    add_test(NAME editor_vim_indent COMMAND ${CMAKE_COMMAND} -E env
      "METAVECT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      "${METAVECT_VIM_EXECUTABLE}" -Nu NONE -n -i NONE -V1 -es
      -S "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-vim-indent.vim")
  endif()
  find_program(METAVECT_PYTHON_EXECUTABLE NAMES python3)
  if(METAVECT_PYTHON_EXECUTABLE)
    add_test(NAME editor_vim_static
      COMMAND "${METAVECT_PYTHON_EXECUTABLE}"
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-vim.py"
              "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME editor_sublime
      COMMAND "${METAVECT_PYTHON_EXECUTABLE}"
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-sublime.py"
              "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME document_tex
      COMMAND "${METAVECT_PYTHON_EXECUTABLE}"
              "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-tex.py"
              "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
endif()
