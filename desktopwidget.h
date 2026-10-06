#ifndef DESKTOPWIDGET_H
#define DESKTOPWIDGET_H

#include <QColor>
#include <QDialog>
#include <QDate>
#include <QPoint>

class QLCDNumber;
class QLabel;
class QPushButton;

/**
 * @brief 桌面小组件（可移动、真圆角、无阴影）
 *
 * - 通过鼠标拖拽任意位置移动窗口；
 * - 使用逐像素 Alpha 透明实现真正的圆角（四周不再使用 QGraphicsDropShadowEffect 阴影，
 *   避免旧实现中“阴影矩形 + 样式表圆角不生效”的假圆角问题）；
 * - 支持从配置读取背景色与显示内容（高考/一模倒计时）。
 */
class DesktopWidget : public QDialog
{
    Q_OBJECT

public:
    DesktopWidget(const QColor &backgroundColor,
                  bool showYiMo,
                  const QDate &gaokaoDate,
                  const QDate &yimoDate,
                  QWidget *parent = nullptr);

signals:
    /// 用户点击“返回主界面”
    void returnToMainRequested();

protected:
    // 事件重写：拖拽移动
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    // 事件重写：真圆角遮罩
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuildMask();            // 依据当前尺寸重建圆角遮罩
    static int remainingDays(const QDate &targetDate); // 计算剩余天数（过期返回 0）

    QPoint m_dragPos;              // 按下时鼠标全局位置与窗口左上角的偏移
    bool   m_dragging = false;     // 是否正在拖拽
    int    m_radius = 18;          // 圆角半径
    QColor m_bgColor;              // 小组件背景色

    QLCDNumber *m_lcd = nullptr;
    QLabel     *m_titleLabel = nullptr;
    QLabel     *m_unitLabel = nullptr;
    QPushButton *m_exitBtn = nullptr;
    QPushButton *m_returnBtn = nullptr;
};

#endif // DESKTOPWIDGET_H
