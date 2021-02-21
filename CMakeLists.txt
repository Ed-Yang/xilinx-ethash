cmake_minimum_required(VERSION 3.3)  # CMake version check

project(xleth)               # Create project "simple_example"

#set(CMAKE_CXX_STANDARD 14)            # Enable c++14 standard

# Add main.cpp file of project root directory as source file
#set(XCL_SOURCE ./common/includes/xcl2/xcl2.cpp)
#set(ETM_SOURCE ../ethminer/libdevcore/CommonData.cpp)
set(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/ethash.h  ./xleth/src/xleth.cpp)

set(ETHASH_PATH ./ethash)
set(XRT_PATH $ENV{XILINX_XRT})

#include_directories(./common/includes/xcl2)
include_directories(../ethminer)
include_directories(${ETHASH_PATH})
include_directories(${ETHASH_PATH}/include)
include_directories(${XRT_PATH}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

link_directories(${ETHASH_PATH}/build/lib/ethash)
link_directories(${ETHASH_PATH}/build/lib/keccak)
link_directories({XRT_PATH}/lib)

# ethash.cl to ethash.h
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ethash.h
	COMMAND ${CMAKE_COMMAND} ARGS
	-DBIN2H_SOURCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/xleth/kernel/ethash.cl"
	-DBIN2H_VARIABLE_NAME=ethash_cl
	-DBIN2H_HEADER_FILE="${CMAKE_CURRENT_BINARY_DIR}/ethash.h"
	-P "${CMAKE_CURRENT_SOURCE_DIR}/bin2h.cmake"
	COMMENT "Generating OpenCL Kernel Byte Array"
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/xleth/kernel/ethash.cl
)

add_custom_target(
    clbin2h_stable ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/ethash.h ${CMAKE_CURRENT_SOURCE_DIR}/xleth/kernel/ethash.cl
)

# xleth
add_executable(xleth ${SOURCE})

target_link_libraries(xleth libethash.a libkeccak.a)
target_link_libraries(xleth -L${XRT_PATH}/lib -lOpenCL -lpthread)
