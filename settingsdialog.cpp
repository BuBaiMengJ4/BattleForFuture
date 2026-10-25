#include "settingsdialog.h"

#include "diagnostics.h"
#include "logger.h"
#include "restarthelper.h"
#include "slidingnavlist.h"
#include "version.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QProcess>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTextEdit>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>
#include <QWheelEvent>

#include <memory>

namespace {

/// 分类表：名称与提示集中定义，新增/调整分类只需要改这一处
struct Category {
    const char *title;    ///< 左侧导航文字（需要翻译）
    const char *subtitle; ///< 导航项下方的小字说明（作为 tooltip）
    const char *iconPath; ///< 资源路径（空表示无图标）
};

const Category kCategories[] = {
    { QT_TRANSLATE_NOOP("SettingsDialog", "常规设置"),   QT_TRANSLATE_NOOP("SettingsDialog", "启动 · 语言 · 考试年份"),"" },
    { QT_TRANSLATE_NOOP("SettingsDialog", "小组件设置"), QT_TRANSLATE_NOOP("SettingsDialog", "样式 · 内容 · 颜色"),"" },
    { QT_TRANSLATE_NOOP("SettingsDialog", "文字编辑"),   QT_TRANSLATE_NOOP("SettingsDialog", "首页鼓励语句"),"" },
    { QT_TRANSLATE_NOOP("SettingsDialog", "关于"),       QT_TRANSLATE_NOOP("SettingsDialog", "版本与项目信息"),"" }
};

constexpr int kCategoryCount = int(sizeof(kCategories) / sizeof(kCategories[0]));

/// 统一的 ComboBox 样式，避免大段重复字符串
const char *const kComboStyle =
    "QComboBox {"
    "   padding: 5px 8px;"
    "   border: 1px solid #d0d7de;"
    "   border-radius: 6px;"
    "   background-color: white;"
    "   color: #24292f;"
    "   min-height: 22px;"
    "}"
    "QComboBox:focus { border-color: #3498db; }"
    "QComboBox::drop-down { border: none; width: 22px; }"
    "QComboBox QAbstractItemView {"
    "   padding: 4px;"
    "   border: 1px solid #3498db;"
    "   border-radius: 6px;"
    "   color: #24292f;"
    "   background-color: white;"
    "   selection-background-color: #3498db;"
    "   selection-color: white;"
    "   outline: none;"
    "}";

const char *const kGroupBoxStyle =
    "QGroupBox {"
    "   font-weight: bold;"
    "   color: #2c3e50;"
    "   border: 1px solid #dfe4ea;"
    "   border-radius: 10px;"
    "   margin-top: 14px;"
    "   padding: 14px 14px 10px 14px;"
    "   background-color: #ffffff;"
    "}"
    "QGroupBox::title {"
    "   subcontrol-origin: margin;"
    "   subcontrol-position: top left;"
    "   left: 12px;"
    "   padding: 0 6px;"
    "}";

const char *const kCheckBoxStyle =
    "QCheckBox { spacing: 10px; color: #24292f; font-size: 14px; background: transparent; }"
    "QCheckBox::indicator {"
    "   width: 20px; height: 20px;"
    "   border-radius: 5px;"
    "   border: 1.5px solid #cccccc;"
    "   background-color: white;"
    "}"
    "QCheckBox::indicator:hover { border-color: #0078d7; background-color: #f0f7ff; }"
    // 勾选态使用 checkmark.svg：该 SVG 自带蓝色圆角底 + 白色对勾，
    // 因此这里把边框也一起交给图片，避免出现“方框里再套一个圆角矩形”的双层边框。
    "QCheckBox::indicator:checked {"
    "   border: none;"
    "   background-color: transparent;"
    "   image: url(:/icon/checkmark.svg);"
    "}"
    "QCheckBox::indicator:checked:hover { border: none; background-color: transparent; }"
    "QCheckBox::indicator:disabled { border: 1.5px solid #eeeeee; background-color: #f9f9f9; }"
    "QCheckBox::indicator:checked:disabled {"
    "   border: none;"
    "   background-color: transparent;"
    "   image: url(:/icon/checkmark.svg);"
    "}"
    "QCheckBox:disabled { color: #a0a0a0; }";

/**
 * @brief 统一的滚动条样式
 *
 * 为什么需要它？
 * 不给滚动条写样式时，Qt 会落到当前平台样式的默认实现；在 Windows 上
 * 那就是经典的 3D 立体滚动条（浅灰凹槽 + 立体箭头按钮），也就是用户看到的
 * “Windows 3.x 风格”。这里统一改成扁平胶囊样式：
 * 透明凹槽 + 圆角滑块 + 悬停加深，并且隐藏 Qt 默认的增减箭头线。
 *
 * 直接挂在 QScrollArea / QTextEdit 这类可滚动控件上即可生效。
 */
const char *const kScrollBarStyle =
    "QScrollBar:vertical {"
    "   background: transparent;"
    "   width: 10px;"
    "   margin: 0px;"
    "   border: none;"
    "}"
    "QScrollBar::handle:vertical {"
    "   background: #c9d1d9;"
    "   min-height: 32px;"
    "   border-radius: 5px;"
    "}"
    "QScrollBar::handle:vertical:hover { background: #a9b4c0; }"
    "QScrollBar::handle:vertical:pressed { background: #8c98a6; }"
    "QScrollBar:horizontal {"
    "   background: transparent;"
    "   height: 10px;"
    "   margin: 0px;"
    "   border: none;"
    "}"
    "QScrollBar::handle:horizontal {"
    "   background: #c9d1d9;"
    "   min-width: 32px;"
    "   border-radius: 5px;"
    "}"
    "QScrollBar::handle:horizontal:hover { background: #a9b4c0; }"
    "QScrollBar::add-line, QScrollBar::sub-line {"
    "   height: 0px; width: 0px; border: none; background: none;"
    "}"
    "QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }";

/// 滚轮滚动区域：让滚轮事件真正作用于滚动条（Qt 默认对某些子控件不转发）
const char *const kScrollAreaStyle =
    "QScrollArea { background: transparent; border: none; }"
    "QScrollArea > QWidget > QWidget { background: transparent; }";

const char *const kPrimaryButtonStyle =
    "QPushButton {"
    "   background-color: #3498db;"
    "   color: white;"
    "   padding: 8px 18px;"
    "   border: none;"
    "   border-radius: 6px;"
    "   font-weight: bold;"
    "}"
    "QPushButton:hover { background-color: #2980b9; }"
    "QPushButton:pressed { background-color: #21618c; }"
    "QPushButton:disabled { background-color: #bdc3c7; color: #ecf0f1; }";

/// 统一的颜色按钮样式：底色即当前颜色，禁用时给出明显的灰色示意
QString colorButtonStyle(const QString &objectName, const QColor &color)
{
    const QString textColor = color.lightness() > 128 ? QStringLiteral("#24292f")
                                                      : QStringLiteral("#ffffff");
    return QStringLiteral(
               "QPushButton#%1 {"
               "   background-color: %2;"
               "   color: %3;"
               "   border: 1px solid rgba(0,0,0,60);"
               "   padding: 7px 10px;"
               "   border-radius: 6px;"
               "   font-weight: bold;"
               "}"
               "QPushButton#%1:hover { border: 1px solid #3498db; }"
               "QPushButton#%1:disabled {"
               "   background-color: #f2f4f6;"
               "   color: #a0a6ad;"
               "   border: 1px dashed #c8ced4;"
               "}")
        .arg(objectName, color.name(QColor::HexRgb), textColor);
}

/// QSettings 安全包装（任何异常都不向外传播）
template <typename Fn>
void withSettings(const QString &path, Fn &&fn) noexcept
{
    try {
        QSettings settings(path, QSettings::IniFormat);
        fn(settings);
    } catch (...) {
        Logger::critical(QStringLiteral("读写配置文件失败: %1").arg(path));
    }
}

#ifdef Q_OS_LINUX
/// Linux 开机自启动的 desktop 文件路径（仅 Linux 使用，其他平台不编译以免告警）
QString linuxAutoStartFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                        + QStringLiteral("/autostart");
    return dir + QStringLiteral("/battleforfuture.desktop");
}
#endif

} // namespace

// ============================= 构造 =============================

SettingsDialog::SettingsDialog(const QString &configPath, const QString &quotesFilePath, QWidget *parent)
    : QDialog(parent)
    , m_configPath(configPath)
    , m_quotesFilePath(quotesFilePath)
{
    setWindowTitle(tr("应用设置"));
    setWindowIcon(QIcon(QStringLiteral(":/icon/WindowIcon.ico")));
    setMinimumSize(720, 480);
    resize(760, 520);

    // 对话框自身的背景必须显式指定：Linux 上若只依赖主题调色板，
    // 部分桌面环境（深色主题 + 无合成器）会把未绘制区域画成黑色。
    setStyleSheet(QStringLiteral("QDialog { background-color: #f5f7fa; }"));

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // ---------------- 左侧分类导航 ----------------
    QFrame *leftFrame = new QFrame(this);
    leftFrame->setObjectName(QStringLiteral("navFrame"));
    leftFrame->setStyleSheet(QStringLiteral(
        "QFrame#navFrame { background-color: #2c3e50; border: none; }"));
    leftFrame->setFixedWidth(212);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftFrame);
    leftLayout->setContentsMargins(0, 14, 0, 14);
    leftLayout->setSpacing(0);

    QLabel *navTitle = new QLabel(tr("设置"), leftFrame);
    navTitle->setStyleSheet(QStringLiteral(
        "color: #ecf0f1; font-size: 18px; font-weight: bold; padding: 0 18px 12px 18px;"));
    leftLayout->addWidget(navTitle);

    m_categoryList = new SlidingNavList(leftFrame);
    m_categoryList->setFrameShape(QFrame::NoFrame);
    m_categoryList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_categoryList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // 分类配图：Settings.png / Widget.png 已打进可执行文件（resource.qrc）
    m_categoryList->setIconSize(QSize(22, 22));
    m_categoryList->setAnimationDuration(200);
    m_categoryList->setHighlightColor(QColor(52, 73, 94));    // #34495e
    m_categoryList->setAccentColor(QColor(52, 152, 219));     // #3498db
    // 关键：选中项的**背景与左侧竖条设为透明**，交给 SlidingNavList 自己画，
    // 这样高亮才能在两行之间平滑滑动，而不是“旧行瞬间消失、新行瞬间出现”。
    // 文字颜色/字重仍由样式表负责（切换时立即变白，观感自然）。
    m_categoryList->setStyleSheet(QStringLiteral(
        "QListWidget { background: transparent; border: none; outline: none; }"
        "QListWidget::item {"
        "   color: #d6dde3;"
        "   padding: 10px 18px;"
        "   border-left: none;"
        "}"
        "QListWidget::item:selected {"
        "   background: transparent;"
        "   color: #ffffff;"
        "   border-left: none;"
        "   font-weight: bold;"
        "}"
        "QListWidget::item:hover:!selected { background-color: #31465c; color: #ffffff; }"));
    for (int i = 0; i < kCategoryCount; ++i) {
        QListWidgetItem *item = new QListWidgetItem(m_categoryList);
        item->setText(tr(kCategories[i].title));
        if (kCategories[i].iconPath && *kCategories[i].iconPath) {
            const QIcon icon(QString::fromLatin1(kCategories[i].iconPath));
            if (!icon.isNull())
                item->setIcon(icon);
            else
                Logger::warning(QStringLiteral("设置分类图标加载失败: %1")
                                    .arg(QString::fromLatin1(kCategories[i].iconPath)));
        }
        // 副标题放进 tooltip：列表项保持单行，视觉更干净
        item->setToolTip(QStringLiteral("%1\n%2").arg(tr(kCategories[i].title),
                                                      tr(kCategories[i].subtitle)));
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    leftLayout->addWidget(m_categoryList, 1);

    QLabel *navFooter = new QLabel(tr("BattleForFuture %1").arg(QStringLiteral(APP_VERSION)), leftFrame);
    navFooter->setStyleSheet(QStringLiteral("color: #7f8c9b; font-size: 11px; padding: 8px 18px 0 18px;"));
    leftLayout->addWidget(navFooter);

    // ---------------- 右侧内容区 ----------------
    m_pages = new QStackedWidget(this);
    m_pages->setStyleSheet(QStringLiteral("QStackedWidget { background: transparent; }"));

    mainLayout->addWidget(leftFrame);
    mainLayout->addWidget(m_pages, 1);

    // 分类切换：页面首次访问时才构建（惰性加载），切换时左右滑动
    connect(m_categoryList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= kCategoryCount)
            return;
        showPage(row, true);
    });

    m_categoryList->setCurrentRow(0);
    // currentRowChanged 在 row 已经是 0 时不会触发，这里显式同步一次，
    // 保证初始页面一定被构建并显示（首次不做动画）。
    showPage(0, false);
}

// ============================= 页面调度 =============================

QWidget *SettingsDialog::pageAt(int index)
{
    if (index < 0 || index >= kCategoryCount)
        return nullptr;

    // 惰性构建：只有用户真正打开过的分类才会创建控件。
    // 注意页面加入 m_pages 的顺序取决于用户的访问顺序，所以这里必须用
    // “分类索引 -> 页面指针”的映射表，而不能假设 m_pages->widget(index) 对应分类 index。
    if (QWidget *cached = m_pageCache.value(index, nullptr))
        return cached;

    QWidget *page = nullptr;
    switch (index) {
    case 0: page = createGeneralPage();  break;
    case 1: page = createWidgetPage();   break;
    case 2: page = createWordEditPage(); break;
    case 3: page = createAboutPage();    break;
    default: break;
    }
    if (!page)
        return nullptr;

    m_pageCache.insert(index, page);
    m_pages->addWidget(page);
    Logger::debug(QStringLiteral("设置页已构建: %1").arg(tr(kCategories[index].title)));
    return page;
}

void SettingsDialog::showPage(int index, bool animate)
{
    Logger::debug(QStringLiteral("[设置] 请求切换分类 -> 索引 %1（动画=%2）")
                      .arg(index)
                      .arg(animate ? 1 : 0));
    Diagnostics::Step stepSwitch(QStringLiteral("设置页切换"));

    QWidget *next = pageAt(index);
    if (!next) {
        Logger::warning(QStringLiteral("[设置] 分类 %1 没有可用页面，已忽略本次切换").arg(index));
        return;
    }

    const int stackIndex = m_pages->indexOf(next);
    if (stackIndex < 0) {
        Logger::warning(QStringLiteral("[设置] 分类 %1 的页面不在页面栈中，已忽略本次切换").arg(index));
        return;
    }

    QWidget *current = m_pages->currentWidget();
    const bool samePage = (current == next);

    // 方向：目标索引比当前大 -> 从右侧滑入
    const int direction = (index >= m_currentCategory) ? 1 : -1;
    m_currentCategory = index;

    Diagnostics::setStage(QStringLiteral("设置页切换/切换当前页"));

    if (!animate || !current || samePage) {
        // 首次显示 / 同一页 / 显式要求不动画：直接切换
        cleanupPageSlide();
        m_pages->setCurrentIndex(stackIndex);
        Diagnostics::setStage(QString());
        Logger::debug(QStringLiteral("[设置] 分类 %1 已直接显示（页面栈索引 %2）")
                          .arg(index)
                          .arg(stackIndex));
        return;
    }

    // ============================ 左右滑动切换 ============================
    // 历史教训：早期用 QGraphicsOpacityEffect 给整页做淡入，会在切换完成后的
    // 重绘阶段触发访问违例（日志捕获到 0xC0000005）。原因是它强制把整棵子控件树
    // 渲染到离屏 pixmap，而设置页里有 QScrollArea + 大量带样式表的控件。
    //
    // 现在改为「快照 + 两个 QLabel 位移」：完全不碰布局，也不申请图形特效，
    // 只是把两页各截一张图，让它们一左一右地滑过去，结束后删掉临时控件。
    //
    // 顺序很关键：**先** setCurrentIndex，**再**创建覆盖层。
    // 因为 QStackedWidget::setCurrentIndex 会把目标页 raise() 到最上层；
    // 若覆盖层先生成，就会被新页面盖住 —— 表现正是“还没滑过去新页内容就出现，
    // 看起来像两页叠在一起”。
    // =====================================================================
    m_pages->setCurrentIndex(stackIndex);
    animatePageSwitch(current, next, direction);

    Diagnostics::setStage(QString());
    Logger::debug(QStringLiteral("[设置] 分类 %1 切换完成（页面栈索引 %2，方向 %3）")
                      .arg(index)
                      .arg(stackIndex)
                      .arg(direction > 0 ? QStringLiteral("左滑") : QStringLiteral("右滑")));
}

void SettingsDialog::animatePageSwitch(QWidget *from, QWidget *to, int direction)
{
    cleanupPageSlide();

    const QRect area = m_pages->rect();
    if (!from || !to || area.width() <= 4 || area.height() <= 4) {
        // 还没有有效尺寸（例如对话框尚未显示）：不做动画
        return;
    }

    // ---- 1) 按内容区尺寸重新摆放两页，再截图 ----
    // from 此刻已被 setCurrentIndex 隐藏，直接 grab() 可能拿到旧几何，
    // 因此显式 resize 并激活布局后再截。
    auto snap = [&area](QWidget *w) {
        w->resize(area.size());
        if (QLayout *lay = w->layout())
            lay->activate();
        return w->grab();
    };
    const QPixmap fromPix = snap(from);
    const QPixmap toPix = snap(to);
    if (fromPix.isNull() || toPix.isNull())
        return;

    // ---- 2) 动画期间把真实页面藏起来，画面完全由两张快照构成 ----
    // 只靠 raise() 不够稳：QStackedWidget / 布局在动画期间一旦被激活
    // （例如内部控件尺寸变化触发 layout 更新），会把当前页重新抬到最上层，
    // 于是就出现“还没滑过去，新页内容已经露出来 / 两页叠在一起”。
    // 直接把真实页面隐藏，快照又恰好铺满整个内容区，就没有可露出的底图了。
    to->hide();

    // ---- 3) 覆盖层（延迟到页面隐藏之后再 raise，确保在最上层）----
    auto makeOverlay = [this](const QPixmap &pix, const QRect &geo) {
        auto *label = new QLabel(m_pages);
        label->setPixmap(pix);
        label->setScaledContents(true);
        label->setGeometry(geo);
        label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        label->show();
        label->raise();
        return label;
    };

    m_slideLabelFrom = makeOverlay(fromPix, area);
    m_slideLabelTo = makeOverlay(toPix, area.translated(direction * area.width(), 0));

    // ---- 3) 两个位移动画：旧页滑出，新页滑入 ----
    const int duration = 220;
    auto *animFrom = new QPropertyAnimation(m_slideLabelFrom, "pos", this);
    animFrom->setDuration(duration);
    animFrom->setEasingCurve(QEasingCurve::OutCubic);
    animFrom->setStartValue(m_slideLabelFrom->pos());
    animFrom->setEndValue(area.topLeft() - QPoint(direction * area.width(), 0));

    auto *animTo = new QPropertyAnimation(m_slideLabelTo, "pos", this);
    animTo->setDuration(duration);
    animTo->setEasingCurve(QEasingCurve::OutCubic);
    animTo->setStartValue(m_slideLabelTo->pos());
    animTo->setEndValue(area.topLeft());

    // 先隐藏旧页的覆盖层到结束时统一清理；新页动画结束就清理即可
    connect(animTo, &QPropertyAnimation::finished, this, [this]() { cleanupPageSlide(); });

    animFrom->start(QAbstractAnimation::DeleteWhenStopped);
    animTo->start(QAbstractAnimation::DeleteWhenStopped);
}

void SettingsDialog::cleanupPageSlide()
{
    // 动画期间真实页面被临时隐藏（只显示两张快照），这里必须恢复它的可见性，
    // 否则动画结束后内容区会变成空白。
    if (m_pages) {
        if (QWidget *current = m_pages->currentWidget())
            current->show();
    }

    // 删除临时覆盖层。动画对象设置了 DeleteWhenStopped，会自行释放；
    // 这里只需把它们的目标控件从场景里移除（stop 由父对象析构兜底）。
    if (m_slideLabelFrom) {
        m_slideLabelFrom->hide();
        m_slideLabelFrom->deleteLater();
        m_slideLabelFrom = nullptr;
    }
    if (m_slideLabelTo) {
        m_slideLabelTo->hide();
        m_slideLabelTo->deleteLater();
        m_slideLabelTo = nullptr;
    }
    // 动画对象以 this 为父，未停止的会在控件被删除后自动失效并销毁
}

// ============================= 布局小工具 =============================

QLabel *SettingsDialog::makePageTitle(const QString &text)
{
    QLabel *label = new QLabel(text);
    label->setStyleSheet(QStringLiteral(
        "font-size: 19px; font-weight: bold; color: #2c3e50; padding-bottom: 2px;"));
    return label;
}

QLabel *SettingsDialog::makeHint(const QString &text)
{
    QLabel *label = new QLabel(text);
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 12px;"));
    return label;
}

QPushButton *SettingsDialog::makeColorButton(const QString &objectName)
{
    QPushButton *button = new QPushButton;
    button->setObjectName(objectName);
    button->setMinimumWidth(160);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

void SettingsDialog::paintColorButton(QPushButton *button, const QString &label, const QColor &color)
{
    if (!button)
        return;
    button->setProperty("currentColor", color);
    button->setText(QStringLiteral("%1  %2").arg(label, color.name(QColor::HexRgb).toUpper()));
    button->setStyleSheet(colorButtonStyle(button->objectName(), color));
    // 悬停提示：说明这个颜色作用在哪里，以及怎么改
    button->setToolTip(tr("%1：点击打开取色板\n当前值 %2 (R%3 G%4 B%5)")
                           .arg(label, color.name(QColor::HexRgb).toUpper())
                           .arg(color.red())
                           .arg(color.green())
                           .arg(color.blue()));
}

// ============================= 配置读写 =============================

QString SettingsDialog::readValue(const QString &key, const QString &fallback) const
{
    QString result = fallback;
    withSettings(m_configPath, [&](QSettings &s) { result = s.value(key, fallback).toString(); });
    return result;
}

void SettingsDialog::writeValue(const QString &key, const QVariant &value)
{
    withSettings(m_configPath, [&](QSettings &s) {
        s.setValue(key, value);
        s.sync();
    });
}

int SettingsDialog::readAcademyYear() const
{
    const int currentYear = QDate::currentDate().year();
    bool ok = false;
    const int parsed = readValue(QStringLiteral("Global/academicyear")).toInt(&ok);
    if (ok && parsed >= currentYear && parsed <= currentYear + 6)
        return parsed;
    return currentYear;
}

void SettingsDialog::emitWidgetConfig()
{
    emit widgetConfigChanged(WidgetConfig::load(m_configPath));
}

// ============================= 常规设置页 =============================

QWidget *SettingsDialog::createGeneralPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *outer = new QVBoxLayout(page);
    outer->setContentsMargins(24, 20, 24, 20);
    outer->setSpacing(14);

    outer->addWidget(makePageTitle(tr("常规设置")));

    // ---- 启动选项 ----
    QGroupBox *startupGroup = new QGroupBox(tr("启动选项"));
    startupGroup->setStyleSheet(QLatin1String(kGroupBoxStyle));
    QVBoxLayout *startupLayout = new QVBoxLayout(startupGroup);
    startupLayout->setSpacing(10);

    m_autoStartCheck = new QCheckBox(tr("开机自动启动"));
    m_autoStartCheck->setStyleSheet(QLatin1String(kCheckBoxStyle));
    m_autoStartCheck->setChecked(autoStartEnabled());
#ifdef Q_OS_WINDOWS
    m_autoStartCheck->setToolTip(tr("在注册表 HKCU\\...\\CurrentVersion\\Run 中登记本程序，登录后自动运行"));
#elif defined(Q_OS_LINUX)
    m_autoStartCheck->setToolTip(tr("在 ~/.config/autostart 下创建 battleforfuture.desktop 自启动项"));
#else
    m_autoStartCheck->setToolTip(tr("当前平台不支持开机自启动设置"));
#endif
#ifndef Q_OS_WINDOWS
    // Linux/macOS 通过 autostart 文件实现，同样支持；其它平台保持可用性提示
#endif
    connect(m_autoStartCheck, &QCheckBox::toggled, this, [this](bool checked) {
        applyAutoStart(checked);
    });
    startupLayout->addWidget(m_autoStartCheck);

    QHBoxLayout *modeRow = new QHBoxLayout;
    QLabel *modeLabel = new QLabel(tr("启动后直接进入："));
    m_startupModeCombo = new QComboBox;
    m_startupModeCombo->setStyleSheet(QLatin1String(kComboStyle));
    m_startupModeCombo->addItem(tr("主窗口（推荐）"));
    m_startupModeCombo->addItem(tr("桌面小组件"));
    m_startupModeCombo->setToolTip(tr("程序启动后直接显示的界面：\n"
                                      "主窗口：完整界面，适合日常查看\n"
                                      "桌面小组件：直接显示桌面上的倒计时"));
    // 配置语义：Start/StartAsWidget = true 表示启动时直接进入小组件
    const bool startAsWidget = readValue(QStringLiteral("Start/StartAsWidget"),
                                         QStringLiteral("false")).compare(QLatin1String("true"),
                                                                           Qt::CaseInsensitive) == 0;
    m_startupModeCombo->setCurrentIndex(startAsWidget ? 1 : 0);
    connect(m_startupModeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        writeValue(QStringLiteral("Start/StartAsWidget"), index == 1);
        Logger::info(QStringLiteral("启动形态更新为: %1")
                         .arg(index == 1 ? tr("桌面小组件") : tr("主窗口")));
    });
    modeRow->addWidget(modeLabel);
    modeRow->addWidget(m_startupModeCombo, 1);
    startupLayout->addLayout(modeRow);
    startupLayout->addWidget(makeHint(tr("若已有一个实例在运行，再次启动会把已有窗口带到前台，而不是另开一个窗口。")));

    outer->addWidget(startupGroup);

    // ---- 语言与考试年份 ----
    QGroupBox *targetGroup = new QGroupBox(tr("语言与考试年份"));
    targetGroup->setStyleSheet(QLatin1String(kGroupBoxStyle));
    QVBoxLayout *targetLayout = new QVBoxLayout(targetGroup);
    targetLayout->setSpacing(10);

    QHBoxLayout *langRow = new QHBoxLayout;
    langRow->addWidget(new QLabel(tr("显示语言：")));
    m_languageCombo = new QComboBox;
    m_languageCombo->setStyleSheet(QLatin1String(kComboStyle));
    m_languageCombo->addItem(QStringLiteral("简体中文"));
    m_languageCombo->addItem(QStringLiteral("繁體中文"));
    m_languageCombo->setToolTip(tr("界面显示语言：\n"
                                   "简体中文：源码原文\n"
                                   "繁體中文：在简体基础上运行时转换为繁体\n"
                                   "切换语言需要重启程序才能完整生效。"));
    const int savedLang = readValue(QStringLiteral("Global/language"), QStringLiteral("0")).toInt();
    m_languageCombo->setCurrentIndex(qBound(0, savedLang, m_languageCombo->count() - 1));
    connect(m_languageCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        writeValue(QStringLiteral("Global/language"), index);
        Logger::info(QStringLiteral("语言设置更新为索引 %1").arg(index));

        // 语言是启动时装载翻译器的：改完必须重启才能完整生效，
        // 这里直接问用户要不要立刻重启，避免“改了没反应”的困惑。
        const QString languageName = (index == 1) ? QStringLiteral("繁體中文")
                                                  : QStringLiteral("简体中文");
        const QMessageBox::StandardButton reply = QMessageBox::question(
            this, tr("需要重启"),
            tr("界面语言已切换为「%1」。\n\n翻译在程序启动时加载，需要重启程序才能完整生效。\n"
               "是否现在重启？")
                .arg(languageName),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (reply == QMessageBox::Yes)
            restartApplication();
    });
    langRow->addWidget(m_languageCombo, 1);
    targetLayout->addLayout(langRow);

    QHBoxLayout *yearRow = new QHBoxLayout;
    yearRow->addWidget(new QLabel(tr("参加高考年份：")));
    m_yearCombo = new QComboBox;
    m_yearCombo->setStyleSheet(QLatin1String(kComboStyle));
    const int currentYear = QDate::currentDate().year();
    for (int y = currentYear; y <= currentYear + 6; ++y)
        m_yearCombo->addItem(QString::number(y));
    m_yearCombo->setToolTip(tr("参加高考的年份：\n高考倒计时按该年 6 月 7 日计算，一模按该年 3 月 17 日计算"));
    m_yearCombo->setCurrentIndex(qBound(0, readAcademyYear() - currentYear, m_yearCombo->count() - 1));
    connect(m_yearCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        writeValue(QStringLiteral("Global/academicyear"), text);
        bool ok = false;
        const int year = text.toInt(&ok);
        if (ok) {
            emit targetYearChanged(year);
            Logger::info(QStringLiteral("高考年份更新为 %1").arg(year));
        }
    });
    yearRow->addWidget(m_yearCombo, 1);
    targetLayout->addLayout(yearRow);
    targetLayout->addWidget(makeHint(tr("高考日期按“该年 6 月 7 日”计算，一模日期按“该年 3 月 17 日”计算。")));

    outer->addWidget(targetGroup);
    outer->addStretch(1);
    return page;
}

// ============================= 小组件设置页 =============================

QWidget *SettingsDialog::createWidgetPage()
{
    // 每个分组都打一条阶段日志：一旦某步崩溃/卡死，日志最后一行就是罪魁祸首。
    // 这正是“切换设置页卡死但日志毫无输出”最需要的排查手段。
    Diagnostics::Step stepPage(QStringLiteral("构建设置页: 小组件设置"));

    QWidget *page = new QWidget;
    Diagnostics::setStage(QStringLiteral("小组件设置/滚动容器"));
    QScrollArea *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    // 统一滚动条样式（替换平台默认的 3D 立体滚动条）
    scroll->setStyleSheet(QString::fromLatin1(kScrollAreaStyle)
                          + QString::fromLatin1(kScrollBarStyle));
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 记住这块滚动区域，wheelEvent 里把滚轮事件转发给它
    // （Qt 默认不会把落在子控件上的滚轮事件转给父级滚动区，
    //   导致“鼠标在设置页上滚轮没反应”）
    m_scrollArea = scroll;

    QWidget *content = new QWidget;
    QVBoxLayout *outer = new QVBoxLayout(content);
    outer->setContentsMargins(24, 20, 24, 20);
    outer->setSpacing(14);

    QVBoxLayout *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(scroll);
    scroll->setWidget(content);
    Diagnostics::setStage(QStringLiteral("小组件设置/读取配置"));
    outer->addWidget(makePageTitle(tr("小组件设置")));

    const WidgetConfig cfg = WidgetConfig::load(m_configPath);
    Diagnostics::setStage(QStringLiteral("小组件设置/样式分组"));

    // ---- 样式选择 ----
    QGroupBox *styleGroup = new QGroupBox(tr("小组件样式"));
    styleGroup->setStyleSheet(QLatin1String(kGroupBoxStyle));
    QVBoxLayout *styleLayout = new QVBoxLayout(styleGroup);
    styleLayout->setSpacing(10);

    QHBoxLayout *styleRow = new QHBoxLayout;
    styleRow->addWidget(new QLabel(tr("样式：")));
    m_styleCombo = new QComboBox;
    m_styleCombo->setStyleSheet(QLatin1String(kComboStyle));
    m_styleCombo->addItem(tr("样式 1 · 圆角卡片（带按钮，可设背景色）"));
    m_styleCombo->addItem(tr("样式 2 · 右下角透明区域（无标题栏、无背景）"));
    m_styleCombo->setCurrentIndex(cfg.style == WidgetConfig::Style2 ? 1 : 0);
    m_styleCombo->setToolTip(tr("选择桌面小组件的外观：\n"
                                "样式 1：圆角卡片，带按钮，可自定义背景色\n"
                                "样式 2：屏幕右下角完全透明的区域，无标题栏、无背景色"));
    styleRow->addWidget(m_styleCombo, 1);
    styleLayout->addLayout(styleRow);

    m_styleHint = makeHint(QString());
    styleLayout->addWidget(m_styleHint);

    // 样式说明文案集中生成，切换与初始化共用，避免两处文案漂移
    auto updateStyleHint = [this](int style) {
        if (!m_styleHint)
            return;
        m_styleHint->setText(style == WidgetConfig::Style2
                                 ? tr("样式 2 在屏幕右下角创建一个完全透明的区域：没有标题栏、没有背景色，"
                                      "只显示“距离一模/高考还有 N 天”，数字为 LCD 七段数码管。"
                                      "鼠标拖动可移动，双击返回主界面。")
                                 : tr("样式 1 是与主界面风格一致的圆角卡片，包含退出/返回按钮，"
                                      "支持自定义背景色、文字颜色与 LCD 数字颜色。"));
    };
    updateStyleHint(m_styleCombo->currentIndex());

    connect(m_styleCombo, &QComboBox::currentIndexChanged, this, [this, updateStyleHint](int index) {
        const int style = (index == 1) ? WidgetConfig::Style2 : WidgetConfig::Style1;
        updateStyleHint(style);
        updateBackgroundAvailability(style);
        WidgetConfig cfgNow = WidgetConfig::load(m_configPath);
        cfgNow.style = style;
        cfgNow.save(m_configPath);
        Logger::info(QStringLiteral("小组件样式切换为样式 %1").arg(style + 1));
        emitWidgetConfig();
    });
    outer->addWidget(styleGroup);
    Diagnostics::setStage(QStringLiteral("小组件设置/显示内容分组"));

    // ---- 显示内容 ----
    QGroupBox *contentGroup = new QGroupBox(tr("显示内容"));
    contentGroup->setStyleSheet(QLatin1String(kGroupBoxStyle));
    QVBoxLayout *contentLayout = new QVBoxLayout(contentGroup);
    contentLayout->setSpacing(10);

    QHBoxLayout *targetRow = new QHBoxLayout;
    targetRow->addWidget(new QLabel(tr("倒计时目标：")));
    m_targetCombo = new QComboBox;
    m_targetCombo->setStyleSheet(QLatin1String(kComboStyle));
    m_targetCombo->addItem(tr("高考（6 月 7 日）"));
    m_targetCombo->addItem(tr("一模（3 月 17 日）"));
    m_targetCombo->setCurrentIndex(cfg.showYiMo ? 1 : 0);
    m_targetCombo->setToolTip(tr("小组件上显示哪一个倒计时：高考（6 月 7 日）或一模（3 月 17 日）"));
    connect(m_targetCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        WidgetConfig cfgNow = WidgetConfig::load(m_configPath);
        cfgNow.showYiMo = (index == 1);
        cfgNow.save(m_configPath);
        Logger::info(QStringLiteral("小组件倒计时目标更新为 %1")
                         .arg(index == 1 ? tr("一模") : tr("高考")));
        emitWidgetConfig();
    });
    targetRow->addWidget(m_targetCombo, 1);
    contentLayout->addLayout(targetRow);

    QHBoxLayout *fontRow = new QHBoxLayout;
    fontRow->addWidget(new QLabel(tr("文字大小：")));
    m_fontSizeSpin = new QSpinBox;
    m_fontSizeSpin->setStyleSheet(QLatin1String(kComboStyle));
    m_fontSizeSpin->setRange(WidgetConfig::minFontSize(), WidgetConfig::maxFontSize());
    m_fontSizeSpin->setSuffix(tr(" px"));
    m_fontSizeSpin->setValue(cfg.fontSize);
    m_fontSizeSpin->setToolTip(tr("小组件中“还有 / 天到高考”等文字的字号（像素），"
                                  "LCD 数字大小会随之等比变化"));
    connect(m_fontSizeSpin, &QSpinBox::valueChanged, this, [this](int value) {
        WidgetConfig cfgNow = WidgetConfig::load(m_configPath);
        cfgNow.fontSize = WidgetConfig::clampFontSize(value);
        cfgNow.save(m_configPath);
        emitWidgetConfig();
    });
    fontRow->addWidget(m_fontSizeSpin, 1);
    contentLayout->addLayout(fontRow);

    // ---- 窗口行为：始终置于最上层（默认开启）----
    m_alwaysOnTopCheck = new QCheckBox(tr("小组件始终显示在最上层"));
    m_alwaysOnTopCheck->setStyleSheet(QLatin1String(kCheckBoxStyle));
    m_alwaysOnTopCheck->setChecked(cfg.alwaysOnTop);
    m_alwaysOnTopCheck->setToolTip(tr("开启后小组件浮在其它窗口之上，不会被浏览器、编辑器挡住；\n"
                                      "关闭后它就像普通窗口一样，可以被其它窗口覆盖。"));
    connect(m_alwaysOnTopCheck, &QCheckBox::toggled, this, [this](bool checked) {
        WidgetConfig cfgNow = WidgetConfig::load(m_configPath);
        cfgNow.alwaysOnTop = checked;
        cfgNow.save(m_configPath);
        Logger::info(QStringLiteral("小组件置顶: %1").arg(checked ? tr("开") : tr("关")));
        emitWidgetConfig();   // 主窗口据此按新的窗口标志重建小组件
    });
    contentLayout->addWidget(m_alwaysOnTopCheck);

    outer->addWidget(contentGroup);
    Diagnostics::setStage(QStringLiteral("小组件设置/颜色分组"));

    // ---- 颜色设置 ----
    QGroupBox *colorGroup = new QGroupBox(tr("颜色设置（两种样式通用）"));
    colorGroup->setStyleSheet(QLatin1String(kGroupBoxStyle));
    QVBoxLayout *colorLayout = new QVBoxLayout(colorGroup);
    colorLayout->setSpacing(10);

    m_textColorBtn = makeColorButton(QStringLiteral("textColorBtn"));
    m_lcdColorBtn  = makeColorButton(QStringLiteral("lcdColorBtn"));
    m_bgColorBtn   = makeColorButton(QStringLiteral("backgroundColorBtn"));

    paintColorButton(m_textColorBtn, tr("文字颜色"), cfg.textColor);
    paintColorButton(m_lcdColorBtn,  tr("数字颜色"), cfg.lcdColor);
    paintColorButton(m_bgColorBtn,   tr("背景颜色"), cfg.bgColor);

    m_bgColorHint = makeHint(QString());

    colorLayout->addWidget(m_textColorBtn);
    colorLayout->addWidget(m_lcdColorBtn);
    colorLayout->addWidget(m_bgColorBtn);
    colorLayout->addWidget(m_bgColorHint);

    QPushButton *resetColors = new QPushButton(tr("恢复默认配色"));
    resetColors->setCursor(Qt::PointingHandCursor);
    resetColors->setToolTip(tr("把文字颜色、数字颜色、背景色与文字大小恢复为出厂默认值"));
    resetColors->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: transparent; color: #3498db; border: 1px solid #bcd6ea;"
        "  padding: 7px 14px; border-radius: 6px; }"
        "QPushButton:hover { background-color: #eaf4fc; }"));
    connect(resetColors, &QPushButton::clicked, this, [this]() {
        WidgetConfig defaults;
        WidgetConfig cfgNow = WidgetConfig::load(m_configPath);
        cfgNow.textColor = defaults.textColor;
        cfgNow.lcdColor  = defaults.lcdColor;
        cfgNow.bgColor   = defaults.bgColor;
        cfgNow.fontSize  = defaults.fontSize;
        cfgNow.save(m_configPath);
        paintColorButton(m_textColorBtn, tr("文字颜色"), cfgNow.textColor);
        paintColorButton(m_lcdColorBtn,  tr("数字颜色"), cfgNow.lcdColor);
        paintColorButton(m_bgColorBtn,   tr("背景颜色"), cfgNow.bgColor);
        if (m_fontSizeSpin)
            m_fontSizeSpin->setValue(cfgNow.fontSize);
        Logger::info(QStringLiteral("小组件配色已恢复默认"));
        emitWidgetConfig();
    });
    colorLayout->addWidget(resetColors, 0, Qt::AlignLeft);

    connect(m_textColorBtn, &QPushButton::clicked, this,
            [this]() { applyColorButtonClick(m_textColorBtn, QStringLiteral("TextColor"), tr("文字颜色")); });
    connect(m_lcdColorBtn, &QPushButton::clicked, this,
            [this]() { applyColorButtonClick(m_lcdColorBtn, QStringLiteral("LcdColor"), tr("数字颜色")); });
    connect(m_bgColorBtn, &QPushButton::clicked, this,
            [this]() { applyColorButtonClick(m_bgColorBtn, QStringLiteral("BackGrandColor"), tr("背景颜色")); });

    outer->addWidget(colorGroup);
    outer->addStretch(1);

    // 初始状态：样式 2 时禁用背景色
    Diagnostics::setStage(QStringLiteral("小组件设置/收尾"));
    updateBackgroundAvailability(cfg.style);
    Diagnostics::setStage(QString());
    return page;
}

void SettingsDialog::updateBackgroundAvailability(int style)
{
    const bool enabled = (style != WidgetConfig::Style2);
    if (m_bgColorBtn) {
        m_bgColorBtn->setEnabled(enabled);
        m_bgColorBtn->setToolTip(enabled
                                     ? tr("点击选择小组件背景色")
                                     : tr("样式 2 是完全透明的区域，没有背景色可设置"));
    }
    if (m_bgColorHint) {
        m_bgColorHint->setText(enabled
                                   ? tr("背景色仅对样式 1（圆角卡片）生效。")
                                   : tr("当前为样式 2：背景色设置已自动禁用。"
                                        "切回样式 1 后会自动恢复（原有的背景色取值不会被清除）。"));
    }
}

void SettingsDialog::applyColorButtonClick(QPushButton *button, const QString &colorKey,
                                           const QString &label)
{
    if (!button)
        return;
    try {
        const QColor initial = button->property("currentColor").value<QColor>();
        const QColor color = QColorDialog::getColor(initial.isValid() ? initial : Qt::red,
                                                    this, tr("请选择%1").arg(label));
        if (!color.isValid())
            return; // 用户取消

        WidgetConfig cfg = WidgetConfig::load(m_configPath);
        cfg.setColorByKey(colorKey, color);
        cfg.save(m_configPath);

        paintColorButton(button, label, color);
        Logger::info(QStringLiteral("小组件%1更新为 %2").arg(label, color.name()));
        emitWidgetConfig();
    } catch (...) {
        Logger::critical(QStringLiteral("更改小组件%1失败").arg(label));
        QMessageBox::warning(this, tr("错误"), tr("无法保存颜色设置，请查看日志。"));
    }
}

// ============================= 文字编辑页 =============================

QWidget *SettingsDialog::createWordEditPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *outer = new QVBoxLayout(page);
    outer->setContentsMargins(24, 20, 24, 20);
    outer->setSpacing(12);

    outer->addWidget(makePageTitle(tr("文字编辑")));
    outer->addWidget(makeHint(tr("这里编辑的语句会随机显示在主界面底部，每行一条。")));

    m_quotesEdit = new QTextEdit;
    m_quotesEdit->setAcceptRichText(false);
    m_quotesEdit->setPlaceholderText(tr("每行写一条鼓励语句，例如：\n乾坤未定，你我皆是黑马。"));
    m_quotesEdit->setToolTip(tr("每行一条鼓励语句，保存后主界面会随机显示其中一条"));
    m_quotesEdit->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "   padding: 8px;"
        "   border: 1px solid #d0d7de;"
        "   border-radius: 8px;"
        "   background-color: white;"
        "   color: #24292f;"
        "   font-size: 14px;"
        "}"
        "QTextEdit:focus { border-color: #3498db; }"));

    // 一次性读取全文（而不是逐行 setText），并统一按 UTF-8 解码
    try {
        QFile file(m_quotesFilePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            m_quotesEdit->setPlainText(stream.readAll());
            file.close();
        } else {
            Logger::warning(QStringLiteral("编辑页无法打开鼓励语句文件: %1 (%2)")
                                .arg(m_quotesFilePath, file.errorString()));
        }
    } catch (...) {
        Logger::critical(QStringLiteral("载入鼓励语句到编辑框失败"));
    }
    outer->addWidget(m_quotesEdit, 1);

    m_noticeCheck = new QCheckBox(tr("启动时在首页推送一条鼓励语句"));
    m_noticeCheck->setStyleSheet(QLatin1String(kCheckBoxStyle));
    m_noticeCheck->setToolTip(tr("勾选后，程序启动时会在首页底部推送一条鼓励语句；\n"
                                 "推送的语句与首页显示的为同一条。"));
    // 与 MainWindow::readNoticeEnabled() 保持完全一致的解析规则，
    // 否则配置写成 1/yes/on 时会出现“复选框没勾但实际在推送”的矛盾状态。
    auto parseBool = [](const QString &raw, bool fallback) {
        const QString v = raw.trimmed().toLower();
        if (v.isEmpty())
            return fallback;
        if (v == QLatin1String("0") || v == QLatin1String("false")
            || v == QLatin1String("no") || v == QLatin1String("off"))
            return false;
        return true;
    };
    const bool noticeOn = parseBool(readValue(QStringLiteral("Global/Notice"),
                                              QStringLiteral("true")), true);
    m_noticeCheck->setChecked(noticeOn);
    connect(m_noticeCheck, &QCheckBox::toggled, this, [this](bool checked) {
        writeValue(QStringLiteral("Global/Notice"), checked);
        Logger::info(QStringLiteral("启动推送鼓励语句: %1").arg(checked ? tr("开") : tr("关")));
        // 立即让主界面生效，不必等下次启动
        emit quotesChanged();
    });
    outer->addWidget(m_noticeCheck);

    QHBoxLayout *actionRow = new QHBoxLayout;
    actionRow->addStretch(1);
    m_saveQuotesBtn = new QPushButton(tr("保存"));
    m_saveQuotesBtn->setCursor(Qt::PointingHandCursor);
    m_saveQuotesBtn->setStyleSheet(QLatin1String(kPrimaryButtonStyle));
    m_saveQuotesBtn->setToolTip(tr("把编辑框中的内容保存到 %1").arg(m_quotesFilePath));
    connect(m_saveQuotesBtn, &QPushButton::clicked, this, [this]() {
        m_saveQuotesBtn->setEnabled(false); // 防抖：避免重复点击并发写入
        const auto restore = qScopeGuard([this] { m_saveQuotesBtn->setEnabled(true); });
        try {
            QFile file(m_quotesFilePath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                Logger::critical(QStringLiteral("保存鼓励语句失败: %1 (%2)")
                                     .arg(m_quotesFilePath, file.errorString()));
                QMessageBox::warning(this, tr("错误"),
                                     tr("文件保存失败：%1").arg(file.errorString()));
                return;
            }
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            stream << m_quotesEdit->toPlainText();
            file.close();

            Logger::info(QStringLiteral("鼓励语句已保存到 %1").arg(m_quotesFilePath));
            emit quotesChanged();
            QMessageBox::information(this, tr("成功"), tr("鼓励语句已保存。"));
        } catch (...) {
            Logger::critical(QStringLiteral("保存鼓励语句时发生异常"));
            QMessageBox::warning(this, tr("错误"), tr("文件保存时发生未知错误。"));
        }
    });
    actionRow->addWidget(m_saveQuotesBtn);
    outer->addLayout(actionRow);

    return page;
}

// ============================= 关于页 =============================

QWidget *SettingsDialog::createAboutPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setAlignment(Qt::AlignCenter);

    QLabel *title = new QLabel(tr("BattleForFuture"));
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("font-size: 26px; font-weight: bold; color: #3498db;"));

    QLabel *subtitle = new QLabel(tr("为高考考生准备的倒计时工具"));
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(QStringLiteral("font-size: 14px; color: #2c3e50;"));

    QLabel *version = new QLabel(tr("版本 %1").arg(QStringLiteral(APP_VERSION)));
    version->setAlignment(Qt::AlignCenter);
    version->setStyleSheet(QStringLiteral("color: #7f8c8d;"));

    QLabel *description = new QLabel(tr("可以激励你为高考奋勇拼搏。\n"
                                        "桌面小组件支持圆角卡片与右下角透明区域两种样式，\n"
                                        "文字与 LCD 数字颜色均可自由定义。"));
    description->setAlignment(Qt::AlignCenter);
    description->setWordWrap(true);
    description->setStyleSheet(QStringLiteral("color: #34495e;"));

    QPushButton *websiteBtn = new QPushButton(tr("访问 GitHub 项目主页"));
    websiteBtn->setCursor(Qt::PointingHandCursor);
    websiteBtn->setStyleSheet(QLatin1String(kPrimaryButtonStyle));
    connect(websiteBtn, &QPushButton::clicked, this, []() {
        const bool ok = QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://github.com/BuBaiMengJ4/BattleForFuture")));
        if (!ok)
            Logger::warning(QStringLiteral("无法打开项目主页链接"));
    });

    layout->addStretch(1);
    layout->addWidget(title);
    layout->addSpacing(6);
    layout->addWidget(subtitle);
    layout->addSpacing(4);
    layout->addWidget(version);
    layout->addSpacing(18);
    layout->addWidget(description);
    layout->addSpacing(22);
    layout->addWidget(websiteBtn, 0, Qt::AlignCenter);
    layout->addStretch(1);
    return page;
}

// ============================= 开机自启动 =============================

bool SettingsDialog::autoStartEnabled() const
{
#ifdef Q_OS_WINDOWS
    QSettings runSettings(QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"),
                          QSettings::NativeFormat);
    return runSettings.contains(QCoreApplication::applicationName());
#elif defined(Q_OS_LINUX)
    return QFile::exists(linuxAutoStartFilePath());
#else
    return false;
#endif
}

void SettingsDialog::applyAutoStart(bool enabled)
{
    const QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    try {
#ifdef Q_OS_WINDOWS
        QSettings runSettings(QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"),
                              QSettings::NativeFormat);
        if (enabled) {
            const QMessageBox::StandardButton reply = QMessageBox::question(
                this, tr("确认"), tr("确定要设置为开机自启动吗？"),
                QMessageBox::Yes | QMessageBox::No);
            if (reply != QMessageBox::Yes) {
                // 用户取消：把复选框恢复，注意避免信号递归
                m_autoStartCheck->blockSignals(true);
                m_autoStartCheck->setChecked(false);
                m_autoStartCheck->blockSignals(false);
                return;
            }
            runSettings.setValue(QCoreApplication::applicationName(), appPath);
            Logger::info(QStringLiteral("已设置开机自启动: %1").arg(appPath));
        } else {
            runSettings.remove(QCoreApplication::applicationName());
            Logger::info(QStringLiteral("已取消开机自启动"));
        }
#elif defined(Q_OS_LINUX)
        const QString desktopPath = linuxAutoStartFilePath();
        if (enabled) {
            QDir().mkpath(QFileInfo(desktopPath).absolutePath());
            QFile file(desktopPath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                Logger::critical(QStringLiteral("无法写入 autostart 文件: %1").arg(desktopPath));
                QMessageBox::warning(this, tr("错误"), tr("无法创建自启动文件，请检查权限。"));
                m_autoStartCheck->blockSignals(true);
                m_autoStartCheck->setChecked(false);
                m_autoStartCheck->blockSignals(false);
                return;
            }
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            stream << "[Desktop Entry]\n"
                   << "Type=Application\n"
                   << "Name=BattleForFuture\n"
                   << "Comment=Gaokao countdown widget for students\n"
                   << "Exec=\"" << appPath << "\"\n"
                   << "Terminal=false\n"
                   << "X-GNOME-Autostart-enabled=true\n";
            file.close();
            Logger::info(QStringLiteral("已创建自启动项: %1").arg(desktopPath));
        } else {
            QFile::remove(desktopPath);
            Logger::info(QStringLiteral("已删除自启动项: %1").arg(desktopPath));
        }
#else
        Logger::warning(QStringLiteral("当前平台不支持开机自启动设置（已请求 %1）")
                            .arg(enabled ? QStringLiteral("启用") : QStringLiteral("禁用")));
        m_autoStartCheck->blockSignals(true);
        m_autoStartCheck->setChecked(false);
        m_autoStartCheck->blockSignals(false);
#endif
    } catch (...) {
        Logger::critical(QStringLiteral("修改开机自启动设置失败"));
        QMessageBox::warning(this, tr("错误"), tr("无法修改开机自启动设置，请检查权限。"));
        m_autoStartCheck->blockSignals(true);
        m_autoStartCheck->setChecked(!enabled);
        m_autoStartCheck->blockSignals(false);
    }
}

// ============================= 滚轮滚动 =============================

void SettingsDialog::wheelEvent(QWheelEvent *event)
{
    // 把滚轮事件交给设置页的滚动区域。
    // Qt 默认不让子控件上的滚轮事件冒泡到父级 QScrollArea，所以用户
    // 在小组件设置页上滚动滚轮时界面毫无反应。这里统一转发。
    const int delta = event->angleDelta().y();
    if (!m_scrollArea || delta == 0 || !m_scrollArea->isVisible()) {
        QDialog::wheelEvent(event);
        return;
    }

    QScrollBar *bar = m_scrollArea->verticalScrollBar();
    if (!bar || bar->maximum() <= bar->minimum()) {
        // 内容不足一屏，没有可滚动的空间
        event->accept();
        return;
    }

    // 按“行”滚动：与系统滚轮步进（通常 15° 一格）保持一致的手感
    const int steps = delta / 120;
    bar->setValue(bar->value() - steps * bar->singleStep() * 3);
    event->accept();
}

// ============================= 重启程序 =============================

void SettingsDialog::restartApplication()
{
    Logger::info(QStringLiteral("准备重启程序"));

    // 先藏起窗口，让用户感觉是“关掉再打开”，而不是两个窗口短暂重叠。
    hide();
    if (QWidget *top = parentWidget())
        top->hide();

    // ============================ 重启为什么需要“监工” ============================
    // 本程序有单实例保护。若在这里直接 QProcess::startDetached 拉起新实例再退出：
    //   1) 此刻本进程仍持有单实例通道，新进程一连接就判定“已有实例在运行”，
    //      发一条 show 请求后**自行退出**；
    //   2) 而 startDetached 只要进程被创建就返回 true，于是本进程以为
    //      “新进程已启动”，紧接着自己也退出 —— 最终程序关掉了却没有重启。
    //
    // 现在改为：启动一个自身副本作为监工（--restart-wait <pid>），
    // 由它等待本进程真正结束（届时通道才被系统回收）后再拉起新实例。
    // 这样不依赖任何脆弱的延时猜测。
    // ============================================================================
    const QStringList forwardArgs = QCoreApplication::arguments().mid(1);
    const bool armed = RestartHelper::launchRestart(forwardArgs);
    if (!armed) {
        QMessageBox::warning(this, tr("错误"),
                             tr("自动重启失败，请手动重新打开程序以应用新的语言设置。"));
        return;
    }

    // 立刻退出，把“等待 + 启动新实例”完全交给监工进程。
    QTimer::singleShot(0, this, []() { QCoreApplication::exit(0); });
}
