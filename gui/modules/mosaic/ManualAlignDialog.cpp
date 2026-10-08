#include "ManualAlignDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QComboBox>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QEvent>
#include <QScrollArea>
#include <QPixmap>
#include <QtMath>

ManualAlignDialog::ManualAlignDialog(const QImage &frontPreview, const QImage &backPreview,
                                     int previewStep, int pairIndex, int pairTotal,
                                     QWidget *parent)
    : QDialog(parent)
    , front_(frontPreview.convertToFormat(QImage::Format_Grayscale8))
    , back_(backPreview.convertToFormat(QImage::Format_Grayscale8))
    , step_(qMax(1, previewStep))
{
    setWindowTitle(QStringLiteral("手动对齐 %1 / %2（拖动后一张，调透明度后确定）")
                       .arg(pairIndex).arg(pairTotal));
    resize(1100, 720);
    setFocusPolicy(Qt::StrongFocus);

    // 初值：后图向右偏约 1/3 预览宽（常见方位重叠）
    dColPrev_ = front_.width() / 3;
    dRowPrev_ = 0;

    auto *root = new QVBoxLayout(this);
    info_ = new QLabel(this);
    info_->setWordWrap(true);
    root->addWidget(info_);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setBackgroundRole(QPalette::Dark);
    view_ = new QLabel;
    view_->setAlignment(Qt::AlignCenter);
    view_->setMinimumSize(640, 400);
    view_->setMouseTracking(true);
    view_->installEventFilter(this);
    scroll->setWidget(view_);
    root->addWidget(scroll, 1);

    auto *form = new QFormLayout;
    alphaFront_ = new QSlider(Qt::Horizontal, this);
    alphaFront_->setRange(5, 100);
    alphaFront_->setValue(100);
    form->addRow(QStringLiteral("前一张透明度"), alphaFront_);
    alphaBack_ = new QSlider(Qt::Horizontal, this);
    alphaBack_->setRange(5, 100);
    alphaBack_->setValue(55);
    form->addRow(QStringLiteral("后一张透明度"), alphaBack_);
    bright_ = new QSlider(Qt::Horizontal, this);
    bright_->setRange(10, 300);
    bright_->setValue(100);
    form->addRow(QStringLiteral("预览亮度(仅显示)"), bright_);
    nudgeStep_ = new QComboBox(this);
    nudgeStep_->addItem(QStringLiteral("1 px"), 1);
    nudgeStep_->addItem(QStringLiteral("4 px"), 4);
    nudgeStep_->addItem(QStringLiteral("16 px"), 16);
    nudgeStep_->addItem(QStringLiteral("32 px"), 32);
    nudgeStep_->addItem(QStringLiteral("128 px"), 128);
    nudgeStep_->setCurrentIndex(2);
    form->addRow(QStringLiteral("方向键步长(全分辨率)"), nudgeStep_);
    root->addLayout(form);

    auto *btns = new QHBoxLayout;
    auto *onlyA = new QPushButton(QStringLiteral("只看前"), this);
    auto *onlyB = new QPushButton(QStringLiteral("只看后"), this);
    auto *half = new QPushButton(QStringLiteral("各50%"), this);
    auto *okBtn = new QPushButton(QStringLiteral("确认该对"), this);
    auto *skipRest = new QPushButton(QStringLiteral("跳过剩余(改自动)"), this);
    auto *cancel = new QPushButton(QStringLiteral("取消拼接"), this);
    btns->addWidget(onlyA);
    btns->addWidget(onlyB);
    btns->addWidget(half);
    btns->addStretch();
    btns->addWidget(okBtn);
    btns->addWidget(skipRest);
    btns->addWidget(cancel);
    root->addLayout(btns);

    connect(alphaFront_, &QSlider::valueChanged, this, [this](int) { rebuildComposite(); });
    connect(alphaBack_, &QSlider::valueChanged, this, [this](int) { rebuildComposite(); });
    connect(bright_, &QSlider::valueChanged, this, [this](int) { rebuildComposite(); });
    connect(onlyA, &QPushButton::clicked, this, [this]() {
        alphaFront_->setValue(100);
        alphaBack_->setValue(5);
    });
    connect(onlyB, &QPushButton::clicked, this, [this]() {
        alphaFront_->setValue(5);
        alphaBack_->setValue(100);
    });
    connect(half, &QPushButton::clicked, this, [this]() {
        alphaFront_->setValue(50);
        alphaBack_->setValue(50);
    });
    connect(okBtn, &QPushButton::clicked, this, [this]() {
        dRowFull_ = dRowPrev_ * step_;
        dColFull_ = dColPrev_ * step_;
        accept();
    });
    connect(skipRest, &QPushButton::clicked, this, [this]() {
        skipRest_ = true;
        dRowFull_ = dRowPrev_ * step_;
        dColFull_ = dColPrev_ * step_;
        accept();
    });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    rebuildComposite();
}

bool ManualAlignDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == view_) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *e = static_cast<QMouseEvent *>(event);
            if (e->button() == Qt::LeftButton) {
                dragging_ = true;
                lastPos_ = e->pos();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && dragging_) {
            auto *e = static_cast<QMouseEvent *>(event);
            const QPoint d = e->pos() - lastPos_;
            lastPos_ = e->pos();
            // 视图坐标 y 向下为正 → 预览行偏移同向；x 同理
            dColPrev_ += d.x();
            dRowPrev_ += d.y();
            rebuildComposite();
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            dragging_ = false;
            return true;
        } else if (event->type() == QEvent::Wheel) {
            // 滚轮交给外层滚动区
        }
    }
    return QDialog::eventFilter(obj, event);
}

void ManualAlignDialog::keyPressEvent(QKeyEvent *event)
{
    const int fullStep = nudgeStep_->currentData().toInt();
    const int prevStep = qMax(1, fullStep / step_);
    switch (event->key()) {
    case Qt::Key_Left:
        applyNudge(0, -prevStep);
        return;
    case Qt::Key_Right:
        applyNudge(0, prevStep);
        return;
    case Qt::Key_Up:
        applyNudge(-prevStep, 0);
        return;
    case Qt::Key_Down:
        applyNudge(prevStep, 0);
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        dRowFull_ = dRowPrev_ * step_;
        dColFull_ = dColPrev_ * step_;
        accept();
        return;
    default:
        QDialog::keyPressEvent(event);
    }
}

void ManualAlignDialog::applyNudge(int dRowPrev, int dColPrev)
{
    dRowPrev_ += dRowPrev;
    dColPrev_ += dColPrev;
    rebuildComposite();
}

void ManualAlignDialog::rebuildComposite()
{
    const int aA = alphaFront_->value();
    const int aB = alphaBack_->value();
    const double br = bright_->value() / 100.0;

    const int minX = qMin(0, dColPrev_);
    const int minY = qMin(0, dRowPrev_);
    const int maxX = qMax(front_.width(), dColPrev_ + back_.width());
    const int maxY = qMax(front_.height(), dRowPrev_ + back_.height());
    const int W = qMax(1, maxX - minX);
    const int H = qMax(1, maxY - minY);

    QImage out(W, H, QImage::Format_RGB32);
    out.fill(QColor(20, 20, 20));

    auto sample = [](const QImage &im, int x, int y) -> int {
        if (x < 0 || y < 0 || x >= im.width() || y >= im.height())
            return -1;
        return qGray(im.pixel(x, y));
    };

    for (int y = 0; y < H; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < W; ++x) {
            const int xa = x + minX;
            const int ya = y + minY;
            const int xb = xa - dColPrev_;
            const int yb = ya - dRowPrev_;
            const int va = sample(front_, xa, ya);
            const int vb = sample(back_, xb, yb);
            double r = 0, g = 0, b = 0, wsum = 0;
            if (va >= 0) {
                const double w = aA / 100.0;
                const double v = qBound(0.0, va * br, 255.0);
                r += v * w;
                g += v * w;
                b += v * w;
                wsum += w;
            }
            if (vb >= 0) {
                const double w = aB / 100.0;
                const double v = qBound(0.0, vb * br, 255.0);
                // 后一张略偏青，便于分辨重叠
                r += v * w * 0.85;
                g += v * w;
                b += v * w;
                wsum += w;
            }
            if (wsum > 1e-6) {
                r /= wsum;
                g /= wsum;
                b /= wsum;
            }
            line[x] = qRgb(qBound(0, int(r), 255), qBound(0, int(g), 255), qBound(0, int(b), 255));
        }
    }

    view_->setPixmap(QPixmap::fromImage(out));
    info_->setText(QStringLiteral("左键拖动后一张 | 方向键微移 | 回车确认\n"
                                  "当前位移(全分辨率)：Δ行=%1  Δ列=%2  （预览 Δ行=%3 Δ列=%4，抽稀×%5）")
                       .arg(dRowPrev_ * step_)
                       .arg(dColPrev_ * step_)
                       .arg(dRowPrev_)
                       .arg(dColPrev_)
                       .arg(step_));
}
