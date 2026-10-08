#ifndef RCS_API_QT_H
#define RCS_API_QT_H

#include "Types.h"
#include "rcs/rcs_api.h"
#include <QImage>
#include <QString>
#include <QVector>
#include <cstring>
#include <string>
#include <vector>

// Qt ↔ 库：字符串、灰度图、选点结果的小转换

// 与库约定：路径/消息一律 UTF-8（Windows 下库内再转宽字符）
inline QString qs(const std::string &s) { return QString::fromUtf8(s.data(), static_cast<int>(s.size())); }
inline std::string ss(const QString &s)
{
    const QByteArray u = s.toUtf8();
    return std::string(u.constData(), static_cast<size_t>(u.size()));
}

inline QImage grayToQImage(const GrayImage &g)
{
    if (g.width <= 0 || g.height <= 0 || g.pixels.empty())
        return QImage();
    QImage img(g.width, g.height, QImage::Format_Grayscale8);
    for (int y = 0; y < g.height; ++y) {
        memcpy(img.scanLine(y), &g.pixels[static_cast<size_t>(y) * g.width],
               static_cast<size_t>(g.width));
    }
    return img;
}

inline QVector<double> toQV(const std::vector<double> &v)
{
    QVector<double> out;
    out.reserve(static_cast<int>(v.size()));
    for (size_t i = 0; i < v.size(); ++i)
        out.push_back(v[i]);
    return out;
}

// 界面选点 → 库选点
inline InteractSelection toLibSel(const UserSelection &q)
{
    InteractSelection a;
    a.targetMode = q.targetMode;
    a.cornerRangeOverrideM = q.cornerRangeOverrideM;
    for (int i = 0; i < q.corners.size(); ++i)
        a.corners.push_back(InteractPick(q.corners[i].az, q.corners[i].rg));
    for (int i = 0; i < q.targets.size(); ++i)
        a.targets.push_back(InteractPick(q.targets[i].az, q.targets[i].rg));
    for (int i = 0; i < q.extendedRanges.size(); ++i)
        a.extendedRanges.push_back(std::make_pair(q.extendedRanges[i].first, q.extendedRanges[i].second));
    return a;
}

struct MosaicResultQt {
    QString mosaicTif;
    QString offsetsCsv;
    QString flattenTif;
    QString displayTif;
    QVector<int> seamCols;
};

typedef MosaicResultQt MosaicResult;

inline MosaicResultQt toQtMosaic(const MosaicResponse &r)
{
    MosaicResultQt o;
    o.mosaicTif = qs(r.mosaicTif);
    o.offsetsCsv = qs(r.offsetsCsv);
    o.flattenTif = qs(r.flattenTif);
    o.displayTif = qs(r.displayTif);
    for (size_t i = 0; i < r.seamCols.size(); ++i)
        o.seamCols.push_back(r.seamCols[i]);
    return o;
}

#endif
