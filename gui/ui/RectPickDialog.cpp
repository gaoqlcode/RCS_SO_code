#include "RectPickDialog.h"
#include <QDialogButtonBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QRubberBand>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QWheelEvent>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

QImage autoLevelsGray(const QImage &src, double loPct, double hiPct)
{
    QImage g = src.convertToFormat(QImage::Format_Grayscale8);
    if (g.isNull() || g.width() <= 0 || g.height() <= 0)
        return g;

    std::vector<int> hist(256, 0);
    int n = 0;
    const int stepX = std::max(1, g.width() / 400);
    const int stepY = std::max(1, g.height() / 400);
    for (int y = 0; y < g.height(); y += stepY) {
        const uchar *line = g.constScanLine(y);
        for (int x = 0; x < g.width(); x += stepX) {
            ++hist[line[x]];
            ++n;
        }
    }
    if (n <= 1)
        return g;

    auto findPct = [&](double p) -> int {
        const int target = static_cast<int>(p / 100.0 * (n - 1));
        int acc = 0;
        for (int i = 0; i < 256; ++i) {
            acc += hist[i];
            if (acc >= target)
                return i;
        }
        return 255;
    };
    int lo = findPct(loPct);
    int hi = findPct(hiPct);
    if (hi <= lo)
        hi = lo + 1;

    QImage out(g.size(), QImage::Format_Grayscale8);
    const double inv = 1.0 / (hi - lo);
    for (int y = 0; y < g.height(); ++y) {
        const uchar *in = g.constScanLine(y);
        uchar *o = out.scanLine(y);
        for (int x = 0; x < g.width(); ++x) {
            double t = (static_cast<int>(in[x]) - lo) * inv;
            if (t < 0.0)
                t = 0.0;
            if (t > 1.0)
                t = 1.0;
            t = std::pow(t, 0.75);
            o[x] = static_cast<uchar>(std::max(0, std::min(255, static_cast<int>(t * 255.0 + 0.5))));
        }
    }
    return out;
}

} // namespace

RectPickDialog::RectPickDialog(const QImage &preview, int fullNr, int fullNa, const QString &title,
                               QWidget *parent)
    : QDialog(parent)
    , scroll_(0)
    , label_(0)
    , band_(0)
    , whiteSlider_(0)
    , whiteSpin_(0)
    , zoom_(1.0)
    , scaleX_(1.0)
    , scaleY_(1.0)
    , offX_(0)
    , offY_(0)
    , fullNr_(fullNr)
    , fullNa_(fullNa)
    , ok_(false)
    , dragging_(false)
    , firstShow_(true)
    , stretchFill_(true)
{
    setWindowTitle(title);
    setWindowModality(Qt::ApplicationModal);
    // 独立窗口：可最小化/最大化/缩放
    setWindowFlags((windowFlags() | Qt::Window) & ~Qt::WindowContextHelpButtonHint);
    setSizeGripEnabled(true);
    resize(1100, 800);
    setMinimumSize(720, 520);

    source_ = preview.convertToFormat(QImage::Format_Grayscale8);
    if (source_.isNull())
        source_ = preview.convertToFormat(QImage::Format_RGB32).convertToFormat(QImage::Format_Grayscale8);
    // 预览在库内已做色阶；此处仅轻度再拉一层，避免二次过曝抹掉地物
    source_ = autoLevelsGray(source_, 0.5, 99.5);

    auto *hint = new QLabel(
        QStringLiteral(
            "拖矩形后点「确定」 | 「铺满」拉伸占满窗口 | 滚轮缩放 | 窗口可最大化。"
            "偏暗/偏亮可调「白场」（仅显示）。Esc/取消放弃。"),
        this);
    hint->setWordWrap(true);

    scroll_ = new QScrollArea(this);
    // false：由 refreshDisplay 控制 label 尺寸，才能真正铺满/滚轮放大后出滚动条
    scroll_->setWidgetResizable(false);
    scroll_->setAlignment(Qt::AlignCenter);
    scroll_->setBackgroundRole(QPalette::Dark);
    scroll_->setStyleSheet(QStringLiteral("QScrollArea{background:#222;}"));
    scroll_->viewport()->installEventFilter(this);

    label_ = new QLabel;
    label_->setAlignment(Qt::AlignCenter);
    label_->setMouseTracking(true);
    label_->installEventFilter(this);
    label_->setMinimumSize(200, 150);
    label_->setStyleSheet(QStringLiteral("background:#222;"));
    scroll_->setWidget(label_);
    band_ = new QRubberBand(QRubberBand::Rectangle, label_);

    const int white0 = suggestWhite(source_);
    whiteSlider_ = new QSlider(Qt::Horizontal, this);
    whiteSlider_->setRange(1, 255);
    whiteSlider_->setValue(white0);
    whiteSpin_ = new QSpinBox(this);
    whiteSpin_->setRange(1, 255);
    whiteSpin_->setValue(white0);
    auto *toneRow = new QHBoxLayout;
    toneRow->addWidget(new QLabel(QStringLiteral("白场(↓更亮)"), this));
    toneRow->addWidget(whiteSlider_, 1);
    toneRow->addWidget(whiteSpin_);
    auto *fitBtn = new QPushButton(QStringLiteral("铺满"), this);
    auto *oneBtn = new QPushButton(QStringLiteral("1:1"), this);
    toneRow->addWidget(fitBtn);
    toneRow->addWidget(oneBtn);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, [this]() { finish(true); });
    connect(btns, &QDialogButtonBox::rejected, this, [this]() { finish(false); });
    connect(whiteSlider_, &QSlider::valueChanged, this, &RectPickDialog::onWhiteChanged);
    connect(whiteSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &RectPickDialog::onWhiteChanged);
    connect(fitBtn, &QPushButton::clicked, this, &RectPickDialog::fitView);
    connect(oneBtn, &QPushButton::clicked, this, &RectPickDialog::resetZoom);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->addWidget(hint);
    lay->addWidget(scroll_, 1);
    lay->addLayout(toneRow);
    lay->addWidget(btns);

    rebuildToneMapped();
    refreshDisplay();
}

void RectPickDialog::showEvent(QShowEvent *ev)
{
    QDialog::showEvent(ev);
    if (firstShow_) {
        firstShow_ = false;
        showMaximized();
        // 等最大化布局落地后再按视口铺满
        QTimer::singleShot(0, this, [this]() { fitView(); });
    }
}

void RectPickDialog::resizeEvent(QResizeEvent *ev)
{
    QDialog::resizeEvent(ev);
    // 拉伸铺满模式下随窗口重算
    if (stretchFill_ && std::abs(zoom_ - 1.0) < 1e-6)
        refreshDisplay();
}

int RectPickDialog::suggestWhite(const QImage &src) const
{
    if (src.isNull() || src.width() <= 0 || src.height() <= 0)
        return 220;
    std::vector<int> hist(256, 0);
    int n = 0;
    const int stepX = std::max(1, src.width() / 200);
    const int stepY = std::max(1, src.height() / 200);
    for (int y = 0; y < src.height(); y += stepY) {
        const uchar *line = src.constScanLine(y);
        for (int x = 0; x < src.width(); x += stepX) {
            ++hist[line[x]];
            ++n;
        }
    }
    if (n <= 0)
        return 220;
    int acc = 0;
    int p95 = 255;
    const int target = static_cast<int>(0.95 * n);
    for (int i = 0; i < 256; ++i) {
        acc += hist[i];
        if (acc >= target) {
            p95 = i;
            break;
        }
    }
    // 默认略压白场，贴近 PS 自动色阶观感
    return std::max(24, std::min(255, p95 > 10 ? p95 : 96));
}

void RectPickDialog::onWhiteChanged(int white)
{
    white = std::max(1, std::min(255, white));
    if (whiteSlider_->value() != white) {
        const QSignalBlocker b1(whiteSlider_);
        whiteSlider_->setValue(white);
    }
    if (whiteSpin_->value() != white) {
        const QSignalBlocker b2(whiteSpin_);
        whiteSpin_->setValue(white);
    }
    rebuildToneMapped();
    refreshDisplay();
}

void RectPickDialog::rebuildToneMapped()
{
    if (source_.isNull()) {
        toneMapped_ = QImage();
        preview_ = QImage();
        return;
    }
    const int white = std::max(1, whiteSlider_ ? whiteSlider_->value() : 255);
    toneMapped_ = QImage(source_.size(), QImage::Format_RGB32);
    const double scale = 255.0 / white;
    for (int y = 0; y < source_.height(); ++y) {
        const uchar *in = source_.constScanLine(y);
        QRgb *out = reinterpret_cast<QRgb *>(toneMapped_.scanLine(y));
        for (int x = 0; x < source_.width(); ++x) {
            const int v = std::max(0, std::min(255, static_cast<int>(in[x] * scale + 0.5)));
            out[x] = qRgb(v, v, v);
        }
    }
    preview_ = toneMapped_;
}

void RectPickDialog::fitView()
{
    stretchFill_ = true;
    zoom_ = 1.0;
    refreshDisplay();
}

void RectPickDialog::resetZoom()
{
    // 1:1：原图像素，不拉伸
    stretchFill_ = false;
    zoom_ = 1.0;
    refreshDisplay();
}

void RectPickDialog::refreshDisplay()
{
    if (preview_.isNull() || !label_ || !scroll_) {
        if (label_)
            label_->clear();
        return;
    }
    const QSize vp = scroll_->viewport()->size();
    const int vw = std::max(2, vp.width());
    const int vh = std::max(2, vp.height());
    const int iw = std::max(1, preview_.width());
    const int ih = std::max(1, preview_.height());

    if (stretchFill_) {
        // 宽、高分别拉伸到视口，消除上下/左右黑边（略变形，框选仍按图像坐标映射）
        scaleX_ = (static_cast<double>(vw) / iw) * zoom_;
        scaleY_ = (static_cast<double>(vh) / ih) * zoom_;
    } else {
        scaleX_ = zoom_;
        scaleY_ = zoom_;
    }
    if (scaleX_ < 1e-9)
        scaleX_ = 1e-9;
    if (scaleY_ < 1e-9)
        scaleY_ = 1e-9;

    const int dw = std::max(1, static_cast<int>(iw * scaleX_ + 0.5));
    const int dh = std::max(1, static_cast<int>(ih * scaleY_ + 0.5));

    QPixmap pm = QPixmap::fromImage(preview_).scaled(dw, dh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    const int lw = std::max(vw, dw);
    const int lh = std::max(vh, dh);
    offX_ = (lw - dw) / 2;
    offY_ = (lh - dh) / 2;

    QPixmap canvas(lw, lh);
    canvas.fill(QColor(34, 34, 34));
    {
        QPainter painter(&canvas);
        painter.drawPixmap(offX_, offY_, pm);
    }
    label_->setPixmap(canvas);
    label_->resize(lw, lh);
}

QPointF RectPickDialog::labelToImage(const QPoint &p) const
{
    if (scaleX_ < 1e-9 || scaleY_ < 1e-9)
        return QPointF(0, 0);
    return QPointF((p.x() - offX_) / scaleX_, (p.y() - offY_) / scaleY_);
}

QRect RectPickDialog::labelRectToImage(const QRect &r) const
{
    const QPointF a = labelToImage(r.topLeft());
    const QPointF b = labelToImage(r.bottomRight());
    QRectF rf = QRectF(a, b).normalized();
    return rf.toAlignedRect();
}

bool RectPickDialog::eventFilter(QObject *obj, QEvent *ev)
{
    // 视口空白处滚轮也可缩放
    if (scroll_ && obj == scroll_->viewport() && ev->type() == QEvent::Wheel) {
        auto *we = static_cast<QWheelEvent *>(ev);
        const double factor = (we->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
        zoom_ = std::max(0.2, std::min(20.0, zoom_ * factor));
        refreshDisplay();
        return true;
    }
    if (obj != label_)
        return QDialog::eventFilter(obj, ev);

    if (ev->type() == QEvent::Wheel) {
        auto *we = static_cast<QWheelEvent *>(ev);
        const double factor = (we->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
        zoom_ = std::max(0.2, std::min(20.0, zoom_ * factor));
        refreshDisplay();
        return true;
    }
    if (ev->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(ev);
        if (me->button() == Qt::LeftButton) {
            origin_ = me->pos();
            band_->setGeometry(QRect(origin_, QSize()));
            band_->show();
            dragging_ = true;
            return true;
        }
    } else if (ev->type() == QEvent::MouseMove && dragging_) {
        auto *me = static_cast<QMouseEvent *>(ev);
        band_->setGeometry(QRect(origin_, me->pos()).normalized());
        return true;
    } else if (ev->type() == QEvent::MouseButtonRelease && dragging_) {
        dragging_ = false;
        return true;
    }
    return QDialog::eventFilter(obj, ev);
}

void RectPickDialog::finish(bool ok)
{
    ok_ = false;
    if (!ok) {
        reject();
        return;
    }
    const QRect rLabel = band_->geometry();
    if (rLabel.width() < 2 || rLabel.height() < 2) {
        reject();
        return;
    }
    QRect rImg = labelRectToImage(rLabel);
    rImg = rImg.intersected(QRect(0, 0, preview_.width(), preview_.height()));
    if (rImg.width() < 2 || rImg.height() < 2) {
        reject();
        return;
    }
    const int pw = std::max(1, preview_.width());
    const int ph = std::max(1, preview_.height());
    const double sx = static_cast<double>(fullNa_) / pw;
    const double sy = static_cast<double>(fullNr_) / ph;
    roi_.x = static_cast<int>(rImg.x() * sx);
    roi_.y = static_cast<int>(rImg.y() * sy);
    roi_.w = std::max(1, static_cast<int>(rImg.width() * sx));
    roi_.h = std::max(1, static_cast<int>(rImg.height() * sy));
    ok_ = true;
    accept();
}
