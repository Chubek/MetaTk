include(GNUInstallDirs)

find_package(Doxygen QUIET)
find_program(MAKE_EXECUTABLE NAMES gmake make)

set(PARZEK_DOC_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PARZEK_DOC_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(PARZEK_DOXYFILE ${PARZEK_DOC_BINARY_DIR}/Doxyfile)

configure_file(${PARZEK_DOC_SOURCE_DIR}/Doxyfile.in ${PARZEK_DOXYFILE} @ONLY)
configure_file(${PARZEK_DOC_SOURCE_DIR}/conf.py.in ${PARZEK_DOC_BINARY_DIR}/conf.py @ONLY)
configure_file(${PARZEK_DOC_SOURCE_DIR}/index.rst.in ${PARZEK_DOC_BINARY_DIR}/index.rst @ONLY)
file(COPY ${PARZEK_DOC_SOURCE_DIR}/frontpage.md ${PARZEK_DOC_SOURCE_DIR}/manual DESTINATION ${PARZEK_DOC_BINARY_DIR})

set(PARZEK_DOC_STAMP ${PARZEK_DOC_BINARY_DIR}/parzek-docs.stamp)

if(DOXYGEN_FOUND)
  add_custom_command(
    OUTPUT ${PARZEK_DOC_STAMP}
    COMMAND ${DOXYGEN_EXECUTABLE} ${PARZEK_DOXYFILE}
    COMMAND ${CMAKE_COMMAND} -E touch ${PARZEK_DOC_STAMP}
    DEPENDS ${PARZEK_DOXYFILE} ${PARZEK_DOC_SOURCE_DIR}/frontpage.md
    WORKING_DIRECTORY ${PARZEK_DOC_BINARY_DIR}
    COMMENT "Generating Parzek Doxygen HTML, LaTeX, and XML"
    VERBATIM
  )
  add_custom_target(parzek-doxygen ALL DEPENDS ${PARZEK_DOC_STAMP})
  add_custom_target(parzek-docs-html ALL DEPENDS parzek-doxygen)
  add_custom_target(parzek-docs-latex ALL DEPENDS parzek-doxygen)
  if(MAKE_EXECUTABLE)
    add_custom_target(parzek-docs-pdf
      COMMAND ${MAKE_EXECUTABLE}
      WORKING_DIRECTORY ${PARZEK_DOC_BINARY_DIR}/doxygen/latex
      DEPENDS parzek-doxygen
      COMMENT "Compiling Parzek Doxygen LaTeX into PDF"
      VERBATIM
    )
    set(PARZEK_PDF_INSTALL_SCRIPT ${PARZEK_DOC_BINARY_DIR}/parzek-install-pdf.cmake)
    file(WRITE ${PARZEK_PDF_INSTALL_SCRIPT} [=[
if(EXISTS "@DOC_LATEX_DIR@/Makefile")
  execute_process(
    COMMAND "@MAKE_PROGRAM@"
    WORKING_DIRECTORY "@DOC_LATEX_DIR@"
    RESULT_VARIABLE @PROJECT_KEY@_PDF_RESULT
  )
  if(NOT @PROJECT_KEY@_PDF_RESULT EQUAL 0)
    message(WARNING "@PROJECT_TITLE@ PDF compilation failed during install")
  endif()
endif()
]=])
    file(READ ${PARZEK_PDF_INSTALL_SCRIPT} PARZEK_PDF_INSTALL_CODE)
    string(REPLACE "@DOC_LATEX_DIR@" "${PARZEK_DOC_BINARY_DIR}/doxygen/latex" PARZEK_PDF_INSTALL_CODE "${PARZEK_PDF_INSTALL_CODE}")
    string(REPLACE "@MAKE_PROGRAM@" "${MAKE_EXECUTABLE}" PARZEK_PDF_INSTALL_CODE "${PARZEK_PDF_INSTALL_CODE}")
    string(REPLACE "@PROJECT_KEY@" "PARZEK" PARZEK_PDF_INSTALL_CODE "${PARZEK_PDF_INSTALL_CODE}")
    string(REPLACE "@PROJECT_TITLE@" "Parzek" PARZEK_PDF_INSTALL_CODE "${PARZEK_PDF_INSTALL_CODE}")
    file(WRITE ${PARZEK_PDF_INSTALL_SCRIPT} "${PARZEK_PDF_INSTALL_CODE}")
  else()
    add_custom_target(parzek-docs-pdf
      COMMAND ${CMAKE_COMMAND} -E echo "make not found; Parzek PDF compilation skipped"
      DEPENDS parzek-doxygen
      VERBATIM
    )
  endif()
  if(NOT TARGET docs-html)
    add_custom_target(docs-html)
  endif()
  if(NOT TARGET docs-latex)
    add_custom_target(docs-latex)
  endif()
  if(NOT TARGET docs-pdf)
    add_custom_target(docs-pdf)
  endif()
  add_dependencies(docs-html parzek-docs-html)
  add_dependencies(docs-latex parzek-docs-latex)
  add_dependencies(docs-pdf parzek-docs-pdf)
  install(DIRECTORY ${PARZEK_DOC_BINARY_DIR}/doxygen/html DESTINATION ${CMAKE_INSTALL_DOCDIR}/Parzek OPTIONAL)
  install(DIRECTORY ${PARZEK_DOC_BINARY_DIR}/doxygen/latex DESTINATION ${CMAKE_INSTALL_DOCDIR}/Parzek OPTIONAL)
  if(MAKE_EXECUTABLE)
    install(SCRIPT ${PARZEK_PDF_INSTALL_SCRIPT})
  endif()
else()
  add_custom_target(parzek-doxygen ALL COMMAND ${CMAKE_COMMAND} -E echo "Doxygen not found; skipping Parzek documentation")
endif()
