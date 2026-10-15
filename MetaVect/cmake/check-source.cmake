if(NOT EXISTS "$ENV{METAVECT_SOURCE_DIR}/README.md")
  message(FATAL_ERROR "README.md is missing")
endif()
message(STATUS "MetaVect source tree check passed")
