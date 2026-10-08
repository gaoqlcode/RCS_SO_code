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
      "RCS_ENABLE_MOSAIC=ON 但找不到 libtiff。\n"
      "  1) Linux: 3rdparty/scripts/build_linux_x86_64.sh 生成静态库后重配\n"
      "  2) Windows: 使用 3rdparty/install/mingw73_64\n"
      "  3) 或 apt install libtiff-dev / vcpkg tiff\n"
      "  4) 或不需要拼接: -DRCS_ENABLE_MOSAIC=OFF")
  endif()
  add_compile_definitions(RCS_ENABLE_MOSAIC=1)
  message(STATUS "Mosaic ENABLED (tiff=${RCS_TIFF_SOURCE}, static=${RCS_TIFF_STATIC})")
else()
  add_compile_definitions(RCS_ENABLE_MOSAIC=0)
  message(STATUS "Mosaic DISABLED (libtiff not required)")
endif()

add_subdirectory(lib)

if(RCS_BUILD_CLI)
  add_subdirectory(cli)
endif()

if(RCS_BUILD_GUI)
  find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)
  add_subdirectory(gui)
endif()

if(RCS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
