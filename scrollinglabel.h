#ifndef SCROLLINGLABEL_H
#define SCROLLINGLABEL_H

#include <QColor>
#include <QFont>
#include <QWidget>

class QPropertyAnimation;
class QTimer;

/**
 * @brief 会左右滚动的单行文字标签（替代主界面底部的 QLabel）
 *
 * 解决的问题：主界面底部（两个按钮之间）显示随机鼓励语句，语句一长就超出
 * 标签宽度，多出来的部分**直接看不见**，用户根本不知道还有后半句。
 *
 * 现在的行为：
 * - 文字放得下时：正常居中显示，不滚动、不打扰；
 * - 文字超宽时：从右向左匀速滚动，让用户读完；滚到末尾后**停 5 秒**，
 *   再朝相反方向滚回起点，到起点后再停 5 秒，如此往复；
 * - 文字变化（换了一条鼓励语）或控件尺寸变化时，自动回到起点重新开始。
 *
 * 实现上只用一个 QPropertyAnimation 驱动一个整数偏移量 + 一个单次 QTimer
 * 负责“停留 5 秒”，没有额外线程，也不会在文字放得下时空转。
 */
class ScrollingLabel : public QWidget
{
    Q_OBJECT
    /// 当前滚动偏移（像素，>= 0 表示向右滚出的距离）
    Q_PROPERTY(int scrollOffset READ scrollOffset WRITE setScrollOffset)
    /// 文字颜色；未设置时退回调色板 WindowText
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)

public:
    explicit ScrollingLabel(QWidget *parent = nullptr);

    QString text() const { return m_text; }
    int     scrollOffset() const { return m_offset; }
    /// 每次到达一端的停留时间（毫秒），默认 5000
    int     pauseMs() const { return m_pauseMs; }
    QColor  textColor() const { return m_textColor; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setText(const QString &text);
    void setPauseMs(int ms);
    /// 设置滚动速度（像素/秒）
    void setSpeed(int pixelsPerSecond);
    /// 设置文字颜色（不设置则使用调色板颜色）
    void setTextColor(const QColor &color);
    /// 回到起点并重新判断是否需要滚动
    void resetScroll();
    /// 滚动偏移量的 setter（供 QPropertyAnimation 通过属性系统驱动）
    void setScrollOffset(int offset);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    /// 滚轮事件不应被标签吞掉（避免挡住父级滚动）
    void wheelEvent(QWheelEvent *event) override;

private:
    /// 滚动状态：明确区分“正在滚”“正在停留”“空闲”，
    /// 避免用“偏移量是否落在端点”去反推动画与停留的先后关系（那样两者会互相抢占）
    enum class Phase {
        Idle,      ///< 不需要滚动（文字放得下）或未显示
        Scrolling, ///< 动画进行中
        Pausing    ///< 停在某一端，正在等待停留时间结束
    };

    /// 文本总宽度
    int textWidth() const;
    /// 需要滚动的距离（0 表示不需要滚动）
    int scrollRange() const;
    /// 依据当前状态决定是继续滚、还是进入停留
    void scheduleNextMove();
    /// 启动一段滚动动画
    void animateTo(int targetOffset);
    /// 立刻把偏移量应用到界面
    void applyOffset(int offset);

    QString m_text;
    int     m_offset = 0;              ///< 当前偏移
    int     m_direction = 1;           ///< 1 = 滚向末端，-1 = 滚回起点
    int     m_pauseMs = 5000;          ///< 每端停留时长（默认 5 秒）
    int     m_speed = 40;              ///< 滚动速度（像素/秒）
    QColor  m_textColor;               ///< 无效时使用调色板 WindowText
    Phase   m_phase = Phase::Idle;

    QPropertyAnimation *m_animation = nullptr;
    QTimer             *m_pauseTimer = nullptr;
};

#endif // SCROLLINGLABEL_H
