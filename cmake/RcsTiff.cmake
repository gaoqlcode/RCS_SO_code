# 解析 libtiff：优先「当前目标架构」的工程内静态库（须带 -fPIC，才能打进 .so）
# 禁止使用系统 /usr/lib/.../libtiff.a：发行版静态库通常无 PIC，链进 librcs_proc.so 会报
#   relocation R_AARCH64_* ... recompile with -fPIC

set(RCS_TIFF_OK FALSE)
set(RCS_TIFF_STATIC FALSE)
unset(RCS_TIFF_SOURCE)

option(RCS_TIFF_PREFER_STATIC "Prefer bundled static libtiff linked into librcs_proc" ON)

# 归一化架构目录名
set(_rcs_proc "${CMAKE_SYSTEM_PROCESSOR}")
string(TOLOWER "${_rcs_proc}" _rcs_proc)
if(_rcs_proc MATCHES "^(amd64|x86_64|x64)$")
  set(_rcs_linux_arch "x86_64")
elseif(_rcs_proc MATCHES "^(aarch64|arm64)$")
  set(_rcs_linux_arch "aarch64")
elseif(_rcs_proc MATCHES "^(armv7|armv7l|armhf)$")
  set(_rcs_linux_arch "armv7l")
else()
  set(_rcs_linux_arch "${_rcs_proc}")
endif()

# ---- 1) 工程自带静态库（build_linux_static.sh 生成，带 PIC）----
if(RCS_TIFF_PREFER_STATIC)
  set(_rcs_bundled_candidates "")
  if(WIN32 OR MINGW)
    list(APPEND _rcs_bundled_candidates
      "${CMAKE_SOURCE_DIR}/3rdparty/install/mingw73_64")
  endif()
  if(UNIX AND NOT APPLE)
    list(APPEND _rcs_bundled_candidates
      "${CMAKE_SOURCE_DIR}/3rdparty/install/linux_${_rcs_linux_arch}")
  endif()

  foreach(_pfx IN LISTS _rcs_bundled_candidates)
    if(EXISTS "${_pfx}/include/tiffio.h" AND EXISTS "${_pfx}/lib/libtiff.a")
      add_library(rcs_tiff STATIC IMPORTED GLOBAL)
      set_target_properties(rcs_tiff PROPERTIES
        IMPORTED_LOCATION "${_pfx}/lib/libtiff.a"
        INTERFACE_INCLUDE_DIRECTORIES "${_pfx}/include"
      )
      if(EXISTS "${_pfx}/lib/libz.a")
        set_property(TARGET rcs_tiff APPEND PROPERTY
          INTERFACE_LINK_LIBRARIES "${_pfx}/lib/libz.a")
      elseif(EXISTS "${_pfx}/lib/libzlibstatic.a")
        set_property(TARGET rcs_tiff APPEND PROPERTY
          INTERFACE_LINK_LIBRARIES "${_pfx}/lib/libzlibstatic.a")
      endif()
      if(WIN32 OR MINGW)
        set_property(TARGET rcs_tiff APPEND PROPERTY INTERFACE_LINK_LIBRARIES ws2_32)
      endif()
      if(NOT TARGET TIFF::TIFF)
        add_library(TIFF::TIFF ALIAS rcs_tiff)
      endif()
      set(RCS_TIFF_OK TRUE)
      set(RCS_TIFF_STATIC TRUE)
      set(RCS_TIFF_SOURCE "${_pfx}")
      message(STATUS "Using bundled static libtiff (${_rcs_linux_arch}): ${_pfx}")
      break()
    endif()
  endforeach()

  if(NOT RCS_TIFF_OK AND UNIX AND NOT APPLE)
    message(FATAL_ERROR
      "RCS_TIFF_PREFER_STATIC=ON，但没有 3rdparty/install/linux_${_rcs_linux_arch}/lib/libtiff.a\n"
      "系统自带的 libtiff.a 通常无 -fPIC，不能打进 librcs_proc.so（正是你看到的链接错误）。\n"
      "请先在本机生成带 PIC 的静态库：\n"
      "  ./3rdparty/scripts/fetch_sources.sh\n"
      "  ./3rdparty/scripts/build_linux_static.sh\n"
      "然后删掉旧 build 再 cmake。\n"
      "若暂时用系统动态库（客户需装 libtiff）：-DRCS_TIFF_PREFER_STATIC=OFF")
  endif()
endif()

# ---- 2) 系统动态库（客户机需有 libtiff.so）----
if(NOT RCS_TIFF_OK)
  # 明确找 .so，避免 FindTIFF 误选无 PIC 的 .a
  find_library(RCS_TIFF_SO NAMES tiff libtiff.so.6 libtiff.so.5 libtiff.so
    PATHS /usr/lib/${_rcs_linux_arch}-linux-gnu /usr/lib /usr/local/lib)
  find_path(RCS_TIFF_INC tiffio.h PATHS /usr/include /usr/local/include)
  if(RCS_TIFF_SO AND RCS_TIFF_INC)
    add_library(rcs_tiff SHARED IMPORTED GLOBAL)
    set_target_properties(rcs_tiff PROPERTIES
      IMPORTED_LOCATION "${RCS_TIFF_SO}"
      INTERFACE_INCLUDE_DIRECTORIES "${RCS_TIFF_INC}"
    )
    if(NOT TARGET TIFF::TIFF)
      add_library(TIFF::TIFF ALIAS rcs_tiff)
    endif()
    set(RCS_TIFF_OK TRUE)
    set(RCS_TIFF_STATIC FALSE)
    set(RCS_TIFF_SOURCE "system-shared:${RCS_TIFF_SO}")
    message(STATUS "Using system shared libtiff: ${RCS_TIFF_SO}")
  else()
    find_package(TIFF)
    if(TIFF_FOUND)
      set(RCS_TIFF_OK TRUE)
      set(RCS_TIFF_STATIC FALSE)
      set(RCS_TIFF_SOURCE "system-shared")
      message(STATUS "Using system libtiff via FindTIFF")
    endif()
  endif()
endif()
