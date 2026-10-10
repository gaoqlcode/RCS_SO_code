cmake_minimum_required(VERSION 3.10)
project(rcs_gui_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# 默认指向上级 sdk/
set(RCS_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "SDK 根目录（含 include/ 与 lib/）")

find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

find_library(RCS_PROC_LIB rcs_proc PATHS "${RCS_SDK_ROOT}/lib" NO_DEFAULT_PATH)
if(NOT RCS_PROC_LIB)
  message(FATAL_ERROR "没找到 librcs_proc.so，请检查 ${RCS_SDK_ROOT}/lib，或先跑 ./sdk/pack_sdk.sh")
endif()

set(GUI_DEMO_SOURCES
  main.cpp
  MainWindow.cpp
  DemoWorker.cpp
  ConfirmBridge.cpp
  SimplePlotWidget.cpp
)

if(WIN32)
  add_executable(rcs_gui_demo WIN32 ${GUI_DEMO_SOURCES})
  target_link_libraries(rcs_gui_demo PRIVATE Qt5::WinMain)
else()
  add_executable(rcs_gui_demo ${GUI_DEMO_SOURCES})
endif()

target_include_directories(rcs_gui_demo PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  "${RCS_SDK_ROOT}/include"
)
target_link_libraries(rcs_gui_demo PRIVATE
  ${RCS_PROC_LIB}
  Qt5::Core
  Qt5::Gui
  Qt5::Widgets
)
if(UNIX AND NOT APPLE)
  target_link_libraries(rcs_gui_demo PRIVATE pthread dl)
endif()

set_target_properties(rcs_gui_demo PROPERTIES
  BUILD_RPATH "${RCS_SDK_ROOT}/lib"
  INSTALL_RPATH "${RCS_SDK_ROOT}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

message(STATUS "RCS_SDK_ROOT = ${RCS_SDK_ROOT}")
message(STATUS "librcs_proc  = ${RCS_PROC_LIB}")
message(STATUS "target       = rcs_gui_demo")
