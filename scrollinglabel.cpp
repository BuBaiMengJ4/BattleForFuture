#include "scrollinglabel.h"

#include "logger.h"

#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPalette>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>

#include <cmath>

ScrollingLabel::ScrollingLabel(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    // 文字自带描边需要抗锯齿；控件本身不画背景
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoFillBackground(false);

    m_animation = new QPropertyAnimation(this, "scrollOffset", this);
    // 线性速度：滚动字幕用缓动反而会显得忽快忽慢
    m_animation->setEasingCurve(QEasingCurve::Linear);
    connect(m_animation, &QPropertyAnimation::finished, this, [this]() {
        // 一段滚动结束，说明已经到达某一端：先夹紧到合法范围，
        // 再进入“停留”状态。注意这里**不能**直接调用 scheduleNextMove()，
        // 否则它会立刻又启动一段动画，把停留时间挤掉。
        m_offset = qBound(0, m_offset, scrollRange());
        update();

        if (scrollRange() <= 0) {
            m_phase = Phase::Idle;
            m_offset = 0;
            update();
            return;
        }
        // 停在末端则下一步往回滚，停在起点则下一步往前滚
        m_direction = (m_offset >= scrollRange()) ? -1 : 1;
        m_phase = Phase::Pausing;
        m_pauseTimer->start(m_pauseMs);
    });

    m_pauseTimer = new QTimer(this);
    m_pauseTimer->setSingleShot(true);
    connect(m_pauseTimer, &QTimer::timeout, this, [this]() {
        const int range = scrollRange();
        if (range <= 0) {
            m_phase = Phase::Idle;
            m_offset = 0;
            update();
            return;
        }
        // 停够之后朝相反方向滚
        animateTo(m_direction > 0 ? range : 0);
    });
}

// ============================= 文本与尺寸 =============================

void ScrollingLabel::setText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    resetScroll();
    updateGeometry();
    update();
}

void ScrollingLabel::setTextColor(const QColor &color)
{
    m_textColor = color;
    update();
}

void ScrollingLabel::setPauseMs(int ms)
{
    m_pauseMs = qMax(0, ms);
    // 正处在停留阶段时立刻按新时长重新计时，避免要等一轮才生效
    if (m_phase == Phase::Pausing && m_pauseTimer->isActive())
        m_pauseTimer->start(m_pauseMs);
}

void ScrollingLabel::setSpeed(int pixelsPerSecond)
{
    m_speed = qMax(5, pixelsPerSecond);
}

int ScrollingLabel::textWidth() const
{
    if (m_text.isEmpty())
        return 0;
    return QFontMetrics(font()).horizontalAdvance(m_text);
}

int ScrollingLabel::scrollRange() const
{
    // 需要滚出的距离 = 文本宽度 - 可视宽度（放得下时为 0，即不需要滚动）
    return qMax(0, textWidth() - width());
}

QSize ScrollingLabel::sizeHint() const
{
    const QFontMetrics fm(font());
    // 理想宽度就是文字本身的宽度：布局有多少就给多少（配合 .ui 中两侧的
    // Expanding 间隔项，标签会默认居中并占满两个按钮之间的全部空间），
    // 只有确实分不到足够宽度时才退化成滚动。
    return QSize(qMax(textWidth(), 120), fm.height() + 6);
}

QSize ScrollingLabel::minimumSizeHint() const
{
    const QFontMetrics fm(font());
    return QSize(80, fm.height() + 4);
}

// ============================= 滚动调度 =============================

void ScrollingLabel::resetScroll()
{
    if (m_animation)
        m_animation->stop();
    if (m_pauseTimer)
        m_pauseTimer->stop();
    m_offset = 0;
    m_direction = 1;
    m_phase = Phase::Idle;
    // 布局可能还没完成（此时 width() 还是 0），等一次事件循环后再决定是否滚动
    QTimer::singleShot(0, this, [this]() { scheduleNextMove(); });
}

void ScrollingLabel::scheduleNextMove()
{
    if (!isVisible() || m_text.isEmpty()) {
        m_phase = Phase::Idle;
        return;
    }

    // 已经在滚动或已经在停留：交给各自的 finished/timeout 回调继续，
    // 这里绝不重复启动，否则停留时间会被下一段动画挤掉。
    if (m_phase == Phase::Scrolling || m_phase == Phase::Pausing)
        return;

    if (scrollRange() <= 0) {
        // 文字放得下：停在起点居中显示，不滚动也不空转
        m_offset = 0;
        m_direction = 1;
        m_phase = Phase::Idle;
        update();
        return;
    }

    // 空闲且需要滚动：从起点开始，先停一下再滚（避免刚出现就动起来太突兀）
    m_offset = 0;
    m_direction = 1;
    m_phase = Phase::Pausing;
    m_pauseTimer->start(m_pauseMs);
}

void ScrollingLabel::animateTo(int targetOffset)
{
    const int range = scrollRange();
    const int target = qBound(0, targetOffset, range);
    const int distance = qAbs(target - m_offset);
    if (distance <= 0) {
        // 长度刚好变化导致无需移动：直接按“已到端点”处理
        m_phase = Phase::Idle;
        scheduleNextMove();
        return;
    }

    // 时长按“距离 / 速度”计算，保证不同长度的文字滚动速度一致
    const int durationMs = qMax(40, int(std::lround(distance * 1000.0 / m_speed)));

    m_phase = Phase::Scrolling;
    m_animation->stop();
    m_animation->setDuration(durationMs);
    m_animation->setStartValue(m_offset);
    m_animation->setEndValue(target);
    m_animation->start();
}

void ScrollingLabel::applyOffset(int offset)
{
    m_offset = offset;
    update();
}

void ScrollingLabel::setScrollOffset(int offset)
{
    applyOffset(offset);
}

// ============================= 事件 =============================

void ScrollingLabel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    if (m_text.isEmpty())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setFont(font());

    const QFontMetrics fm(font());
    const int textW = fm.horizontalAdvance(m_text);

    // 文字整体上移偏移量：偏移 0 时左对齐，偏移增大时内容左移。
    // 当文字比控件窄时居中显示（不滚动的情况下视觉最自然）。
    int x = -m_offset;
    if (textW <= width())
        x = (width() - textW) / 2;

    const int baseline = (height() + fm.ascent() - fm.descent()) / 2;
    // 优先用显式颜色：主界面背景是深色星空图，而调色板 WindowText 在 Windows 上是
    // 纯黑 —— 直接用调色板会导致文字“埋”在背景里看不见（看起来就像没有推送内容）。
    painter.setPen(m_textColor.isValid() ? m_textColor
                                         : palette().color(QPalette::WindowText));
    painter.drawText(QPointF(x, baseline), m_text);
}

void ScrollingLabel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // 尺寸变化会改变可滚动距离，重新计算并回到起点，避免停在越界位置
    resetScroll();
}

void ScrollingLabel::changeEvent(QEvent *event)
{
    // 字体/调色板变化后文字宽度会变，需要重新测量
    if (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange
        || event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        QWidget::changeEvent(event);
        resetScroll();
        return;
    }
    QWidget::changeEvent(event);
}

void ScrollingLabel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    resetScroll();
}

void ScrollingLabel::hideEvent(QHideEvent *event)
{
    // 隐藏时停掉动画与停留计时，不必为一个看不见的标签耗电；
    // 同时把状态机复位，避免恢复显示后停在一个再也推进不下去的状态。
    m_animation->stop();
    m_pauseTimer->stop();
    m_phase = Phase::Idle;
    QWidget::hideEvent(event);
}

void ScrollingLabel::wheelEvent(QWheelEvent *event)
{
    // 标签自身不处理滚轮，交回父级（设置页需要用它滚动）
    event->ignore();
    QWidget::wheelEvent(event);
}
