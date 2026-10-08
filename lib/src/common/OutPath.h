#ifndef OUTPATH_H
#define OUTPATH_H

#include "Util.hpp"
#include <string>

// 默认结果目录：数据目录下「处理结果_xxx」

inline std::string resultFolderName(const std::string &tag)
{
    return std::string("处理结果_") + tag;
}

inline std::string defaultResultDir(const std::string &dataFolder, const std::string &tag)
{
    if (dataFolder.empty())
        return std::string();
    return pathJoin(dataFolder, resultFolderName(tag));
}

inline std::string rcsResultTag(const std::string &pol)
{
    return std::string("RCS_") + toUpperStr(trimStr(pol));
}

inline std::string hrrpResultTag(const std::string &pol)
{
    return std::string("HRRP_") + toUpperStr(trimStr(pol));
}

inline std::string mosaicResultTag()
{
    return std::string("拼接");
}

inline std::string cwRcsResultTag(const std::string &pol)
{
    return std::string("点频RCS_") + toUpperStr(trimStr(pol));
}

// 用户指定了目录就用；否则落到数据目录下带标记的子目录
inline std::string resolveOutDir(const std::string &userOut, const std::string &dataFolder,
                                 const std::string &tag)
{
    const std::string u = trimStr(userOut);
    if (!u.empty())
        return u;
    return defaultResultDir(dataFolder, tag);
}

#endif
