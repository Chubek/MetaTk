set(prefix "${BUILD_DIR}/package-test/install")
set(consumer "${BUILD_DIR}/package-test/consumer")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${prefix}" --config "${CONFIG}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Install failed: ${out}\n${err}")
endif()
execute_process(COMMAND "${prefix}/${INSTALL_BINDIR}/metapp${EXECUTABLE_SUFFIX}" --version
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Installed CLI failed: ${out}\n${err}")
endif()
file(MAKE_DIRECTORY "${consumer}")
file(WRITE "${consumer}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.24)
project(MetaPPConsumer LANGUAGES CXX)
find_package(MetaPP CONFIG REQUIRED)
add_executable(consumer main.cpp)
set_target_properties(consumer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
target_link_libraries(consumer PRIVATE MetaPP::MetaPP)
]=])
file(WRITE "${consumer}/main.cpp" [=[
#include <MetaPP.h>
int main() { return metapp::Preprocessor{}.process("#kernel eval 2+2") == "4\n" ? 0 : 1; }
]=])
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${consumer}" -B "${consumer}/build"
    -G "${GENERATOR}" "-DCMAKE_BUILD_TYPE=${CONFIG}"
    "-DCMAKE_PREFIX_PATH=${prefix}" RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Consumer configuration failed: ${out}\n${err}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${consumer}/build" --config "${CONFIG}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Consumer build failed: ${out}\n${err}")
endif()
execute_process(COMMAND "${consumer}/build/bin/consumer${EXECUTABLE_SUFFIX}" RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Installed package consumer failed: ${status}")
endif()
