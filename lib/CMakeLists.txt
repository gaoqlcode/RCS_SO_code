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
  list(APPEND RCS_PROC_SOURCES
    src/tozerodata/process_tozerodata.cpp
    src/tozerodata/process_zeroimage.cpp
  )
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
  set(TOZERODATA_SO "${TOZERODATA_ROOT}/lib/libToZeroDataCpp11.so")
  if(NOT EXISTS "${TOZERODATA_SO}")
    message(FATAL_ERROR "缺少源 DAT 转 L0 动态库: ${TOZERODATA_SO}")
  endif()
  if(NOT EXISTS "${TOZERODATA_ROOT}/include/tozerodata.h")
    message(FATAL_ERROR "缺少 ${TOZERODATA_ROOT}/include/tozerodata.h")
  endif()

  set(POST_ROOT "${CMAKE_SOURCE_DIR}/3rdparty/sar_isar_post")
  if(NOT EXISTS "${POST_ROOT}/bin/sarIsarPostProcess")
    message(FATAL_ERROR "缺少成像后处理程序: ${POST_ROOT}/bin/sarIsarPostProcess")
  endif()

  add_library(ToZeroDataCpp11 SHARED IMPORTED GLOBAL)
  set_target_properties(ToZeroDataCpp11 PROPERTIES
    IMPORTED_LOCATION "${TOZERODATA_SO}"
    IMPORTED_NO_SONAME TRUE
    INTERFACE_INCLUDE_DIRECTORIES "${TOZERODATA_ROOT}/include"
  )

  target_include_directories(rcs_proc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tozerodata
    ${TOZERODATA_ROOT}/include
  )
  # 客户仍只链 -lrcs_proc；libToZeroDataCpp11.so 放在同一目录，由 $ORIGIN 找到
  target_link_libraries(rcs_proc PRIVATE ToZeroDataCpp11 ${CMAKE_DL_LIBS})
  set_target_properties(rcs_proc PROPERTIES
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN"
  )
  add_custom_command(TARGET rcs_proc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${TOZERODATA_SO}" "$<TARGET_FILE_DIR:rcs_proc>/libToZeroDataCpp11.so"
    COMMENT "copy libToZeroDataCpp11.so next to librcs_proc")
  target_compile_definitions(rcs_proc PUBLIC RCS_ENABLE_TOZERODATA=1)
  target_compile_definitions(rcs_proc PRIVATE "RCS_POST_ROOT=\"${POST_ROOT}\"")
  message(STATUS "ToZeroData: shared ${TOZERODATA_SO}")
  message(STATUS "SAR/ISAR post: ${POST_ROOT}")
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
