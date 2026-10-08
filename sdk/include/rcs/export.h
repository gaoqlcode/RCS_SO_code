#ifndef RCS_EXPORT_H
#define RCS_EXPORT_H

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
