#include "FlattenPanel.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QFileDialog>

FlattenPanel::FlattenPanel(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    auto *row = new QHBoxLayout;
    pathEdit_ = new QLineEdit(this);
    auto *browse = new QPushButton(QStringLiteral("选择TIFF…"), this);
    row->addWidget(pathEdit_, 1);
    row->addWidget(browse);
    lay->addLayout(row);

    auto *srow = new QHBoxLayout;
    srow->addWidget(new QLabel(QStringLiteral("目标中位亮度"), this));
    slider_ = new QSlider(Qt::Horizontal, this);
    slider_->setRange(5, 80);
    slider_->setValue(35);
    valueLabel_ = new QLabel(QStringLiteral("0.35"), this);
    srow->addWidget(slider_, 1);
    srow->addWidget(valueLabel_);
    lay->addLayout(srow);

    auto *apply = new QPushButton(QStringLiteral("按此亮度输出匀光"), this);
    lay->addWidget(apply);

    connect(slider_, &QSlider::valueChanged, this, [this](int v) {
        valueLabel_->setText(QString::number(v / 100.0, 'f', 2));
    });
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择TIFF"), QString(), QStringLiteral("TIFF (*.tif *.tiff)"));
        if (!f.isEmpty()) pathEdit_->setText(f);
    });
    connect(apply, &QPushButton::clicked, this, [this]() {
        emit applyRequested(pathEdit_->text().trimmed(), medOut());
    });
}

double FlattenPanel::medOut() const
{
    return slider_->value() / 100.0;
}

void FlattenPanel::setTifPath(const QString &path)
{
    pathEdit_->setText(path);
}
