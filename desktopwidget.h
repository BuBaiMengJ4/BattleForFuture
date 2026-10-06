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
 * @brief 桌面小组件·样式 1（可移动、真圆角、无阴影）
 *
 * - 通过鼠标拖拽任意位置移动窗口；
 * - 使用逐像素 Alpha 透明实现真正的圆角；在缺少合成器的 Linux 会话上
 *   自动回退为 setMask 遮罩方案（避免 transparent 变黑，见构造函数注释）；
 * - 文字颜色 / LCD 数字颜色 / 背景色均可由用户在设置中自定义。
 */
class DesktopWidget : public QDialog
{
    Q_OBJECT

public:
    DesktopWidget(const QColor &backgroundColor,
                  const QColor &textColor,
                  const QColor &lcdColor,
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
    void rebuildMask();            // 依据当前尺寸重建圆角遮罩（无合成器回退方案）
    static int remainingDays(const QDate &targetDate); // 计算剩余天数（过期返回 0）

    QPoint m_dragPos;              // 按下时鼠标全局位置与窗口左上角的偏移
    bool   m_dragging = false;     // 是否正在拖拽
    int    m_radius = 18;          // 圆角半径
    QColor m_bgColor;              // 小组件背景色
    QColor m_textColor;            // 文字颜色（用户自定义）
    QColor m_lcdColor;             // LCD 数字颜色（用户自定义）
    bool   m_alphaOk = true;       // 平台是否支持逐像素 Alpha（否则用 mask 回退）

    QLCDNumber *m_lcd = nullptr;
    QLabel     *m_titleLabel = nullptr;
    QLabel     *m_unitLabel = nullptr;
    QPushButton *m_exitBtn = nullptr;
    QPushButton *m_returnBtn = nullptr;
};

#endif // DESKTOPWIDGET_H
