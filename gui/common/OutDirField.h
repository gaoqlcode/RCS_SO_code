#ifndef OUTDIRFIELD_H
#define OUTDIRFIELD_H

#include <QObject>
#include <QString>

class QLineEdit;
class QWidget;
class QHBoxLayout;

// 各业务页统一的「输出目录」设置方式：
//   默认 = baseFolder / 处理结果_<tag>
//   用户手改或浏览后保留；输入路径切换时重置为自动路径；
//   极化等 tag 变化时，若仍是上次自动值则跟着变。
class OutDirField : public QObject {
    Q_OBJECT
public:
    explicit OutDirField(QObject *parent = 0);

    /** 建「输出目录」一行（标签 + 编辑框 + 浏览），挂到 pathBox 布局用 */
    QHBoxLayout *createRow(QWidget *pathBox);

    QLineEdit *edit() const { return edit_; }
    QString text() const;

    /** 占位符：如 baseHint="数据文件夹", tagHint="HRRP_<极化>" */
    void setPlaceholderHint(const QString &baseHint, const QString &tagHint);

    /** 输入路径变更：强制按新 base 重算默认输出 */
    void resetFromBase(const QString &baseFolder, const QString &tag);

    /** 刷新默认路径（用户已手改且内容≠上次自动值时不动） */
    void refresh(const QString &baseFolder, const QString &tag);

    /** 浏览选目录；fallback 一般为当前数据文件夹/RAW 所在目录 */
    void browse(QWidget *dialogParent, const QString &fallbackStartDir);

private slots:
    void onTextEdited();
    void onBrowseClicked();

private:
    QLineEdit *edit_;
    bool userEdited_;
    QString lastAutoOut_;
    QString lastBaseFolder_;
    QString lastTag_;
};

#endif
