cmake_minimum_required(VERSION 3.10)
project(rcs_sdk_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 默认认为本 demo 在 sdk/demo 下，上一级就是 SDK 根
set(RCS_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "RCS SDK 根目录（含 include/ 与 lib/）")

find_library(RCS_PROC_LIB rcs_proc PATHS "${RCS_SDK_ROOT}/lib" NO_DEFAULT_PATH)
if(NOT RCS_PROC_LIB)
  message(FATAL_ERROR "找不到 librcs_proc，请检查 RCS_SDK_ROOT=${RCS_SDK_ROOT}/lib")
endif()

add_executable(rcs_demo demo_main.cpp)
target_include_directories(rcs_demo PRIVATE "${RCS_SDK_ROOT}/include")
# libtiff 已静态编进 librcs_proc 时，demo 只需链 rcs_proc
target_link_libraries(rcs_demo PRIVATE ${RCS_PROC_LIB})
if(UNIX AND NOT APPLE)
  target_link_libraries(rcs_demo PRIVATE pthread dl)
endif()

# 运行时直接找 ../lib，少设一次 LD_LIBRARY_PATH
set_target_properties(rcs_demo PROPERTIES
  BUILD_RPATH "${RCS_SDK_ROOT}/lib"
  INSTALL_RPATH "${RCS_SDK_ROOT}/lib"
)

message(STATUS "RCS_SDK_ROOT = ${RCS_SDK_ROOT}")
message(STATUS "librcs_proc  = ${RCS_PROC_LIB}")
