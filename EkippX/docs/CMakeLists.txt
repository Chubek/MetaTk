include(GNUInstallDirs)

find_package(Doxygen QUIET)
find_program(MAKE_EXECUTABLE NAMES gmake make)

set(EKIPPX_DOC_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(EKIPPX_DOC_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(EKIPPX_DOXYFILE ${EKIPPX_DOC_BINARY_DIR}/Doxyfile)

configure_file(${EKIPPX_DOC_SOURCE_DIR}/Doxyfile.in ${EKIPPX_DOXYFILE} @ONLY)
configure_file(${EKIPPX_DOC_SOURCE_DIR}/conf.py.in ${EKIPPX_DOC_BINARY_DIR}/conf.py @ONLY)
configure_file(${EKIPPX_DOC_SOURCE_DIR}/index.rst.in ${EKIPPX_DOC_BINARY_DIR}/index.rst @ONLY)
file(COPY ${EKIPPX_DOC_SOURCE_DIR}/frontpage.md ${EKIPPX_DOC_SOURCE_DIR}/manual DESTINATION ${EKIPPX_DOC_BINARY_DIR})

set(EKIPPX_DOC_STAMP ${EKIPPX_DOC_BINARY_DIR}/ekippx-docs.stamp)

if(DOXYGEN_FOUND)
  add_custom_command(
    OUTPUT ${EKIPPX_DOC_STAMP}
    COMMAND ${DOXYGEN_EXECUTABLE} ${EKIPPX_DOXYFILE}
    COMMAND ${CMAKE_COMMAND} -E touch ${EKIPPX_DOC_STAMP}
    DEPENDS ${EKIPPX_DOXYFILE} ${EKIPPX_DOC_SOURCE_DIR}/frontpage.md
    WORKING_DIRECTORY ${EKIPPX_DOC_BINARY_DIR}
    COMMENT "Generating EkippX Doxygen HTML, LaTeX, and XML"
    VERBATIM
  )
  add_custom_target(ekippx-doxygen ALL DEPENDS ${EKIPPX_DOC_STAMP})
  add_custom_target(ekippx-docs-html ALL DEPENDS ekippx-doxygen)
  add_custom_target(ekippx-docs-latex ALL DEPENDS ekippx-doxygen)
  if(MAKE_EXECUTABLE)
    add_custom_target(ekippx-docs-pdf
      COMMAND ${MAKE_EXECUTABLE}
      WORKING_DIRECTORY ${EKIPPX_DOC_BINARY_DIR}/doxygen/latex
      DEPENDS ekippx-doxygen
      COMMENT "Compiling EkippX Doxygen LaTeX into PDF"
      VERBATIM
    )
    set(EKIPPX_PDF_INSTALL_SCRIPT ${EKIPPX_DOC_BINARY_DIR}/ekippx-install-pdf.cmake)
    file(WRITE ${EKIPPX_PDF_INSTALL_SCRIPT} [=[
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
    file(READ ${EKIPPX_PDF_INSTALL_SCRIPT} EKIPPX_PDF_INSTALL_CODE)
    string(REPLACE "@DOC_LATEX_DIR@" "${EKIPPX_DOC_BINARY_DIR}/doxygen/latex" EKIPPX_PDF_INSTALL_CODE "${EKIPPX_PDF_INSTALL_CODE}")
    string(REPLACE "@MAKE_PROGRAM@" "${MAKE_EXECUTABLE}" EKIPPX_PDF_INSTALL_CODE "${EKIPPX_PDF_INSTALL_CODE}")
    string(REPLACE "@PROJECT_KEY@" "EKIPPX" EKIPPX_PDF_INSTALL_CODE "${EKIPPX_PDF_INSTALL_CODE}")
    string(REPLACE "@PROJECT_TITLE@" "EkippX" EKIPPX_PDF_INSTALL_CODE "${EKIPPX_PDF_INSTALL_CODE}")
    file(WRITE ${EKIPPX_PDF_INSTALL_SCRIPT} "${EKIPPX_PDF_INSTALL_CODE}")
  else()
    add_custom_target(ekippx-docs-pdf
      COMMAND ${CMAKE_COMMAND} -E echo "make not found; EkippX PDF compilation skipped"
      DEPENDS ekippx-doxygen
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
  add_dependencies(docs-html ekippx-docs-html)
  add_dependencies(docs-latex ekippx-docs-latex)
  add_dependencies(docs-pdf ekippx-docs-pdf)
  install(DIRECTORY ${EKIPPX_DOC_BINARY_DIR}/doxygen/html DESTINATION ${CMAKE_INSTALL_DOCDIR}/EkippX OPTIONAL)
  install(DIRECTORY ${EKIPPX_DOC_BINARY_DIR}/doxygen/latex DESTINATION ${CMAKE_INSTALL_DOCDIR}/EkippX OPTIONAL)
  if(MAKE_EXECUTABLE)
    install(SCRIPT ${EKIPPX_PDF_INSTALL_SCRIPT})
  endif()
else()
  add_custom_target(ekippx-doxygen ALL COMMAND ${CMAKE_COMMAND} -E echo "Doxygen not found; skipping EkippX documentation")
endif()
