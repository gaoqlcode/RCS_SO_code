#ifndef SHAREDDATASESSION_H
#define SHAREDDATASESSION_H

#include <QObject>
#include <QString>

// 各页「数据路径」后台同步：L0 / RCS / HRRP / 点频改一处，其余跟进
class SharedDataSession : public QObject {
    Q_OBJECT
public:
    explicit SharedDataSession(QObject *parent = 0) : QObject(parent) {}

    QString dataFolder() const { return dataFolder_; }

    void setDataFolder(const QString &path)
    {
        const QString p = path.trimmed();
        if (p == dataFolder_)
            return;
        dataFolder_ = p;
        emit dataFolderChanged(dataFolder_);
    }

signals:
    void dataFolderChanged(const QString &path);

private:
    QString dataFolder_;
};

#endif
