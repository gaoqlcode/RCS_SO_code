#ifndef OUTPATH_H
#define OUTPATH_H

#include <QDir>
#include <QString>

// 处理结果默认落在「数据文件夹/处理结果_<标记>」下。
// 界面可以改输出目录；留空时 worker 按这个规则拼路径。

inline QString resultFolderName(const QString &tag)
{
    return QStringLiteral("处理结果_%1").arg(tag);
}

inline QString defaultResultDir(const QString &dataFolder, const QString &tag)
{
    if (dataFolder.isEmpty())
        return QString();
    return QDir(dataFolder).filePath(resultFolderName(tag));
}

inline QString rcsResultTag(const QString &pol)
{
    return QStringLiteral("RCS_%1").arg(pol.toUpper().trimmed());
}

inline QString hrrpResultTag(const QString &pol)
{
    return QStringLiteral("HRRP_%1").arg(pol.toUpper().trimmed());
}

inline QString mosaicResultTag()
{
    return QStringLiteral("拼接");
}

inline QString cwRcsResultTag(const QString &pol)
{
    return QStringLiteral("点频RCS_%1").arg(pol.toUpper().trimmed());
}

inline QString resolveOutDir(const QString &userOut, const QString &dataFolder, const QString &tag)
{
    const QString u = userOut.trimmed();
    if (!u.isEmpty())
        return u;
    return defaultResultDir(dataFolder, tag);
}

#endif
