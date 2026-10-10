#include "L0PreviewDialog.h"
#include "ui/InteractivePlotWidget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

L0PreviewDialog::L0PreviewDialog(const QVector<L0PulsePreviewItem> &items, QWidget *parent)
    : QDialog(parent)
    , items_(items)
    , index_(0)
    , fileLabel_(0)
    , hintLabel_(0)
    , timePlot_(0)
    , freqPlot_(0)
{
    setWindowTitle(QStringLiteral("原始数据预处理 — 时域/频域预览"));
    setWindowModality(Qt::NonModal);
    resize(1200, 820);
    setAttribute(Qt::WA_DeleteOnClose, true);

    fileLabel_ = new QLabel(this);
    fileLabel_->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600; padding:4px;"));
    fileLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hintLabel_ = new QLabel(
        QStringLiteral("同一界面显示时域与频域 | 单击图或「上一组/下一组」切换文件（不自动播放）"),
        this);
    hintLabel_->setStyleSheet(QStringLiteral("color:#666;"));

    timePlot_ = new InteractivePlotWidget(this);
    timePlot_->setAxisTitles(QStringLiteral("时间 (μs)"), QStringLiteral("信号幅度"));
    timePlot_->setMinimumHeight(280);
    freqPlot_ = new InteractivePlotWidget(this);
    freqPlot_->setAxisTitles(QStringLiteral("频率 (MHz)"), QStringLiteral("频谱幅度 (dB)"));
    freqPlot_->setFixedYRange(-60.0, 3.0);
    freqPlot_->setMinimumHeight(280);

    auto *btns = new QHBoxLayout;
    auto *prev = new QPushButton(QStringLiteral("上一组"), this);
    auto *next = new QPushButton(QStringLiteral("下一组"), this);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    btns->addWidget(prev);
    btns->addWidget(next);
    btns->addStretch();
    btns->addWidget(closeBtn);

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(fileLabel_);
    lay->addWidget(hintLabel_);
    lay->addWidget(timePlot_, 1);
    lay->addWidget(freqPlot_, 1);
    lay->addLayout(btns);

    connect(timePlot_, &InteractivePlotWidget::plotClicked, this, &L0PreviewDialog::nextItem);
    connect(freqPlot_, &InteractivePlotWidget::plotClicked, this, &L0PreviewDialog::nextItem);
    connect(prev, &QPushButton::clicked, this, &L0PreviewDialog::prevItem);
    connect(next, &QPushButton::clicked, this, &L0PreviewDialog::nextItem);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);

    if (!items_.isEmpty())
        showIndex(0);
    else
        fileLabel_->setText(QStringLiteral("无预览数据"));
}

void L0PreviewDialog::showIndex(int idx)
{
    if (items_.isEmpty())
        return;
    if (idx < 0 || idx >= items_.size())
        idx = 0;
    index_ = idx;
    const L0PulsePreviewItem &it = items_[index_];

    fileLabel_->setText(QStringLiteral("[%1/%2]  文件名：%3")
                            .arg(index_ + 1)
                            .arg(items_.size())
                            .arg(QString::fromUtf8(it.fileName.data(),
                                                   static_cast<int>(it.fileName.size()))));

    QVector<double> t, amp, f, sdB;
    t.reserve(static_cast<int>(it.tUs.size()));
    amp.reserve(static_cast<int>(it.amp.size()));
    for (size_t i = 0; i < it.tUs.size() && i < it.amp.size(); ++i) {
        t.push_back(it.tUs[i]);
        amp.push_back(it.amp[i]);
    }
    f.reserve(static_cast<int>(it.fMHz.size()));
    sdB.reserve(static_cast<int>(it.spectrumDb.size()));
    for (size_t i = 0; i < it.fMHz.size() && i < it.spectrumDb.size(); ++i) {
        f.push_back(it.fMHz[i]);
        sdB.push_back(it.spectrumDb[i]);
    }

    timePlot_->setUpdatesEnabled(false);
    freqPlot_->setUpdatesEnabled(false);
    timePlot_->setData(t, amp);
    timePlot_->setPlotTitle(QStringLiteral("时域特性 | 第%1脉冲 | 峰值 %2 μs | 信号区 %3~%4 μs")
                                .arg(it.pulseSel)
                                .arg(it.ampPkUs, 0, 'f', 4)
                                .arg(it.sig1Us, 0, 'f', 4)
                                .arg(it.sig2Us, 0, 'f', 4));
    freqPlot_->setData(f, sdB);
    freqPlot_->setPlotTitle(
        QStringLiteral("频域特性 | 谱峰 %1 MHz | 90%占用带宽 %2 MHz")
            .arg(QString::number(it.fPkMHz, 'f', 3), QString::number(it.occBwMHz, 'f', 3)));
    timePlot_->setUpdatesEnabled(true);
    freqPlot_->setUpdatesEnabled(true);
    timePlot_->update();
    freqPlot_->update();
}

void L0PreviewDialog::nextItem()
{
    if (items_.isEmpty())
        return;
    index_ = (index_ + 1) % items_.size();
    showIndex(index_);
}

void L0PreviewDialog::prevItem()
{
    if (items_.isEmpty())
        return;
    index_ = (index_ - 1 + items_.size()) % items_.size();
    showIndex(index_);
}
