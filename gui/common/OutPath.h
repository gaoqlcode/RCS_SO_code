#ifndef OUTPATH_H
#define OUTPATH_H

#include <QDir>
#include <QFileInfo>
#include <QString>

// 处理结果默认落在「输入基目录/处理结果_<标记>」下。
// 各页经 OutDirField 统一设置；留空时库内 resolveOutDir 同规则。

inline QString resultFolderName(const QString &tag)
{
    return QStringLiteral("处理结果_%1").arg(tag);
}

inline QString defaultResultDir(const QString &baseFolder, const QString &tag)
{
    if (baseFolder.isEmpty())
        return QString();
    return QDir(baseFolder).filePath(resultFolderName(tag));
}

/** 文件夹原样；文件取其父目录（后向散射 RAW） */
inline QString baseDirOfInput(const QString &fileOrFolder)
{
    const QString p = fileOrFolder.trimmed();
    if (p.isEmpty())
        return QString();
    const QFileInfo fi(p);
    if (fi.isFile() || (!fi.exists() && fi.suffix().size() > 0))
        return fi.absolutePath();
    return QDir::cleanPath(fi.absoluteFilePath().isEmpty() ? p : fi.absoluteFilePath());
}

inline QString rcsResultTag(const QString &pol)
{
    return QStringLiteral("RCS_%1").arg(pol.toUpper().trimmed());
}

inline QString hrrpResultTag(const QString &pol)
{
    return QStringLiteral("HRRP_%1").arg(pol.toUpper().trimmed());
}

inline QString cwRcsResultTag(const QString &pol)
{
    return QStringLiteral("点频RCS_%1").arg(pol.toUpper().trimmed());
}

inline QString sigma0ResultTag()
{
    return QStringLiteral("后向散射");
}

inline QString mosaicResultTag()
{
    return QStringLiteral("拼接");
}

inline QString resolveOutDir(const QString &userOut, const QString &baseFolder, const QString &tag)
{
    const QString u = userOut.trimmed();
    if (!u.isEmpty())
        return u;
    return defaultResultDir(baseFolder, tag);
}

#endif
