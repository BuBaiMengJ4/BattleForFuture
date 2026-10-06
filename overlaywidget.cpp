#include "overlaywidget.h"
#include "logger.h"

#include <QQuickWidget>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QApplication>
#include <QScreen>
#include <QMouseEvent>
#include <QUrl>

namespace {

/// 样式 2 的 QML 源码（内嵌，避免部署时找不到 .qml 文件）
const char *const kOverlayQml = R"QML_SRC(
import QtQuick

// ============================================================
// 小组件样式 2：无边框、无标题栏的透明覆盖层内容（由 OverlayWidget 嵌入）
// - 真透明背景：未绘制像素 alpha=0，不再依赖 QSS "background: transparent"
//   （Linux 碎片化环境下 QSS 透明会变黑，详见 overlaywidget.h 注释）
// - 显示 "还有 N 天到一模/高考"，数字为七段 LCD 风格
// - textColor / lcdColor 可由用户在设置中自定义（样式 1、2 通用）
// ============================================================

Item {
    id: root

    // 由 C++ 通过 QQmlContext 动态属性注入
    property color textColor: overlayTextColor
    property color lcdColor:  overlayLcdColor
    property int   days:      overlayDays
    property string unitText: overlayUnit

    signal returnToMain()

    // 整个区域可交互：双击返回主界面；按下后由宿主 QWidget 负责拖拽移动窗口
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onDoubleClicked: root.returnToMain()
    }

    // ---- 单个七段数码管数字 ----
    component SegmentDigit: Item {
        id: digit
        required property string charValue
        width: 56
        height: 60

        // 每个字符点亮的段集合
        readonly property string pattern: {
            switch (charValue) {
            case "0": return "abcdef";
            case "1": return "bc";
            case "2": return "abged";
            case "3": return "abgcd";
            case "4": return "fgbc";
            case "5": return "afgcd";
            case "6": return "afgedc";
            case "7": return "abc";
            case "8": return "abcdefg";
            case "9": return "abcfgd";
            default:  return "";
            }
        }

        function lit(segName) { return pattern.indexOf(segName) >= 0; }

        // 横向段 a(顶) / g(中) / d(底)：亮段用用户 LCD 颜色，暗段半透明白底纹
        Repeater {
            model: [
                { name: "a", x: 6,  y: 2 },
                { name: "g", x: 10, y: 27 },
                { name: "d", x: 6,  y: 52 }
            ]
            delegate: Rectangle {
                required property var modelData
                x: modelData.x
                y: modelData.y
                width: 44
                height: 6
                radius: 3
                antialiasing: true
                color: digit.lit(modelData.name) ? root.lcdColor : "#1affffff"
            }
        }

        // 纵向段 f/b（上排）、e/c（下排）
        Repeater {
            model: [
                { name: "f", x: 4,  y: 6 },
                { name: "b", x: 42, y: 6 },
                { name: "e", x: 4,  y: 33 },
                { name: "c", x: 42, y: 33 }
            ]
            delegate: Rectangle {
                required property var modelData
                x: modelData.x
                y: modelData.y
                width: 6
                height: 24
                radius: 3
                antialiasing: true
                color: digit.lit(modelData.name) ? root.lcdColor : "#1affffff"
            }
        }
    }

    // ---- 布局：还有 / 数字 / 天到XX（与样式 1 文案一致）----
    Column {
        anchors.centerIn: parent
        spacing: 6

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("还有")
            color: root.textColor
            font.pixelSize: 26
            font.bold: true
            style: Text.Outline          // 半透明描边保证在任意壁纸上可读
            styleColor: "#99000000"
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8

            Repeater {
                // 去掉前导零；0 天时显示单个 "0"
                model: root.days <= 0 ? ["0"] : String(root.days).split("")
                delegate: SegmentDigit {
                    required property string modelData
                    charValue: modelData
                }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.unitText
            color: root.textColor
            font.pixelSize: 26
            font.bold: true
            style: Text.Outline
            styleColor: "#99000000"
        }
    }
}
)QML_SRC";

} // namespace

OverlayWidget::OverlayWidget(const QColor &textColor,
                             const QColor &lcdColor,
                             bool showYiMo,
                             const QDate &gaokaoDate,
                             const QDate &yimoDate,
                             QWidget *parent)
    : QWidget(parent)
{
    // 顶层窗口：无边框 + 无标题栏 + Tool（不占任务栏）+ 置顶便于预览
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    // 原生 RGBA 表面：Qt Quick 场景直接渲染到带 Alpha 的窗口缓冲，
    // 在 Windows(DWM)/Wayland/有合成器的 X11 上均为真透明（不会变黑）。
    if (!QSurfaceFormat::defaultFormat().hasAlpha()) {
        QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
        fmt.setAlpha(8);
        QSurfaceFormat::setDefaultFormat(fmt);
    }
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(260, 180);
    setCursor(Qt::SizeAllCursor); // 提示整体可拖动（与样式 1 一致）

    // ---- QQuickWidget 承载透明内容 ----
    m_quick = new QQuickWidget(this);
    m_quick->setGeometry(0, 0, width(), height());
    m_quick->setResizeMode(QQuickWidget::SizeRootObjectToView);
    // 关键修复：Quick 视口背景色必须是透明色。
    // 默认 QQuickWidget 背景为 palette Base（深色主题/部分 Linux 后端下即黑色），
    // 这正是 "QSS transparent 变黑" 的常见根因之一。
    m_quick->setColor(Qt::transparent);
    m_quick->setStyleSheet(QStringLiteral("_q_qwindowsystemproxy { background: transparent; }"));

    // 数据经上下文属性注入（一次性创建，无需运行时反复 rebind）
    m_context = new QQmlContext(m_quick->rootContext(), m_quick);
    m_context->setContextProperty(QStringLiteral("overlayTextColor"), textColor);
    m_context->setContextProperty(QStringLiteral("overlayLcdColor"),  lcdColor);
    m_context->setContextProperty(QStringLiteral("overlayDays"),
                                  remainingDays(showYiMo ? yimoDate : gaokaoDate));
    m_context->setContextProperty(QStringLiteral("overlayUnit"),
                                  showYiMo ? tr("天到一模") : tr("天到高考"));

    m_component = new QQmlComponent(m_quick->engine(), m_quick);
    connect(m_component, &QQmlComponent::statusChanged,
            this, &OverlayWidget::onQmlStatusChanged);
    m_component->setData(QByteArray(kOverlayQml), QUrl(QStringLiteral("overlay-style2.qml")));
    if (m_component->isReady())
        createOverlayObject(); // 同步编译成功时立即创建

    // 初始位置：屏幕右下角（保持原有习惯，之后可用鼠标拖到任意位置）
    if (QScreen *screen = QApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        move(avail.right() - width() - 12, avail.bottom() - height() - 12);
    }

    Logger::info(QStringLiteral("桌面小组件（样式2·Qt Quick 透明覆盖层）已创建"));
}

void OverlayWidget::onQmlStatusChanged()
{
    if (m_component->isError()) {
        Logger::critical(QStringLiteral("样式2 QML 加载失败: %1").arg(m_component->errorString()));
        return;
    }
    if (m_component->isReady())
        createOverlayObject();
}

void OverlayWidget::createOverlayObject()
{
    if (m_quick->contentItem()->childItems().size() > 0)
        return; // 防重复创建

    QObject *obj = m_component->create(m_context);
    if (!obj) {
        Logger::critical(QStringLiteral("样式2 QML 对象创建失败: %1").arg(m_component->errorString()));
        return;
    }
    auto *item = qobject_cast<QQuickItem *>(obj);
    if (!item) {
        Logger::critical(QStringLiteral("样式2 QML 根对象不是 QQuickItem"));
        obj->deleteLater();
        return;
    }

    item->setParentItem(m_quick->contentItem());
    connect(item, SIGNAL(returnToMain()), this, SIGNAL(returnToMainRequested()));
}

int OverlayWidget::remainingDays(const QDate &targetDate)
{
    if (!targetDate.isValid())
        return 0;
    const int days = QDate::currentDate().daysTo(targetDate);
    return days > 0 ? days : 0;
}

// ---------------- 拖拽移动（与样式 1 行为一致）----------------

void OverlayWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void OverlayWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        QPoint newPos = event->globalPosition().toPoint() - m_dragPos;
        if (QScreen *screen = QApplication::primaryScreen()) {
            const QRect avail = screen->availableGeometry();
            newPos.setX(qBound(avail.left(), newPos.x(), avail.right() - width()));
            newPos.setY(qBound(avail.top(), newPos.y(), avail.bottom() - height()));
        }
        move(newPos);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void OverlayWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        Logger::debug(QStringLiteral("样式2小组件移动到 (%1, %2)").arg(x()).arg(y()));
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}
