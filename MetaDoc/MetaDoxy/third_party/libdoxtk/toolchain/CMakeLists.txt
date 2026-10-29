set(DOXTK_TOOLS
  any2dvi any2pdf any2ps dump2json dump2xml dump2xtas
  html2pdf markdown2pdf texi2pdf doxtk-convert)
foreach(tool IN LISTS DOXTK_TOOLS)
  add_executable(${tool} ${tool}.cpp)
  target_link_libraries(${tool} PRIVATE DoxTk::DoxTk)
endforeach()
