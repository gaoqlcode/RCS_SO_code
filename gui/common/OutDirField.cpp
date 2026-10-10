#include "OutDirField.h"
#include "OutPath.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>
#include <QWidget>

OutDirField::OutDirField(QObject *parent)
    : QObject(parent)
    , edit_(0)
    , userEdited_(false)
{
}

QHBoxLayout *OutDirField::createRow(QWidget *pathBox)
{
    edit_ = new QLineEdit(pathBox);
    edit_->setMinimumHeight(28);
    auto *browse = new QPushButton(QStringLiteral("浏览…"), pathBox);
    browse->setFixedWidth(72);
    auto *row = new QHBoxLayout;
    row->addWidget(new QLabel(QStringLiteral("输出目录"), pathBox));
    row->addWidget(edit_, 1);
    row->addWidget(browse);
    connect(edit_, &QLineEdit::textEdited, this, &OutDirField::onTextEdited);
    connect(browse, &QPushButton::clicked, this, &OutDirField::onBrowseClicked);
    return row;
}

QString OutDirField::text() const
{
    return edit_ ? edit_->text().trimmed() : QString();
}

void OutDirField::setPlaceholderHint(const QString &baseHint, const QString &tagHint)
{
    if (!edit_)
        return;
    edit_->setPlaceholderText(
        QStringLiteral("%1/%2").arg(baseHint, resultFolderName(tagHint)));
}

void OutDirField::resetFromBase(const QString &baseFolder, const QString &tag)
{
    userEdited_ = false;
    refresh(baseFolder, tag);
}

void OutDirField::refresh(const QString &baseFolder, const QString &tag)
{
    lastBaseFolder_ = baseFolder.trimmed();
    lastTag_ = tag;
    if (!edit_ || lastBaseFolder_.isEmpty() || tag.isEmpty())
        return;
    const QString autoPath = defaultResultDir(lastBaseFolder_, tag);
    if (!userEdited_ || edit_->text().trimmed().isEmpty()
        || edit_->text().trimmed() == lastAutoOut_) {
        edit_->setText(QDir::toNativeSeparators(autoPath));
        lastAutoOut_ = edit_->text();
        userEdited_ = false;
    }
}

void OutDirField::browse(QWidget *dialogParent, const QString &fallbackStartDir)
{
    if (!edit_)
        return;
    const QString cur = edit_->text().trimmed();
    const QString start = !cur.isEmpty() ? cur
                          : (!fallbackStartDir.trimmed().isEmpty() ? fallbackStartDir.trimmed()
                                                                  : lastBaseFolder_);
    const QString d = QFileDialog::getExistingDirectory(
        dialogParent, QStringLiteral("选择输出目录"), start);
    if (d.isEmpty())
        return;
    edit_->setText(QDir::toNativeSeparators(d));
    userEdited_ = true;
}

void OutDirField::onTextEdited()
{
    userEdited_ = true;
}

void OutDirField::onBrowseClicked()
{
    QWidget *w = edit_ ? qobject_cast<QWidget *>(edit_->window()) : 0;
    if (!w && edit_)
        w = edit_->parentWidget();
    browse(w, lastBaseFolder_);
}
