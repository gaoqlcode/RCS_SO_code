set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(RCS_GUI_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(RCS_APP_SOURCES
  ${RCS_GUI_DIR}/main.cpp
  ${RCS_GUI_DIR}/app/MainShellWindow.cpp
  ${RCS_GUI_DIR}/common/Types.cpp
  ${RCS_GUI_DIR}/common/SharedDataSession.cpp
  ${RCS_GUI_DIR}/common/ProcessWorker.cpp
  ${RCS_GUI_DIR}/ui/InteractivePlotWidget.cpp
  ${RCS_GUI_DIR}/ui/InteractiveImageWidget.cpp
  ${RCS_GUI_DIR}/ui/LogProgressPanel.cpp
  ${RCS_GUI_DIR}/modules/hrrp/HrrpPage.cpp
  ${RCS_GUI_DIR}/modules/hrrp/HrrpWorker.cpp
  ${RCS_GUI_DIR}/modules/rcs/RcsPage.cpp
  ${RCS_GUI_DIR}/modules/rcs/RcsWorker.cpp
  ${RCS_GUI_DIR}/modules/cw_rcs/CwRcsPage.cpp
  ${RCS_GUI_DIR}/modules/cw_rcs/CwRcsWorker.cpp
  ${RCS_GUI_DIR}/modules/mosaic/MosaicPage.cpp
  ${RCS_GUI_DIR}/modules/mosaic/MosaicWorker.cpp
  ${RCS_GUI_DIR}/modules/mosaic/FlattenPanel.cpp
  ${RCS_GUI_DIR}/modules/mosaic/ManualAlignDialog.cpp
  ${RCS_GUI_DIR}/resources/rcs207.qrc
)

# WIN32：Windows 下无控制台黑框（子系统为 Windows，不是 Console）
if(WIN32)
  add_executable(RCS WIN32 ${RCS_APP_SOURCES})
else()
  add_executable(RCS ${RCS_APP_SOURCES})
endif()
target_include_directories(RCS PRIVATE
  ${RCS_GUI_DIR}
  ${RCS_GUI_DIR}/common
  ${RCS_GUI_DIR}/ui
  ${RCS_GUI_DIR}/modules/rcs
  ${RCS_GUI_DIR}/modules/hrrp
  ${RCS_GUI_DIR}/modules/cw_rcs
  ${RCS_GUI_DIR}/modules/mosaic
  ${RCS_GUI_DIR}/app
  ${CMAKE_SOURCE_DIR}/lib/include
)
target_link_libraries(RCS PRIVATE rcs_proc Qt5::Core Qt5::Gui Qt5::Widgets)
if(WIN32)
  # Qt5 WinMain 入口（配合 WIN32 子系统）
  target_link_libraries(RCS PRIVATE Qt5::WinMain)
endif()

# 把拼接开关传给 GUI（与根工程一致）
if(RCS_ENABLE_MOSAIC)
  target_compile_definitions(RCS PRIVATE RCS_ENABLE_MOSAIC=1)
else()
  target_compile_definitions(RCS PRIVATE RCS_ENABLE_MOSAIC=0)
endif()

set_target_properties(RCS PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  BUILD_RPATH "${CMAKE_BINARY_DIR}/lib"
  INSTALL_RPATH "$ORIGIN/../lib"
)
