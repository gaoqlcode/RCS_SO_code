add_executable(rcs_cli main.cpp)
target_link_libraries(rcs_cli PRIVATE rcs_proc)
target_include_directories(rcs_cli PRIVATE ${CMAKE_SOURCE_DIR}/lib/include)

install(TARGETS rcs_cli RUNTIME DESTINATION bin)
