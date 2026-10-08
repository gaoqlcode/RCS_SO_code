#include "LogProgressPanel.h"
#include <QVBoxLayout>

LogProgressPanel::LogProgressPanel(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    status_ = new QLabel(QStringLiteral("就绪"), this);
    bar_ = new QProgressBar(this);
    bar_->setRange(0, 100);
    bar_->setValue(0);
    log_ = new QTextEdit(this);
    log_->setReadOnly(true);
    log_->setMinimumHeight(120);
    lay->addWidget(status_);
    lay->addWidget(bar_);
    lay->addWidget(log_, 1);
}

void LogProgressPanel::appendLog(const QString &text)
{
    log_->append(text);
}

void LogProgressPanel::setProgress(int value, const QString &status)
{
    bar_->setValue(qBound(0, value, 100));
    status_->setText(status);
}

void LogProgressPanel::reset()
{
    bar_->setValue(0);
    status_->setText(QStringLiteral("就绪"));
}
