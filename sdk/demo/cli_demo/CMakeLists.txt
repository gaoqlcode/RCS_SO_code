cmake_minimum_required(VERSION 3.10)
project(rcs_cli_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 默认指向上级 sdk/（含 include/ 与 lib/）
set(RCS_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "SDK 根目录")

find_library(RCS_PROC_LIB rcs_proc PATHS "${RCS_SDK_ROOT}/lib" NO_DEFAULT_PATH)
if(NOT RCS_PROC_LIB)
  message(FATAL_ERROR "找不到 librcs_proc，请检查 ${RCS_SDK_ROOT}/lib（可先运行 sdk/pack_sdk.sh）")
endif()

add_executable(rcs_cli_demo main.cpp)
target_include_directories(rcs_cli_demo PRIVATE "${RCS_SDK_ROOT}/include")
target_link_libraries(rcs_cli_demo PRIVATE ${RCS_PROC_LIB})
if(UNIX AND NOT APPLE)
  target_link_libraries(rcs_cli_demo PRIVATE pthread dl)
endif()

set_target_properties(rcs_cli_demo PROPERTIES
  BUILD_RPATH "${RCS_SDK_ROOT}/lib"
  INSTALL_RPATH "${RCS_SDK_ROOT}/lib"
)

message(STATUS "RCS_SDK_ROOT = ${RCS_SDK_ROOT}")
message(STATUS "librcs_proc  = ${RCS_PROC_LIB}")
