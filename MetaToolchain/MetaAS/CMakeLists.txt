add_library(metaas MetaAS.cpp)
add_library(MetaAS::metaas ALIAS metaas)
target_link_libraries(metaas PRIVATE MetaArch::as)
target_include_directories(metaas PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/metaarch>)
target_compile_features(metaas PRIVATE cxx_std_20)
set_target_properties(metaas PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(metaas PRIVATE -Wall -Wextra -Wpedantic)
endif()
install(TARGETS metaas ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES MetaAS.h MetaAS.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/metaarch/MetaAS)
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DOCDIR}/MetaAS)
