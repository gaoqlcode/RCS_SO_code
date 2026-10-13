#include "RectPickDialog.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QRubberBand>
#include <QScrollArea>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

RectPickDialog::RectPickDialog(const QImage &preview, int fullNr, int fullNa, const QString &title,
                               QWidget *parent)
    : QDialog(parent)
    , scroll_(0)
    , label_(0)
    , band_(0)
    , scaleX_(1.0)
    , scaleY_(1.0)
    , offX_(0)
    , offY_(0)
    , fullNr_(fullNr)
    , fullNa_(fullNa)
    , ok_(false)
    , dragging_(false)
{
    setWindowTitle(title);
    setWindowModality(Qt::ApplicationModal);
    setWindowFlags((windowFlags() | Qt::Window) & ~Qt::WindowContextHelpButtonHint);
    resize(1000, 720);
    setMinimumSize(640, 480);

    preview_ = preview.convertToFormat(QImage::Format_Grayscale8);
    if (preview_.isNull())
        preview_ = preview.convertToFormat(QImage::Format_RGB32).convertToFormat(QImage::Format_Grayscale8);

    auto *hint = new QLabel(
        QStringLiteral("拖矩形后点「确定」。ROI 会换算为全分辨率像素（x=方位列,y=距离行）。"), this);
    hint->setWordWrap(true);

    scroll_ = new QScrollArea(this);
    scroll_->setWidgetResizable(false);
    scroll_->setAlignment(Qt::AlignCenter);
    scroll_->setStyleSheet(QStringLiteral("QScrollArea{background:#222;}"));

    label_ = new QLabel;
    label_->setAlignment(Qt::AlignCenter);
    label_->setMouseTracking(true);
    label_->installEventFilter(this);
    label_->setStyleSheet(QStringLiteral("background:#222;"));
    scroll_->setWidget(label_);
    band_ = new QRubberBand(QRubberBand::Rectangle, label_);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, [this]() { finish(true); });
    connect(btns, &QDialogButtonBox::rejected, this, [this]() { finish(false); });

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(hint);
    lay->addWidget(scroll_, 1);
    lay->addWidget(btns);

    refreshDisplay();
}

void RectPickDialog::refreshDisplay()
{
    if (preview_.isNull() || !label_ || !scroll_)
        return;
    const QSize vp = scroll_->viewport()->size();
    const int vw = std::max(2, vp.width());
    const int vh = std::max(2, vp.height());
    const int iw = std::max(1, preview_.width());
    const int ih = std::max(1, preview_.height());
    scaleX_ = static_cast<double>(vw) / iw;
    scaleY_ = static_cast<double>(vh) / ih;
    const int dw = std::max(1, static_cast<int>(iw * scaleX_ + 0.5));
    const int dh = std::max(1, static_cast<int>(ih * scaleY_ + 0.5));
    QPixmap pm =
        QPixmap::fromImage(preview_).scaled(dw, dh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
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

bool RectPickDialog::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj != label_)
        return QDialog::eventFilter(obj, ev);
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
    } else if (ev->type() == QEvent::Resize) {
        refreshDisplay();
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
    const QPointF a = labelToImage(rLabel.topLeft());
    const QPointF b = labelToImage(rLabel.bottomRight());
    QRect rImg = QRectF(a, b).normalized().toAlignedRect();
    rImg = rImg.intersected(QRect(0, 0, preview_.width(), preview_.height()));
    if (rImg.width() < 2 || rImg.height() < 2) {
        reject();
        return;
    }
    const int pw = std::max(1, preview_.width());
    const int ph = std::max(1, preview_.height());
    const double sx = static_cast<double>(fullNa_) / pw;
    const double sy = static_cast<double>(fullNr_) / ph;
    roi_.x = static_cast<int>(std::floor(rImg.x() * sx));
    roi_.y = static_cast<int>(std::floor(rImg.y() * sy));
    roi_.w = std::max(1, static_cast<int>(std::floor(rImg.width() * sx)));
    roi_.h = std::max(1, static_cast<int>(std::floor(rImg.height() * sy)));
    ok_ = true;
    accept();
}
