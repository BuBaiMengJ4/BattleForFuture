#include "slidingnavlist.h"

#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>

#include <cmath>

namespace {
/// 左侧蓝色竖条宽度（与样式表里的 border-left 观感一致）
constexpr int kAccentWidth = 3;
/// 高亮背景圆角
constexpr qreal kCornerRadius = 6.0;
} // namespace

SlidingNavList::SlidingNavList(QWidget *parent)
    : QListWidget(parent)
{
    m_animation = new QPropertyAnimation(this, "highlightPos", this);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);

    // 切换分类时把高亮条滑过去。
    // 注意：这里用 currentRowChanged 而不是重写 currentChanged()，
    // 后者在初始化阶段会被反复调用，容易在首帧就触发动画。
    connect(this, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0)
            return;

        // 目标位置直接用行号；起点是当前（可能是动画中间值的）位置，
        // 因此快速连点时动画会从“当前滑到哪”继续滑，不会跳回原点。
        const qreal from = m_highlightPos;
        const qreal to = qreal(row);
        if (qFuzzyCompare(from + 1.0, to + 1.0)) {
            setHighlightPos(to);
            return;
        }

        m_animation->stop();
        m_animation->setDuration(m_duration);
        m_animation->setStartValue(from);
        m_animation->setEndValue(to);
        m_animation->start();
    });
}

void SlidingNavList::setHighlightPos(qreal pos)
{
    if (qFuzzyCompare(m_highlightPos + 1.0, pos + 1.0))
        return;
    m_highlightPos = pos;
    viewport()->update();
}

QRect SlidingNavList::highlightRectForPos(qreal pos) const
{
    // 注意：局部变量不能叫 count，否则会遮蔽 QListWidget::count() 成员函数
    const int itemCount = count();
    if (itemCount <= 0)
        return QRect();

    // 把行坐标夹到 [0, itemCount-1]，避免越界取不到 item
    if (pos < 0.0)
        pos = 0.0;
    if (pos > itemCount - 1)
        pos = itemCount - 1;

    const int rowA = int(std::floor(pos));
    const int rowB = qMin(rowA + 1, itemCount - 1);
    const qreal f = pos - rowA;

    QListWidgetItem *itemA = this->item(rowA);
    QListWidgetItem *itemB = this->item(rowB);
    if (!itemA)
        return QRect();

    const QRect rectA = visualItemRect(itemA);
    const QRect rectB = itemB ? visualItemRect(itemB) : rectA;

    // 线性插值：行高可能不同（多行/不同字体），按比例过渡看起来最自然
    const qreal top = rectA.top() + (rectB.top() - rectA.top()) * f;
    const qreal height = rectA.height() + (rectB.height() - rectA.height()) * f;

    return QRectF(viewport()->rect().left(),
                  top,
                  viewport()->width(),
                  height).toAlignedRect();
}

void SlidingNavList::paintEvent(QPaintEvent *event)
{
    // ============================ 绘制顺序很关键 ============================
    // 高亮条必须画在**文字下面**。
    // 之前是先调 QListWidget::paintEvent（画文字）再补画高亮条，结果高亮
    // 直接盖住了选中项的文字 —— 用户看到的就是“背景色和文字色一样、看不清”。
    //
    // 现在的顺序：
    //   1) 先铺高亮背景（不画文字）
    //   2) 再让 QListWidget 正常绘制各项文字
    // 选中项背景在样式表里设为 transparent，所以基础绘制不会再补一块
    // 与滑动高亮冲突的底色；文字则始终画在高亮之上，清晰可读。
    // ====================================================================
    if (count() > 0) {
        const QRect band = highlightRectForPos(m_highlightPos);
        if (!band.isEmpty()) {
            QPainter painter(viewport());
            painter.setRenderHint(QPainter::Antialiasing, true);

            // 1) 高亮背景：整行宽度，圆角
            QPainterPath bandPath;
            bandPath.addRoundedRect(QRectF(band).adjusted(0, 1, -1, -1),
                                    kCornerRadius, kCornerRadius);
            painter.fillPath(bandPath, m_highlightColor);

            // 2) 左侧蓝色竖条：贴着左边缘，随高亮一起滑动
            painter.fillRect(QRect(band.left() + 1, band.top() + 1,
                                   kAccentWidth, band.height() - 2),
                             m_accentColor);
        }
    }

    // 文字最后绘制（覆盖在高亮之上）
    QListWidget::paintEvent(event);
}
