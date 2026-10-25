#ifndef SLIDINGNAVLIST_H
#define SLIDINGNAVLIST_H

#include <QListWidget>

class QPropertyAnimation;

/**
 * @brief 带“滑动高亮”的分类导航列表
 *
 * 交互需求：切换分类时，选中项的背景色与左侧蓝色竖条不要直接跳变，
 * 而是从原分类**平滑滑到**目标分类。
 *
 * 实现方式：
 * - 用一个 `qreal` 属性 `highlightPos` 表示高亮的“行坐标”（可以是小数，
 *   例如 1.35 表示正处在第 1、2 行之间 35% 的位置），由 QPropertyAnimation 驱动；
 * - 绘制交给本类自己：先把控件的默认绘制跑完（选中项背景设为透明，
 *   见调用方设置的样式表），再用插值出来的矩形画高亮条与蓝色竖条；
 * - 行矩形取自 `visualItemRect()`，因此行高、间距、滚动都能自动适配。
 *
 * 只做绘制层动画，不改变任何布局或项的几何状态，切换过程不会破坏界面结构。
 */
class SlidingNavList : public QListWidget
{
    Q_OBJECT
    /// 高亮所在的“行坐标”（0 = 第一行，1 = 第二行……可以是小数）
    Q_PROPERTY(qreal highlightPos READ highlightPos WRITE setHighlightPos)

public:
    explicit SlidingNavList(QWidget *parent = nullptr);

    qreal highlightPos() const { return m_highlightPos; }

    /// 高亮背景色（默认 #34495e）
    void setHighlightColor(const QColor &color) { m_highlightColor = color; update(); }
    /// 左侧竖条颜色（默认 #3498db）
    void setAccentColor(const QColor &color) { m_accentColor = color; update(); }
    /// 动画时长（毫秒）
    void setAnimationDuration(int ms) { m_duration = qMax(0, ms); }

public slots:
    void setHighlightPos(qreal pos);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    /// 把“行坐标”换算成高亮条的实际矩形（在 viewport 坐标系里）
    QRect highlightRectForPos(qreal pos) const;

    qreal m_highlightPos = 0.0;
    QColor m_highlightColor {52, 73, 94};      // #34495e
    QColor m_accentColor {52, 152, 219};       // #3498db
    int    m_duration = 200;
    QPropertyAnimation *m_animation = nullptr;
};

#endif // SLIDINGNAVLIST_H
