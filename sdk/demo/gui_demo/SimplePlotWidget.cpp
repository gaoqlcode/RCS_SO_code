// =============================================================================
// SimplePlotWidget.cpp — 在 paintEvent 里把 (x_, y_) 映射到像素并画折线
// =============================================================================

#include "SimplePlotWidget.h" // 本类声明
#include <QPainter>           // 2D 绘制：填充、画线、文字
#include <QtMath>             // qFuzzyCompare 等（本文件主要用 qMin/qMax，含在 QtGlobal）

// 构造：初始化纵轴锁定标志与占位范围；规定最小尺寸以免布局挤成一条线
SimplePlotWidget::SimplePlotWidget(QWidget *parent)
    : QWidget(parent)   // 交给 QWidget 管父子与几何
    , fixedY_(false)    // 默认不锁纵轴，按数据自动算 ymin/ymax
    , yLo_(0)           // 锁定模式下的下限占位
    , yHi_(1)           // 锁定模式下的上限占位
{
    // 布局里至少占这么大；实际尺寸由布局拉伸决定
    setMinimumSize(400, 220);

    // 允许用调色板背景填充，配合 paintEvent 里先刷白更干净
    setAutoFillBackground(true);
}

// 保存轴标题并请求重绘
void SimplePlotWidget::setAxisTitles(const QString &x, const QString &y)
{
    xTitle_ = x; // 横轴文字，如「时间 (μs)」
    yTitle_ = y; // 纵轴文字，如「幅度」
    update();    // 异步触发 paintEvent，不要在这里直接 QPainter
}

// 保存图标题并请求重绘
void SimplePlotWidget::setTitle(const QString &t)
{
    title_ = t; // 如「HRRP」或带峰值信息的字符串
    update();   // 标记脏区，下一帧事件循环里重绘
}

// 设置是否锁定纵轴；lo < hi 才锁定
void SimplePlotWidget::setYRange(double lo, double hi)
{
    fixedY_ = (lo < hi); // 只有合法区间才视为锁定（频域图常用 -60~3）
    yLo_ = lo;           // 记下下限
    yHi_ = hi;           // 记下上限
    update();            // 范围变了要重画
}

// 换一组曲线数据；拷贝进成员，随后重绘
void SimplePlotWidget::setData(const QVector<double> &x, const QVector<double> &y)
{
    x_ = x; // 横轴采样
    y_ = y; // 纵轴采样
    update(); // 数据更新 → 刷新显示
}

// 每次需要显示时由 Qt 调用；参数未用（整窗重绘即可）
void SimplePlotWidget::paintEvent(QPaintEvent *)
{
    // 绑定到本控件的绘制器；离开作用域自动结束绘制
    QPainter p(this);

    // 先铺白底，清掉上一次的内容
    p.fillRect(rect(), Qt::white);

    // 绘图区：左边留刻度、上边留标题、右边/底边留边距
    // left=70, top=36, width=总宽-90, height=总高-70
    const QRect plot(70, 36, width() - 90, height() - 70);

    // 黑色画笔：画坐标框和文字
    p.setPen(Qt::black);

    // 坐标轴外框
    p.drawRect(plot);

    // 顶部居中画标题
    p.drawText(QRect(0, 6, width(), 24), Qt::AlignCenter, title_);

    // 底部居中画横轴标题（相对整个控件底部）
    p.drawText(QRect(plot.left(), height() - 28, plot.width(), 20), Qt::AlignCenter, xTitle_);

    // 点数不够画折线就只显示空框和标题
    if (x_.size() < 2 || y_.size() < 2)
        return;

    // 用第一个点初始化数据范围，后面循环扩展
    double xmin = x_[0], xmax = x_[0], ymin = y_[0], ymax = y_[0];

    // 有效点数取 x/y 较短者，防止越界
    const int n = qMin(x_.size(), y_.size());

    // 扫描全部点，求 min/max
    for (int i = 1; i < n; ++i) {
        xmin = qMin(xmin, x_[i]); // 横轴最小
        xmax = qMax(xmax, x_[i]); // 横轴最大
        ymin = qMin(ymin, y_[i]); // 纵轴最小
        ymax = qMax(ymax, y_[i]); // 纵轴最大
    }

    // 若用户锁定了纵轴，覆盖自动算的 ymin/ymax
    if (fixedY_) {
        ymin = yLo_;
        ymax = yHi_;
    }

    // 横轴塌成一点时人为拉开，避免除零
    if (qFuzzyCompare(xmin, xmax)) {
        xmin -= 1;
        xmax += 1;
    }

    // 纵轴塌成一点时同样拉开
    if (qFuzzyCompare(ymin, ymax)) {
        ymin -= 1;
        ymax += 1;
    }

    // 折线用蓝色稍粗一点
    p.setPen(QPen(QColor(30, 90, 180), 1.5));

    // 把每个数据点映射到 plot 矩形内的像素坐标
    QPolygonF line;
    for (int i = 0; i < n; ++i) {
        // nx∈[0,1]：相对横轴位置
        const double nx = (x_[i] - xmin) / (xmax - xmin);
        // ny∈[0,1]：相对纵轴位置（数据大 → 靠近 1）
        const double ny = (y_[i] - ymin) / (ymax - ymin);
        // 屏幕 y 轴向下，所以用 bottom - ny*height
        line << QPointF(plot.left() + nx * plot.width(), plot.bottom() - ny * plot.height());
    }

    // 一次画出整条折线
    p.drawPolyline(line);

    // 恢复黑色，写纵轴上下限数字
    p.setPen(Qt::black);
    p.drawText(QRect(4, plot.top(), 60, 16), Qt::AlignRight, QString::number(ymax, 'f', 1));
    p.drawText(QRect(4, plot.bottom() - 16, 60, 16), Qt::AlignRight, QString::number(ymin, 'f', 1));

    // 纵轴标题要竖排：保存画家状态 → 平移到中线 → 旋转 -90° → 画字 → 恢复
    p.save();
    p.translate(14, plot.center().y()); // 移到绘图区左侧中点附近
    p.rotate(-90);                      // 逆时针 90°，文字沿纵轴方向
    // 在旋转后的坐标系里居中画 yTitle_
    p.drawText(QRect(-plot.height() / 2, -10, plot.height(), 20), Qt::AlignCenter, yTitle_);
    p.restore(); // 恢复变换与画笔，避免影响后续（本函数末尾无更多绘制）
}
