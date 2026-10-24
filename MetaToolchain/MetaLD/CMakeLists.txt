# Compile only the generated linker core against the complete MetaArch ABI.
# Never pull ArchLD's JSON satellite or its duplicate ArchBFD into this graph.
add_library(metald MetaLD.c ../ArchLD/src/ArchLD.c)
add_library(MetaLD::metald ALIAS metald)
target_include_directories(metald PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/metaarch>)
target_link_libraries(metald PRIVATE MetaArch::metaarch)
target_compile_definitions(metald PRIVATE _POSIX_C_SOURCE=200809L
  METALD_SOURCE_CATALOGUE="${PROJECT_SOURCE_DIR}/ArchBFDCatalogue.yaml"
  METALD_INSTALL_CATALOGUE="${CMAKE_INSTALL_FULL_DATADIR}/metaarch/ArchBFDCatalogue.yaml")
set_target_properties(metald PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(metald PRIVATE -Wall -Wextra -Wpedantic)
endif()
install(TARGETS metald ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES MetaLD.h MetaLD.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/metaarch/MetaLD)
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DOCDIR}/MetaLD)

if(ARCHDB_BUILD_TESTS AND ARCHDB_BUILD_DSL)
  # Exercise the full-runtime build with the SAME real-ELF and corruption suites
  # as standalone ArchLD, including its inspection tools and concurrent API test.
  add_executable(metald_archtool ../ArchLD/src/Tools.cpp)
  target_include_directories(metald_archtool PRIVATE "${ARCHDB_DSLTK_DIR}" "${PROJECT_SOURCE_DIR}/ArchLD/src")
  target_link_libraries(metald_archtool PRIVATE metald MetaArch::metaarch)
  set_target_properties(metald_archtool PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
  foreach(tool ArchTool ArchLD ArchNm ArchSize ArchSections ArchRelocs ArchTargets ArchSpec ArchLex)
    add_custom_command(TARGET metald_archtool POST_BUILD COMMAND "${CMAKE_COMMAND}" -E create_symlink
      $<TARGET_FILE_NAME:metald_archtool> "${CMAKE_CURRENT_BINARY_DIR}/bin/${tool}")
  endforeach()
  file(COPY "${PROJECT_SOURCE_DIR}/ArchLD/targets/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/targets")
  find_package(Threads REQUIRED)
  add_executable(metald_core_test ../ArchLD/tests/test_api.c)
  target_include_directories(metald_core_test PRIVATE "${PROJECT_SOURCE_DIR}/ArchLD/src")
  target_compile_options(metald_core_test PRIVATE -UNDEBUG)
  target_link_libraries(metald_core_test PRIVATE metald MetaArch::metaarch Threads::Threads)
  if(Python3_Interpreter_FOUND)
    foreach(suite run tools hardening)
      add_test(NAME metald_core_${suite} COMMAND "${CMAKE_COMMAND}" -E env
        "ARCHLD_BIN=${CMAKE_CURRENT_BINARY_DIR}/bin" "ARCHLD_API_TEST=$<TARGET_FILE:metald_core_test>"
        "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/ArchLD/tests/${suite}.py")
    endforeach()
    find_program(CTANGLE_EXECUTABLE ctangle)
    if(CTANGLE_EXECUTABLE)
      add_test(NAME metald_generated COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/ArchLD/tests/check_generated.py")
    endif()
  endif()
endif()
