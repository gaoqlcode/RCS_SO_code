add_executable(rcs_smoke smoke_main.cpp)
target_link_libraries(rcs_smoke PRIVATE rcs_proc)
target_include_directories(rcs_smoke PRIVATE
  ${CMAKE_SOURCE_DIR}/lib/include
  ${CMAKE_SOURCE_DIR}/lib/src
  ${CMAKE_SOURCE_DIR}/lib/src/common
  ${CMAKE_SOURCE_DIR}/lib/src/io
  ${CMAKE_SOURCE_DIR}/lib/src/signal
  ${CMAKE_SOURCE_DIR}/lib/src/mosaic
)

add_executable(rcs_func func_main.cpp)
target_link_libraries(rcs_func PRIVATE rcs_proc)
target_include_directories(rcs_func PRIVATE
  ${CMAKE_SOURCE_DIR}/lib/include
  ${CMAKE_SOURCE_DIR}/lib/src
  ${CMAKE_SOURCE_DIR}/lib/src/common
  ${CMAKE_SOURCE_DIR}/lib/src/io
  ${CMAKE_SOURCE_DIR}/lib/src/signal
  ${CMAKE_SOURCE_DIR}/lib/src/mosaic
)
if(RCS_ENABLE_MOSAIC)
  target_compile_definitions(rcs_func PRIVATE RCS_ENABLE_MOSAIC=1)
  target_compile_definitions(rcs_smoke PRIVATE RCS_ENABLE_MOSAIC=1)
else()
  target_compile_definitions(rcs_func PRIVATE RCS_ENABLE_MOSAIC=0)
  target_compile_definitions(rcs_smoke PRIVATE RCS_ENABLE_MOSAIC=0)
endif()

add_test(NAME smoke COMMAND rcs_smoke)
add_test(NAME func COMMAND rcs_func)
