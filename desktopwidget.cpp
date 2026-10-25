#include "desktopwidget.h"

#include "platformsupport.h"
#include "logger.h"
#include "widgetstyle.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLCDNumber>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegion>
#include <QScreen>

#include <cmath>

namespace {

/// 与主界面 ui 中的 QLCDNumber 保持一致的配色默认值：背景透明、数字用用户颜色
QString lcdStyleSheet(const QColor &lcdColor)
{
    return QStringLiteral("QLCDNumber { color: %1; background: transparent; border: none; }")
        .arg(lcdColor.name(QColor::HexRgb));
}

} // namespace

DesktopWidget::DesktopWidget(const QColor &backgroundColor,
                             const QColor &textColor,
                             const QColor &lcdColor,
                             bool showYiMo,
                             const QDate &gaokaoDate,
                             const QDate &yimoDate,
                             int fontPixelSize,
                             bool alwaysOnTop,
                             const QString &configPath,
                             QWidget *parent)
    : QWidget(parent)
    , m_configPath(configPath)
{
    // 无边框 + Tool：不占任务栏，可当桌面小组件。
    // WindowStaysOnTopHint 由设置里的“始终显示在最上层”控制（默认开启）。
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool
                   | (alwaysOnTop ? Qt::WindowStaysOnTopHint : Qt::WindowFlags()));
    setObjectName(QStringLiteral("DesktopWidget"));
    setFixedSize(250, 200);
    setCursor(Qt::SizeAllCursor); // 提示整窗可拖动

    // ---- Linux 碎片化兼容：探测逐像素 Alpha 是否可用 ----
    // 不支持时（无合成器的 X11）透明像素会被渲染成黑色，回退为遮罩方案。
    m_alphaOk = PlatformSupport::supportsPerPixelAlpha();
    if (m_alphaOk) {
        setAttribute(Qt::WA_TranslucentBackground);
    } else {
        // 关键：不要设置 WA_TranslucentBackground，否则四角会被填成黑色。
        // 改为“整窗不透明绘制 + 圆角遮罩裁剪”，由系统裁掉圆角外像素。
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAttribute(Qt::WA_NoSystemBackground);
    }

    // ---- 颜色配置（非法值一律回退到默认，避免出现不可见的小组件）----
    // 默认背景取中性深色，与默认 LCD 颜色（青绿）形成足够对比，
    // 避免旧版“背景和数字都是纯红、数字几乎看不见”的问题。
    const QColor bg = backgroundColor.isValid() ? backgroundColor : QColor(26, 43, 74);
    const QColor fg = textColor.isValid() ? textColor : QColor(240, 240, 240);
    const QColor lcd = lcdColor.isValid() ? lcdColor : QColor(0, 230, 160);
    const int fontSize = WidgetConfig::clampFontSize(fontPixelSize > 0 ? fontPixelSize : 22);

    QFont titleFont(QStringLiteral("宋体"), fontSize);
    // 半透明卡片可能压在任意壁纸上：给文字加描边保证可读性
    QColor outline = (fg.lightness() > 128) ? QColor(0, 0, 0, 150) : QColor(255, 255, 255, 150);

    auto makeLabel = [&](const QString &text) {
        QLabel *label = new QLabel(text, this);
        label->setFont(titleFont);
        label->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                 .arg(fg.name(QColor::HexRgb)));
        label->setAttribute(Qt::WA_TranslucentBackground);
        return label;
    };

    QLabel *titleLabel = makeLabel(tr("还有"));
    titleLabel->setGeometry(10, 2, 90, fontSize + 12);

    QLCDNumber *lcdNumber = new QLCDNumber(this);
    lcdNumber->setDigitCount(3);
    lcdNumber->setSegmentStyle(QLCDNumber::Flat);
    lcdNumber->setFrameShape(QFrame::NoFrame);
    lcdNumber->setStyleSheet(lcdStyleSheet(lcd));
    lcdNumber->setAttribute(Qt::WA_TranslucentBackground);
    lcdNumber->setGeometry(20, 34, 210, 96);
    lcdNumber->display(remainingDays(showYiMo && yimoDate.isValid() ? yimoDate : gaokaoDate));

    const QString unitText = (showYiMo && yimoDate.isValid()) ? tr("天到一模") : tr("天到高考");
    QLabel *unitLabel = makeLabel(unitText);
    unitLabel->setGeometry(70, 132, 160, fontSize + 12);

    // ---- 按钮：文字颜色跟随用户设置，底色自适应可读性 ----
    const QString btnStyle = QStringLiteral(
        "QPushButton {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "   border-radius: 4px;"
        "   padding: 1px;"
        "}"
        "QPushButton:hover { background-color: %4; }")
        .arg(fg.lightness() > 128 ? QStringLiteral("rgba(0,0,0,60)")
                                  : QStringLiteral("rgba(255,255,255,60)"),
             fg.name(QColor::HexRgb),
             outline.name(QColor::HexArgb),
             fg.lightness() > 128 ? QStringLiteral("rgba(0,0,0,110)")
                                  : QStringLiteral("rgba(255,255,255,110)"));

    QPushButton *exitBtn = new QPushButton(tr("退出"), this);
    exitBtn->setGeometry(10, 172, 60, 22);
    exitBtn->setStyleSheet(btnStyle);
    exitBtn->setCursor(Qt::PointingHandCursor);

    QPushButton *returnBtn = new QPushButton(tr("返回主界面"), this);
    returnBtn->setGeometry(130, 172, 110, 22);
    returnBtn->setStyleSheet(btnStyle);
    returnBtn->setCursor(Qt::PointingHandCursor);

    connect(exitBtn, &QPushButton::clicked, qApp, &QApplication::quit);
    connect(returnBtn, &QPushButton::clicked, this, &DesktopWidget::returnToMainRequested);

    // 初始位置：有记录则恢复上次位置，否则默认屏幕右上角；
    // 拖动结束后会把新位置写回配置（见 mouseReleaseEvent）。
    restoreSavedPosition();

    if (!m_alphaOk)
        rebuildMask();

    // paintEvent 需要读取的两个颜色（不作为成员保存，避免类接口膨胀）
    setProperty("backgroundColor", bg);
    setProperty("outlineColor", outline);

    Logger::info(QStringLiteral("桌面小组件·样式1（圆角卡片）已创建：文字 %1 / LCD %2 / 背景 %3 / 字号 %4 / "
                                "透明模式 %5 / 位置 (%6, %7)")
                     .arg(fg.name(), lcd.name(), bg.name())
                     .arg(fontSize)
                     .arg(m_alphaOk ? QStringLiteral("逐像素Alpha")
                                    : QStringLiteral("遮罩回退"))
                     .arg(x())
                     .arg(y()));
}

void DesktopWidget::restoreSavedPosition()
{
    // 默认位置：屏幕右上角（沿用原有习惯）
    QScreen *primary = QApplication::primaryScreen();
    const QRect primaryAvail = primary ? primary->availableGeometry() : QRect(0, 0, 1920, 1080);
    move(primaryAvail.right() - width(), primaryAvail.top());

    if (m_configPath.isEmpty())
        return;

    int savedX = 0;
    int savedY = 0;
    if (!WidgetConfig::loadPosition(m_configPath, &savedX, &savedY))
        return; // 从未记录过：保持右上角

    // 关键：必须在窗口已经显示出来之后再判断它落在哪块屏幕上，
    // 否则 QGuiApplication::screenAt() 拿不到正确的屏幕（窗口还没创建平台窗口）。
    // 因此这里先把窗口放到记录的位置，等真正 show() 之后由校正逻辑兜底。
    move(savedX, savedY);
    Logger::info(QStringLiteral("已恢复上次记录的小组件位置 (%1, %2)").arg(savedX).arg(savedY));
}

int DesktopWidget::remainingDays(const QDate &targetDate)
{
    if (!targetDate.isValid())
        return 0;
    const int days = QDate::currentDate().daysTo(targetDate);
    return days > 0 ? days : 0;
}

// ---------------- 拖拽移动 ----------------
void DesktopWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragMoved = false;
        // 记录鼠标全局位置与窗口左上角的偏移，保证拖动跟手
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void DesktopWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        const QPoint newPos = event->globalPosition().toPoint() - m_dragPos;
        if (newPos != pos()) {
            move(newPos);
            m_dragMoved = true;
        }
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void DesktopWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        event->accept();

        if (m_dragMoved) {
            Logger::debug(QStringLiteral("样式1小组件移动到 (%1, %2)").arg(x()).arg(y()));
            // 只在真的移动过时写配置：单击不该产生磁盘写入
            if (!m_configPath.isEmpty())
                WidgetConfig::savePosition(m_configPath, x(), y());
        }
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

// ---------------- 真圆角 ----------------
void DesktopWidget::rebuildMask()
{
    // 用几何 Region 做窗口遮罩：圆角之外的像素被系统直接裁掉。
    // 相比逐像素扫描位图生成遮罩，QPainterPath 转 Region 由 Qt 内部
    // 做扫描线填充，尺寸固定为 250x200，开销可以忽略。
    const int r = m_radius;
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, width(), height()), r, r);
    setMask(QRegion(path.toFillPolygon().toPolygon()));
}

void DesktopWidget::resizeEvent(QResizeEvent *event)
{
    if (!m_alphaOk)
        rebuildMask(); // 仅在无逐像素 Alpha 的回退方案下需要遮罩
    QWidget::resizeEvent(event);
}

void DesktopWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    const QColor bg = qvariant_cast<QColor>(property("backgroundColor"));
    const QColor outline = qvariant_cast<QColor>(property("outlineColor"));

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen borderPen(outline, 1);
    borderPen.setCapStyle(Qt::RoundCap);
    painter.setPen(borderPen);

    if (m_alphaOk) {
        // 正常路径：抗锯齿圆角矩形，四角因逐像素 Alpha 而真正透明（无阴影）
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), m_radius, m_radius);
        painter.fillPath(path, bg);
        painter.drawPath(path);
    } else {
        // 回退路径（无合成器的 Linux 会话）：不透明填充整窗，圆角由 setMask 裁掉
        painter.fillRect(rect(), bg);
    }
}
