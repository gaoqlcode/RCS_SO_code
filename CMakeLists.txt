cmake_minimum_required(VERSION 3.14)
project(RCS_SO VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(RCS_BUILD_GUI "Build Qt GUI application" ON)
option(RCS_BUILD_CLI "Build rcs_cli" ON)
option(RCS_BUILD_TESTS "Build tests" ON)
# 零级成像（ZeroDataImageHandle）：仅 Linux；可用 -DRCS_ENABLE_TOZERODATA=OFF 关闭
option(RCS_ENABLE_TOZERODATA "Enable ZeroDataImageHandle (Linux only; static libZeroDataImageHandleCpp11.a)" ON)
if(WIN32)
  if(RCS_ENABLE_TOZERODATA)
    message(STATUS "零级成像不支持 Windows，已强制 RCS_ENABLE_TOZERODATA=OFF")
  endif()
  set(RCS_ENABLE_TOZERODATA OFF CACHE BOOL "Enable ZeroDataImageHandle (Linux only)" FORCE)
endif()

# GUI/CLI 链 sdk/（需当前平台已有库：Linux .so / Windows .dll）。
# 默认 OFF：本机从源码编完整库（含大图拼接等全部功能）。
option(RCS_USE_SDK "GUI/CLI 使用打包好的 sdk/（include+lib）" OFF)
set(RCS_SDK_ROOT "${CMAKE_SOURCE_DIR}/sdk" CACHE PATH "SDK 根目录（含 include/ 与 lib/）")

if(RCS_USE_SDK)
  find_library(RCS_PROC_LIB rcs_proc PATHS "${RCS_SDK_ROOT}/lib" NO_DEFAULT_PATH)
  if(NOT RCS_PROC_LIB)
    message(FATAL_ERROR
      "RCS_USE_SDK=ON 但在 ${RCS_SDK_ROOT}/lib 找不到当前平台的 rcs_proc。\n"
      "  Windows：先本机编库再打包：\n"
      "    cmake -S . -B build_win -G \"MinGW Makefiles\" -DRCS_USE_SDK=OFF ...\n"
      "    cmake --build build_win -j && .\\sdk\\pack_sdk.bat\n"
      "  Linux：cmake -DRCS_USE_SDK=OFF ... && ./sdk/pack_sdk.sh\n"
      "  或直接关掉：-DRCS_USE_SDK=OFF（从源码编完整库）")
  endif()
  if(NOT EXISTS "${RCS_SDK_ROOT}/include/rcs/rcs_api.h")
    message(FATAL_ERROR "RCS_USE_SDK=ON 但缺少 ${RCS_SDK_ROOT}/include/rcs/rcs_api.h")
  endif()

  add_library(rcs_proc SHARED IMPORTED GLOBAL)
  set_target_properties(rcs_proc PROPERTIES
    IMPORTED_LOCATION "${RCS_PROC_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${RCS_SDK_ROOT}/include"
  )
  if(WIN32)
    find_library(RCS_PROC_IMPLIB rcs_proc PATHS "${RCS_SDK_ROOT}/lib" NO_DEFAULT_PATH)
    if(RCS_PROC_IMPLIB AND NOT RCS_PROC_IMPLIB STREQUAL RCS_PROC_LIB)
      set_target_properties(rcs_proc PROPERTIES IMPORTED_IMPLIB "${RCS_PROC_IMPLIB}")
    elseif(EXISTS "${RCS_SDK_ROOT}/lib/librcs_proc.dll.a")
      set_target_properties(rcs_proc PROPERTIES IMPORTED_IMPLIB "${RCS_SDK_ROOT}/lib/librcs_proc.dll.a")
    endif()
    if(EXISTS "${RCS_SDK_ROOT}/lib/rcs_proc.dll")
      set_target_properties(rcs_proc PROPERTIES IMPORTED_LOCATION "${RCS_SDK_ROOT}/lib/rcs_proc.dll")
    elseif(EXISTS "${RCS_SDK_ROOT}/lib/librcs_proc.dll")
      set_target_properties(rcs_proc PROPERTIES IMPORTED_LOCATION "${RCS_SDK_ROOT}/lib/librcs_proc.dll")
    endif()
  endif()
  add_library(rcs::proc ALIAS rcs_proc)
  if(RCS_ENABLE_TOZERODATA)
    target_compile_definitions(rcs_proc INTERFACE RCS_ENABLE_TOZERODATA=1)
  endif()
  set(RCS_PUBLIC_INCLUDE_DIR "${RCS_SDK_ROOT}/include")
  message(STATUS "Imported rcs_proc: ${RCS_PROC_LIB}（完整功能，含拼接）")
else()
  # 完整库：始终编入大图拼接，依赖 libtiff
  include(RcsTiff)
  if(NOT RCS_TIFF_OK)
    message(FATAL_ERROR
      "编完整库需要 libtiff（大图拼接）。\n"
      "  1) Linux: ./3rdparty/scripts/build_linux_static.sh 生成 3rdparty/install/linux_<架构>\n"
      "  2) Windows: 使用 3rdparty/install/mingw73_64\n"
      "  3) 临时用系统动态库: -DRCS_TIFF_PREFER_STATIC=OFF（需 apt install libtiff5）")
  endif()
  message(STATUS "Full library (mosaic ON, tiff=${RCS_TIFF_SOURCE}, static=${RCS_TIFF_STATIC})")

  add_subdirectory(lib)
  set(RCS_PUBLIC_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/lib/include")
endif()

if(RCS_BUILD_CLI)
  add_subdirectory(cli)
endif()

if(RCS_BUILD_GUI)
  find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)
  add_subdirectory(gui)
endif()

if(RCS_BUILD_TESTS)
  if(RCS_USE_SDK)
    message(STATUS "RCS_USE_SDK=ON：跳过 tests（依赖 lib 源码）")
  else()
    enable_testing()
    add_subdirectory(tests)
  endif()
endif()
