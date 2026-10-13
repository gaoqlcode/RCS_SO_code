# =============================================================================
# gui_demo 独立工程：只链已打包的 SDK + Qt5，不编仓库里完整 gui/
# 用法（在本目录）：
#   cmake -S . -B build && cmake --build build -j
# 需先在仓库根：编好 rcs_proc 并 ./sdk/pack_sdk.sh
# =============================================================================

cmake_minimum_required(VERSION 3.10)          # 最低 CMake；3.10 起 Qt5 查找较稳
project(rcs_gui_demo LANGUAGES CXX)         # 工程名；只编 C++

set(CMAKE_CXX_STANDARD 11)                  # 与 SDK 一致：C++11
set(CMAKE_CXX_STANDARD_REQUIRED ON)         # 编译器必须真正支持 C++11
set(CMAKE_AUTOMOC ON)                       # 自动跑 moc：处理 Q_OBJECT / slots / signals

# 默认 SDK 根 = 本目录的上两级的上一级？当前文件在 sdk/demo/gui_demo → ../.. 即 sdk/
# CACHE PATH：可 -DRCS_SDK_ROOT=... 覆盖
set(RCS_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "SDK 根目录（含 include/ 与 lib/）")

# 找 Qt5 的 Core/Gui/Widgets 组件（开发包需已安装）
find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

# 在 SDK 的 lib 里找 librcs_proc.so / .dll；NO_DEFAULT_PATH 避免误链系统其它库
find_library(RCS_PROC_LIB rcs_proc PATHS "${RCS_SDK_ROOT}/lib" NO_DEFAULT_PATH)
if(NOT RCS_PROC_LIB)
  # 找不到就立刻失败，并提示先 pack
  message(FATAL_ERROR "没找到 librcs_proc.so，请检查 ${RCS_SDK_ROOT}/lib，或先跑 ./sdk/pack_sdk.sh")
endif()

# 本 demo 全部源文件（头文件由编译器通过 #include 找到，不必列进 add_executable）
set(GUI_DEMO_SOURCES
  main.cpp
  MainWindow.cpp
  DemoWorker.cpp
  ConfirmBridge.cpp
  SimplePlotWidget.cpp
)

if(WIN32)
  # WIN32：子系统为 Windows，不弹黑色控制台；需链 WinMain 入口
  add_executable(rcs_gui_demo WIN32 ${GUI_DEMO_SOURCES})
  target_link_libraries(rcs_gui_demo PRIVATE Qt5::WinMain)
else()
  # Linux/mac：普通可执行文件
  add_executable(rcs_gui_demo ${GUI_DEMO_SOURCES})
endif()

# 头文件搜索：本目录（ConfirmBridge.h 等）+ SDK 的 include（rcs/rcs.hpp）
target_include_directories(rcs_gui_demo PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  "${RCS_SDK_ROOT}/include"
)

# 链接：动态库 rcs_proc + Qt 三件套
target_link_libraries(rcs_gui_demo PRIVATE
  ${RCS_PROC_LIB}
  Qt5::Core
  Qt5::Gui
  Qt5::Widgets
)

# Linux：线程与动态加载（部分发行版显式链更稳）
if(UNIX AND NOT APPLE)
  target_link_libraries(rcs_gui_demo PRIVATE pthread dl)
endif()

# 把 SDK lib 写进 RPATH，运行时优先从那里找 .so，减少手动 export LD_LIBRARY_PATH
set_target_properties(rcs_gui_demo PROPERTIES
  BUILD_RPATH "${RCS_SDK_ROOT}/lib"      # 构建树里直接运行时用
  INSTALL_RPATH "${RCS_SDK_ROOT}/lib"    # install 后用（本 demo 很少 install）
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" # 可执行文件输出到 build/
)

# 配置阶段打印路径，方便核对是否指错 SDK
message(STATUS "RCS_SDK_ROOT = ${RCS_SDK_ROOT}")
message(STATUS "librcs_proc  = ${RCS_PROC_LIB}")
message(STATUS "target       = rcs_gui_demo")
