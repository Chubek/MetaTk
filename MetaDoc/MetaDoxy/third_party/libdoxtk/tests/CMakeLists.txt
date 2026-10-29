add_executable(doxtk_tests test_core.cpp)
target_link_libraries(doxtk_tests PRIVATE DoxTk::DoxTk)
add_test(NAME doxtk_core COMMAND doxtk_tests)

foreach(suite IN ITEMS pipeline io markdown)
  add_executable(doxtk_test_${suite} test_${suite}.cpp)
  target_link_libraries(doxtk_test_${suite} PRIVATE DoxTk::DoxTk)
  add_test(NAME doxtk_${suite} COMMAND doxtk_test_${suite} ${CMAKE_CURRENT_BINARY_DIR}/${suite}-work)
  if(suite STREQUAL "markdown")
    set_tests_properties(doxtk_markdown PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/markdown)
  endif()
endforeach()

# Each public header is compiled in its own translation unit, without an umbrella
# header masking missing includes. All generated files stay under the build tree.
file(GLOB_RECURSE public_headers CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/include/*.h ${PROJECT_SOURCE_DIR}/include/*.hpp)
set(header_checks)
foreach(header IN LISTS public_headers)
  file(RELATIVE_PATH relative ${PROJECT_SOURCE_DIR}/include ${header})
  string(MAKE_C_IDENTIFIER "${relative}" identifier)
  set(check ${CMAKE_CURRENT_BINARY_DIR}/headers/${identifier}.cpp)
  file(WRITE ${check} "#include \"${relative}\"\n")
  list(APPEND header_checks ${check})
endforeach()
add_library(doxtk_header_checks OBJECT ${header_checks})
target_link_libraries(doxtk_header_checks PRIVATE DoxTk::DoxTk)
if(DOXTK_ENABLE_CPP_DSL)
  add_executable(doxtk_test_cppdsl test_cppdsl.cpp)
  target_link_libraries(doxtk_test_cppdsl PRIVATE DoxTk::CPPDSL)
  add_test(NAME doxtk_cppdsl COMMAND doxtk_test_cppdsl)
  add_test(NAME doxtk_cppdsl_example COMMAND doxtk_cppdsl_example)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/headers/cppdsl_enabled.cpp "#include <doxtk/DoxTk-CPPDSL.hpp>\n")
  add_library(doxtk_cppdsl_header_check OBJECT ${CMAKE_CURRENT_BINARY_DIR}/headers/cppdsl_enabled.cpp)
  target_link_libraries(doxtk_cppdsl_header_check PRIVATE DoxTk::CPPDSL)
endif()

if(DOXTK_BUILD_TOOLS)
  add_test(NAME doxtk_cli COMMAND ${CMAKE_COMMAND}
    -DCONVERTER=$<TARGET_FILE:doxtk-convert>
    -DJSON_TOOL=$<TARGET_FILE:dump2json>
    -DXML_TOOL=$<TARGET_FILE:dump2xml>
    -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli-work
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test_cli.cmake)
  foreach(tool IN ITEMS any2dvi any2pdf any2ps dump2xtas html2pdf markdown2pdf texi2pdf)
    add_test(NAME ${tool}_help COMMAND ${tool} --help)
    set_tests_properties(${tool}_help PROPERTIES PASS_REGULAR_EXPRESSION "not implemented")
    add_test(NAME ${tool}_unsupported COMMAND ${tool})
    set_tests_properties(${tool}_unsupported PROPERTIES WILL_FAIL TRUE)
  endforeach()
endif()
