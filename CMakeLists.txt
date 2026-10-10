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

# GUI/CLI 链 sdk/（需当前平台已有库：Linux .so / Windows .dll）。
# 默认 OFF：本机从源码编库（含拼接）。仅二次开发/验证交付包时开 ON。
option(RCS_USE_SDK "GUI/CLI 使用打包好的 sdk/（include+lib）" OFF)
set(RCS_SDK_ROOT "${CMAKE_SOURCE_DIR}/sdk" CACHE PATH "SDK 根目录（含 include/ 与 lib/）")

if(RCS_USE_SDK)
  # 客户头不含拼接
  set(RCS_ENABLE_MOSAIC OFF CACHE BOOL "Enable large-scene mosaic UI and API (needs libtiff)" FORCE)
  message(STATUS "RCS_USE_SDK=ON → 使用 ${RCS_SDK_ROOT}，拼接关闭")
  add_compile_definitions(RCS_ENABLE_MOSAIC=0)

  find_library(RCS_PROC_LIB rcs_proc PATHS "${RCS_SDK_ROOT}/lib" NO_DEFAULT_PATH)
  if(NOT RCS_PROC_LIB)
    message(FATAL_ERROR
      "RCS_USE_SDK=ON 但在 ${RCS_SDK_ROOT}/lib 找不到当前平台的 rcs_proc。\n"
      "  Windows：先本机编库再打包：\n"
      "    cmake -S . -B build_win -G \"MinGW Makefiles\" -DRCS_USE_SDK=OFF ...\n"
      "    cmake --build build_win -j && .\\sdk\\pack_sdk.bat\n"
      "  Linux：cmake -DRCS_USE_SDK=OFF ... && ./sdk/pack_sdk.sh\n"
      "  或直接关掉：-DRCS_USE_SDK=OFF（从源码编，可开拼接）")
  endif()
  if(NOT EXISTS "${RCS_SDK_ROOT}/include/rcs/rcs_api.h")
    message(FATAL_ERROR "RCS_USE_SDK=ON 但缺少 ${RCS_SDK_ROOT}/include/rcs/rcs_api.h")
  endif()

  add_library(rcs_proc SHARED IMPORTED GLOBAL)
  set_target_properties(rcs_proc PROPERTIES
    IMPORTED_LOCATION "${RCS_PROC_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${RCS_SDK_ROOT}/include"
  )
  # MinGW：还有 librcs_proc.dll.a
  if(WIN32)
    find_library(RCS_PROC_IMPLIB rcs_proc PATHS "${RCS_SDK_ROOT}/lib" NO_DEFAULT_PATH)
    if(RCS_PROC_IMPLIB AND NOT RCS_PROC_IMPLIB STREQUAL RCS_PROC_LIB)
      set_target_properties(rcs_proc PROPERTIES IMPORTED_IMPLIB "${RCS_PROC_IMPLIB}")
    elseif(EXISTS "${RCS_SDK_ROOT}/lib/librcs_proc.dll.a")
      set_target_properties(rcs_proc PROPERTIES IMPORTED_IMPLIB "${RCS_SDK_ROOT}/lib/librcs_proc.dll.a")
    endif()
    # 运行时 dll
    if(EXISTS "${RCS_SDK_ROOT}/lib/rcs_proc.dll")
      set_target_properties(rcs_proc PROPERTIES IMPORTED_LOCATION "${RCS_SDK_ROOT}/lib/rcs_proc.dll")
    elseif(EXISTS "${RCS_SDK_ROOT}/lib/librcs_proc.dll")
      set_target_properties(rcs_proc PROPERTIES IMPORTED_LOCATION "${RCS_SDK_ROOT}/lib/librcs_proc.dll")
    endif()
  endif()
  add_library(rcs::proc ALIAS rcs_proc)
  set(RCS_PUBLIC_INCLUDE_DIR "${RCS_SDK_ROOT}/include")
  message(STATUS "Imported rcs_proc: ${RCS_PROC_LIB}")
else()
  # 有「当前目标架构」对应的静态 libtiff 时默认开拼接；否则默认关
  set(_RCS_HAS_BUNDLED_TIFF FALSE)
  if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/3rdparty/install/mingw73_64/include/tiffio.h")
    set(_RCS_HAS_BUNDLED_TIFF TRUE)
  endif()
  if(UNIX AND NOT APPLE)
    set(_rcs_p "${CMAKE_SYSTEM_PROCESSOR}")
    string(TOLOWER "${_rcs_p}" _rcs_p)
    if(_rcs_p MATCHES "^(amd64|x86_64|x64)$")
      set(_rcs_la "x86_64")
    elseif(_rcs_p MATCHES "^(aarch64|arm64)$")
      set(_rcs_la "aarch64")
    elseif(_rcs_p MATCHES "^(armv7|armv7l|armhf)$")
      set(_rcs_la "armv7l")
    else()
      set(_rcs_la "${_rcs_p}")
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/3rdparty/install/linux_${_rcs_la}/include/tiffio.h")
      set(_RCS_HAS_BUNDLED_TIFF TRUE)
    endif()
  endif()
  if(_RCS_HAS_BUNDLED_TIFF)
    option(RCS_ENABLE_MOSAIC "Enable large-scene mosaic UI and API (needs libtiff)" ON)
  else()
    option(RCS_ENABLE_MOSAIC "Enable large-scene mosaic UI and API (needs libtiff)" OFF)
  endif()

  if(RCS_ENABLE_MOSAIC)
    include(RcsTiff)
    if(NOT RCS_TIFF_OK)
      message(FATAL_ERROR
        "RCS_ENABLE_MOSAIC=ON 但找不到可用的 libtiff。\n"
        "  1) Linux: ./3rdparty/scripts/build_linux_static.sh 生成 3rdparty/install/linux_<架构>\n"
        "  2) Windows: 使用 3rdparty/install/mingw73_64\n"
        "  3) 临时用系统动态库: -DRCS_TIFF_PREFER_STATIC=OFF（需 apt install libtiff5）\n"
        "  4) 或不需要拼接: -DRCS_ENABLE_MOSAIC=OFF")
    endif()
    add_compile_definitions(RCS_ENABLE_MOSAIC=1)
    message(STATUS "Mosaic ENABLED (tiff=${RCS_TIFF_SOURCE}, static=${RCS_TIFF_STATIC})")
  else()
    add_compile_definitions(RCS_ENABLE_MOSAIC=0)
    message(STATUS "Mosaic DISABLED (libtiff not required)")
  endif()

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
    message(STATUS "RCS_USE_SDK=ON：跳过 tests（依赖 lib 源码与拼接接口）")
  else()
    enable_testing()
    add_subdirectory(tests)
  endif()
endif()
