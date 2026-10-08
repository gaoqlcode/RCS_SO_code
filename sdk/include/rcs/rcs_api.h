#ifndef RCS_API_H
#define RCS_API_H

#include "rcs/callbacks.hpp"
#include "rcs/export.h"
#include "rcs/types.hpp"
#include <string>

// 本 SDK 对外入口（C++11）。大场景拼接相关接口不在交付范围内。

RCS_API const char *rcsVersion();

RCS_API bool processHrrp(const HrrpRequest &req, HrrpResponse &resp, Callbacks &cb,
                         std::string *err = 0);

RCS_API bool processRcs(const RcsRequest &req, RcsResponse &resp, Callbacks &cb,
                        std::string *err = 0);

RCS_API bool processCwRcs(const CwRcsRequest &req, CwRcsResponse &resp, Callbacks &cb,
                          std::string *err = 0);

#endif
