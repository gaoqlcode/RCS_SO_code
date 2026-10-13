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
  src/sigma0/process_sigma0.cpp
  src/sigma0/process_rcs_cal_tif.cpp
  src/l0_preview/process_l0_preview.cpp
  src/sdk_util/process_sdk_util.cpp
  # 大图拼接（完整库始终编入）
  src/mosaic/process_mosaic.cpp
  src/mosaic/MosaicAlgorithm.cpp
)

if(RCS_ENABLE_TOZERODATA)
  list(APPEND RCS_PROC_SOURCES src/tozerodata/process_tozerodata.cpp)
endif()

add_library(rcs_proc SHARED ${RCS_PROC_SOURCES})
add_library(rcs::proc ALIAS rcs_proc)

target_compile_definitions(rcs_proc PRIVATE RCS_PROC_BUILD)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sigma0
    ${CMAKE_CURRENT_SOURCE_DIR}/src/l0_preview
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sdk_util
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mosaic
)

target_link_libraries(rcs_proc PRIVATE TIFF::TIFF)

if(RCS_ENABLE_TOZERODATA)
  set(TOZERODATA_ROOT "${CMAKE_SOURCE_DIR}/3rdparty/tozerodata")
  set(TOZERODATA_A "${TOZERODATA_ROOT}/lib/libToZeroDataCpp11.a")
  if(NOT EXISTS "${TOZERODATA_A}")
    message(FATAL_ERROR
      "RCS_ENABLE_TOZERODATA=ON 但缺少静态库: ${TOZERODATA_A}\n"
      "请放入 libToZeroDataCpp11.a / tozerodata.h，或关掉: -DRCS_ENABLE_TOZERODATA=OFF")
  endif()
  if(NOT EXISTS "${TOZERODATA_ROOT}/include/tozerodata.h")
    message(FATAL_ERROR "缺少 ${TOZERODATA_ROOT}/include/tozerodata.h")
  endif()

  add_library(ToZeroDataCpp11 STATIC IMPORTED GLOBAL)
  set_target_properties(ToZeroDataCpp11 PROPERTIES
    IMPORTED_LOCATION "${TOZERODATA_A}"
    INTERFACE_INCLUDE_DIRECTORIES "${TOZERODATA_ROOT}/include"
  )

  target_include_directories(rcs_proc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tozerodata
    ${TOZERODATA_ROOT}/include
  )
  # --whole-archive：把 .a 打进 librcs_proc.so
  target_link_libraries(rcs_proc PRIVATE
    -Wl,--whole-archive ToZeroDataCpp11 -Wl,--no-whole-archive)
  target_compile_definitions(rcs_proc PUBLIC RCS_ENABLE_TOZERODATA=1)
  message(STATUS "ToZeroData: ON (static link ${TOZERODATA_A})")
else()
  message(STATUS "ToZeroData: OFF")
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
