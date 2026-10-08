set(RCS_PROC_SOURCES
  # 公共工具
  src/common/BinaryIo.cpp
  src/common/FftBackend.cpp
  # 数据读写
  src/io/Level0Dat.cpp
  src/io/Level2Pdat.cpp
  src/io/Level3Text.cpp
  src/io/MergeDat.cpp
  # 信号处理
  src/signal/Preprocess.cpp
  src/signal/PulseCompress.cpp
  src/signal/AzimuthFocus.cpp
  src/signal/Calibration.cpp
  # 各业务入口
  src/hrrp/process_hrrp.cpp
  src/rcs/process_rcs.cpp
  src/cw_rcs/process_cw_rcs.cpp
  # 拼接入口始终编译（禁用时仅为 stub）
  src/mosaic/process_mosaic.cpp
)

# 真正的 TIFF 算法仅在开启拼接时编进库
if(RCS_ENABLE_MOSAIC)
  list(APPEND RCS_PROC_SOURCES src/mosaic/MosaicAlgorithm.cpp)
endif()

add_library(rcs_proc SHARED ${RCS_PROC_SOURCES})
add_library(rcs::proc ALIAS rcs_proc)

target_compile_definitions(rcs_proc PRIVATE RCS_PROC_BUILD)
if(RCS_ENABLE_MOSAIC)
  target_compile_definitions(rcs_proc PRIVATE RCS_ENABLE_MOSAIC=1)
else()
  target_compile_definitions(rcs_proc PRIVATE RCS_ENABLE_MOSAIC=0)
endif()

target_include_directories(rcs_proc
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io
    ${CMAKE_CURRENT_SOURCE_DIR}/src/signal
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hrrp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rcs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cw_rcs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mosaic
)

if(RCS_ENABLE_MOSAIC)
  target_link_libraries(rcs_proc PRIVATE TIFF::TIFF)
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(rcs_proc PRIVATE pthread)
endif()

set_target_properties(rcs_proc PROPERTIES
  OUTPUT_NAME rcs_proc
  VERSION ${PROJECT_VERSION}
  SOVERSION 1
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)

install(TARGETS rcs_proc
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY include/rcs DESTINATION include)
