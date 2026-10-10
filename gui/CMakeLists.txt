set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(RCS_GUI_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(RCS_APP_SOURCES
  ${RCS_GUI_DIR}/main.cpp
  ${RCS_GUI_DIR}/app/MainShellWindow.cpp
  ${RCS_GUI_DIR}/common/Types.cpp
  ${RCS_GUI_DIR}/common/SharedDataSession.cpp
  ${RCS_GUI_DIR}/common/OutDirField.cpp
  ${RCS_GUI_DIR}/common/ProcessWorker.cpp
  ${RCS_GUI_DIR}/ui/InteractivePlotWidget.cpp
  ${RCS_GUI_DIR}/ui/InteractiveImageWidget.cpp
  ${RCS_GUI_DIR}/ui/LogProgressPanel.cpp
  ${RCS_GUI_DIR}/ui/RectPickDialog.cpp
  ${RCS_GUI_DIR}/modules/hrrp/HrrpPage.cpp
  ${RCS_GUI_DIR}/modules/hrrp/HrrpWorker.cpp
  ${RCS_GUI_DIR}/modules/rcs/RcsPage.cpp
  ${RCS_GUI_DIR}/modules/rcs/RcsWorker.cpp
  ${RCS_GUI_DIR}/modules/cw_rcs/CwRcsPage.cpp
  ${RCS_GUI_DIR}/modules/cw_rcs/CwRcsWorker.cpp
  ${RCS_GUI_DIR}/modules/sigma0/Sigma0Page.cpp
  ${RCS_GUI_DIR}/modules/sigma0/Sigma0Worker.cpp
  ${RCS_GUI_DIR}/modules/l0_preview/L0PreviewPage.cpp
  ${RCS_GUI_DIR}/modules/l0_preview/L0PreviewWorker.cpp
  ${RCS_GUI_DIR}/modules/l0_preview/L0PreviewDialog.cpp
  ${RCS_GUI_DIR}/resources/rcs207.qrc
)

if(RCS_ENABLE_MOSAIC)
  list(APPEND RCS_APP_SOURCES
    ${RCS_GUI_DIR}/modules/mosaic/MosaicPage.cpp
    ${RCS_GUI_DIR}/modules/mosaic/MosaicWorker.cpp
    ${RCS_GUI_DIR}/modules/mosaic/FlattenPanel.cpp
    ${RCS_GUI_DIR}/modules/mosaic/ManualAlignDialog.cpp
  )
endif()

# WIN32：无控制台；嵌入 app.ico 供资源管理器/任务栏显示
if(WIN32)
  list(APPEND RCS_APP_SOURCES ${RCS_GUI_DIR}/resources/rcs.rc)
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
  ${RCS_GUI_DIR}/modules/sigma0
  ${RCS_GUI_DIR}/modules/l0_preview
  ${RCS_GUI_DIR}/modules/mosaic
  ${RCS_GUI_DIR}/app
  ${RCS_PUBLIC_INCLUDE_DIR}
)

target_link_libraries(RCS PRIVATE rcs_proc Qt5::Core Qt5::Gui Qt5::Widgets)
if(WIN32)
  target_link_libraries(RCS PRIVATE Qt5::WinMain)
endif()
if(UNIX AND NOT APPLE)
  target_link_libraries(RCS PRIVATE pthread dl)
endif()

if(RCS_ENABLE_MOSAIC)
  target_compile_definitions(RCS PRIVATE RCS_ENABLE_MOSAIC=1)
else()
  target_compile_definitions(RCS PRIVATE RCS_ENABLE_MOSAIC=0)
endif()

if(RCS_USE_SDK)
  set(_rcs_rpath "${RCS_SDK_ROOT}/lib")
else()
  set(_rcs_rpath "${CMAKE_BINARY_DIR}/lib")
endif()

set_target_properties(RCS PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  BUILD_RPATH "${_rcs_rpath}"
  INSTALL_RPATH "$ORIGIN/../lib"
)
