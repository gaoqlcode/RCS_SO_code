add_executable(rcs_cli main.cpp)
target_link_libraries(rcs_cli PRIVATE rcs_proc)
target_include_directories(rcs_cli PRIVATE ${RCS_PUBLIC_INCLUDE_DIR})
if(UNIX AND NOT APPLE)
  target_link_libraries(rcs_cli PRIVATE pthread dl)
endif()

if(RCS_USE_SDK)
  set_target_properties(rcs_cli PROPERTIES
    BUILD_RPATH "${RCS_SDK_ROOT}/lib"
    INSTALL_RPATH "${RCS_SDK_ROOT}/lib"
  )
endif()

install(TARGETS rcs_cli RUNTIME DESTINATION bin)
