cmake_minimum_required(VERSION 4.5)
project(SerdeTk LANGUAGES C CXX)

include(GNUInstallDirs)

set(INSTALL_MZIP_EXAMPLES OFF)

add_library(DSLtk INTERFACE)
add_subdirectory(docs)
add_subdirectory(test)

target_include_directories(DSLtk INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/DSLtk)

target_link_libraries(SerdeTk PRIVATE DSLtk)

configure_file(
	cmake/serdetk.pc.in
	${CMAKE_CURRENT_BINARY_DIR}/serdetk.pc
	@ONLY
)

configure_file(
	cmake/minizip.pc.in
	${CMAKE_CURRENT_BINARY_DIR}/minizip.pc
	@ONLY
)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/serdetk.pc
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/minizip.pc
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

install(FILES include/SerdeTk.hpp
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES include/MiniZIP.hpp
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(INSTALL_MZIP_EXAMPLES)
	install(DIRECTORY examples/MiniZIP
		DESTINATION ${CMAKE_INSTALL_DOCDIR}/minizip/examples
)
endif()

message(NOTICE "Installed SerdeTk")
message(NOTICE "Installed MiniZIP")
