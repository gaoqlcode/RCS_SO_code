# 解析 libtiff：优先「当前目标架构」的工程内静态库（编进 librcs_proc），否则系统库
# 注意：禁止把 x86_64 的 .a 链进 aarch64 目标（或反过来）

set(RCS_TIFF_OK FALSE)
set(RCS_TIFF_STATIC FALSE)
unset(RCS_TIFF_SOURCE)

option(RCS_TIFF_PREFER_STATIC "Prefer static libtiff linked into librcs_proc" ON)

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

# ---- 1) 工程自带静态库（与目标架构一致）----
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
endif()

# ---- 2) 系统静态 .a（仅本机原生编译时可用；交叉编译勿用宿主 /usr/lib）----
if(NOT RCS_TIFF_OK AND RCS_TIFF_PREFER_STATIC AND UNIX AND NOT APPLE
   AND NOT CMAKE_CROSSCOMPILING)
  find_library(RCS_SYS_TIFF_A NAMES libtiff.a
    PATHS /usr/lib/${_rcs_linux_arch}-linux-gnu /usr/lib /usr/local/lib)
  find_path(RCS_SYS_TIFF_INC tiffio.h PATHS /usr/include /usr/local/include)
  if(RCS_SYS_TIFF_A AND RCS_SYS_TIFF_INC)
    set(_rcs_static_deps "${RCS_SYS_TIFF_A}")
    foreach(_n IN ITEMS webp zstd lzma jbig jpeg z)
      find_library(_rcs_dep_${_n} NAMES lib${_n}.a
        PATHS /usr/lib/${_rcs_linux_arch}-linux-gnu /usr/lib /usr/local/lib)
      if(_rcs_dep_${_n})
        list(APPEND _rcs_static_deps "${_rcs_dep_${_n}}")
      endif()
    endforeach()
    add_library(rcs_tiff INTERFACE IMPORTED GLOBAL)
    set_target_properties(rcs_tiff PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${RCS_SYS_TIFF_INC}"
      INTERFACE_LINK_LIBRARIES "${_rcs_static_deps};m"
    )
    if(NOT TARGET TIFF::TIFF)
      add_library(TIFF::TIFF ALIAS rcs_tiff)
    endif()
    set(RCS_TIFF_OK TRUE)
    set(RCS_TIFF_STATIC TRUE)
    set(RCS_TIFF_SOURCE "system-static")
    message(STATUS "Using system static libtiff (embedded into librcs_proc)")
  endif()
endif()

# ---- 3) 系统动态库 ----
if(NOT RCS_TIFF_OK)
  find_package(TIFF)
  if(TIFF_FOUND)
    set(RCS_TIFF_OK TRUE)
    set(RCS_TIFF_STATIC FALSE)
    set(RCS_TIFF_SOURCE "system-shared")
    message(STATUS "Using system shared libtiff (runtime needs libtiff)")
  endif()
endif()
