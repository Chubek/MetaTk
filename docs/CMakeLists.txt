include(GNUInstallDirs)

find_package(Doxygen QUIET)
find_program(MAKE_EXECUTABLE NAMES gmake make)

set(METATK_DOC_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(METATK_DOC_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(METATK_DOXYFILE ${METATK_DOC_BINARY_DIR}/Doxyfile)

configure_file(${METATK_DOC_SOURCE_DIR}/Doxyfile.in ${METATK_DOXYFILE} @ONLY)
configure_file(${METATK_DOC_SOURCE_DIR}/conf.py.in ${METATK_DOC_BINARY_DIR}/conf.py @ONLY)
configure_file(${METATK_DOC_SOURCE_DIR}/index.rst.in ${METATK_DOC_BINARY_DIR}/index.rst @ONLY)
file(COPY ${METATK_DOC_SOURCE_DIR}/frontpage.md ${METATK_DOC_SOURCE_DIR}/manual DESTINATION ${METATK_DOC_BINARY_DIR})

set(METATK_DOC_STAMP ${METATK_DOC_BINARY_DIR}/metatk-docs.stamp)

if(DOXYGEN_FOUND)
  add_custom_command(
    OUTPUT ${METATK_DOC_STAMP}
    COMMAND ${DOXYGEN_EXECUTABLE} ${METATK_DOXYFILE}
    COMMAND ${CMAKE_COMMAND} -E touch ${METATK_DOC_STAMP}
    DEPENDS ${METATK_DOXYFILE} ${METATK_DOC_SOURCE_DIR}/frontpage.md
    WORKING_DIRECTORY ${METATK_DOC_BINARY_DIR}
    COMMENT "Generating MetaTk Doxygen HTML, LaTeX, and XML"
    VERBATIM
  )
  add_custom_target(metatk-doxygen ALL DEPENDS ${METATK_DOC_STAMP})
  add_custom_target(metatk-docs-html ALL DEPENDS metatk-doxygen)
  add_custom_target(metatk-docs-latex ALL DEPENDS metatk-doxygen)
  if(MAKE_EXECUTABLE)
    add_custom_target(metatk-docs-pdf
      COMMAND ${MAKE_EXECUTABLE}
      WORKING_DIRECTORY ${METATK_DOC_BINARY_DIR}/doxygen/latex
      DEPENDS metatk-doxygen
      COMMENT "Compiling MetaTk Doxygen LaTeX into PDF"
      VERBATIM
    )
    set(METATK_PDF_INSTALL_SCRIPT ${METATK_DOC_BINARY_DIR}/metatk-install-pdf.cmake)
    file(WRITE ${METATK_PDF_INSTALL_SCRIPT} [=[
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
    file(READ ${METATK_PDF_INSTALL_SCRIPT} METATK_PDF_INSTALL_CODE)
    string(REPLACE "@DOC_LATEX_DIR@" "${METATK_DOC_BINARY_DIR}/doxygen/latex" METATK_PDF_INSTALL_CODE "${METATK_PDF_INSTALL_CODE}")
    string(REPLACE "@MAKE_PROGRAM@" "${MAKE_EXECUTABLE}" METATK_PDF_INSTALL_CODE "${METATK_PDF_INSTALL_CODE}")
    string(REPLACE "@PROJECT_KEY@" "METATK" METATK_PDF_INSTALL_CODE "${METATK_PDF_INSTALL_CODE}")
    string(REPLACE "@PROJECT_TITLE@" "MetaTk" METATK_PDF_INSTALL_CODE "${METATK_PDF_INSTALL_CODE}")
    file(WRITE ${METATK_PDF_INSTALL_SCRIPT} "${METATK_PDF_INSTALL_CODE}")
  else()
    add_custom_target(metatk-docs-pdf
      COMMAND ${CMAKE_COMMAND} -E echo "make not found; MetaTk PDF compilation skipped"
      DEPENDS metatk-doxygen
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
  add_dependencies(docs-html metatk-docs-html)
  add_dependencies(docs-latex metatk-docs-latex)
  add_dependencies(docs-pdf metatk-docs-pdf)
  install(DIRECTORY ${METATK_DOC_BINARY_DIR}/doxygen/html DESTINATION ${CMAKE_INSTALL_DOCDIR}/MetaTk OPTIONAL)
  install(DIRECTORY ${METATK_DOC_BINARY_DIR}/doxygen/latex DESTINATION ${CMAKE_INSTALL_DOCDIR}/MetaTk OPTIONAL)
  if(MAKE_EXECUTABLE)
    install(SCRIPT ${METATK_PDF_INSTALL_SCRIPT})
  endif()
else()
  add_custom_target(metatk-doxygen ALL COMMAND ${CMAKE_COMMAND} -E echo "Doxygen not found; skipping MetaTk documentation")
endif()
