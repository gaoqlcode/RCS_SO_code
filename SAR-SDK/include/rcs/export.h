#ifndef RCS_EXPORT_H
#define RCS_EXPORT_H

// 动态库导出宏：编库时定义 RCS_PROC_BUILD；客户链接时自动为 import/默认可见。

#if defined(_WIN32) || defined(__CYGWIN__)
#  ifdef RCS_PROC_BUILD
#    define RCS_API __declspec(dllexport)
#  else
#    define RCS_API __declspec(dllimport)
#  endif
#else
#  if defined(RCS_PROC_BUILD)
#    define RCS_API __attribute__((visibility("default")))
#  else
#    define RCS_API
#  endif
#endif

#endif
