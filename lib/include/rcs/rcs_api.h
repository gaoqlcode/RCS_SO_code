#ifndef RCS_API_H
#define RCS_API_H

#include "rcs/callbacks.hpp"
#include "rcs/export.h"
#include "rcs/types.hpp"
#include <string>
#include <vector>

// 对外入口。库是纯 C++11，调用方自己画界面或写终端。

RCS_API const char *rcsVersion();

RCS_API bool processHrrp(const HrrpRequest &req, HrrpResponse &resp, Callbacks &cb,
                         std::string *err = 0);

RCS_API bool processRcs(const RcsRequest &req, RcsResponse &resp, Callbacks &cb,
                        std::string *err = 0);

// 点频 RCS（一文件一频点）
RCS_API bool processCwRcs(const CwRcsRequest &req, CwRcsResponse &resp, Callbacks &cb,
                          std::string *err = 0);

RCS_API bool processMosaic(const MosaicRequest &req, MosaicResponse &resp, Callbacks &cb,
                           std::string *err = 0);

RCS_API bool flattenMosaic(const std::string &inTif, const std::string &outTif, double medOut,
                           const std::vector<int> &seamCols, Callbacks &cb, std::string *err = 0);

RCS_API std::vector<std::string> listMosaicTiffs(const std::string &folder);

RCS_API bool loadMosaicGrayPreview(const std::string &path, int step, GrayImage &preview, int &fullW,
                                   int &fullH, std::string *err = 0);

// 手动拼接前：整理输入（有 TIFF 直接用，只有 RAW 就先转）
RCS_API bool prepareMosaicInputs(const std::string &inFolder, const std::string &outFolder,
                                 std::vector<std::string> &tiffPaths, Callbacks &cb,
                                 std::string *err = 0, int rawNrHint = 0, int rawNaHint = 0);

#endif
