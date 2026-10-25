#ifndef DESKTOPWIDGET_H
#define DESKTOPWIDGET_H

#include <QColor>
#include <QDate>
#include <QPoint>
#include <QWidget>

class QString;

/**
 * @brief 桌面小组件样式 1：圆角卡片（可自定义背景色）
 *
 * 显示“还有 / N / 天到高考(或一模)”，数字使用 LCD 七段码，
 * 底部提供“退出”“返回主界面”两个按钮，整窗可用鼠标拖动。
 *
 * 透明实现说明（修复 Linux 下 transparent 变黑）：
 * - 会话支持逐像素 Alpha（Windows DWM / macOS / Wayland / 带合成器的 X11）时，
 *   WA_TranslucentBackground + 抗锯齿圆角路径，得到真正的圆角与半透明按钮；
 * - 不支持时（无合成器的 X11，透明像素会被填成黑色）回退为
 *   “不透明圆角区域 + QRegion 遮罩”，圆角由系统裁剪而不是靠 Alpha，
 *   因此任何桌面环境下都不会出现黑块。
 */
class DesktopWidget : public QWidget
{
    Q_OBJECT

public:
    DesktopWidget(const QColor &backgroundColor,
                  const QColor &textColor,
                  const QColor &lcdColor,
                  bool showYiMo,
                  const QDate &gaokaoDate,
                  const QDate &yimoDate,
                  int fontPixelSize = 22,
                  bool alwaysOnTop = true,
                  const QString &configPath = QString(),
                  QWidget *parent = nullptr);

    /// 记录位置的配置文件路径（为空表示不记忆位置）
    void setConfigPath(const QString &path) { m_configPath = path; }

signals:
    /// 用户点击“返回主界面”
    void returnToMainRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    /// 依据当前尺寸重建圆角遮罩（仅无逐像素 Alpha 的回退方案使用）
    void rebuildMask();
    /// 恢复上次记录的位置；没有记录或记录已失效时退回屏幕右上角。
    /// 窗口必须先在目标位置显示出来，再调用本函数。
    void restoreSavedPosition();
    static int remainingDays(const QDate &targetDate);

    QPoint m_dragPos;                // 按下时鼠标全局位置与窗口左上角的偏移
    bool   m_dragging = false;       // 是否正在拖拽
    bool   m_dragMoved = false;      // 本次拖拽是否真的移动过（避免无意义的配置写入）
    int    m_radius = 16;            // 圆角半径
    bool   m_alphaOk = true;         // 是否支持逐像素 Alpha（否则走遮罩回退）
    QString m_configPath;            // 位置记忆用的 config.ini 路径（可空）
};

#endif // DESKTOPWIDGET_H
