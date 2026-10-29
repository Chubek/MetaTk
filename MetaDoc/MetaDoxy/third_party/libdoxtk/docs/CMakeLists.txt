find_package(Doxygen REQUIRED)
configure_file(Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
add_custom_target(doxtk_docs
  COMMAND Doxygen::doxygen ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Generating DoxTk manual and API reference"
  VERBATIM)
