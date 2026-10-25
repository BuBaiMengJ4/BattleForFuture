#include "mainwindow.h"

#include "desktopwidget.h"
#include "iconutils.h"
#include "logger.h"
#include "notifier.h"
#include "overlaywidget.h"
#include "settingsdialog.h"
#include "sevensegmentdisplay.h"
#include "ui_mainwindow.h"
#include "widgetstyle.h"

#include <QCloseEvent>
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLinearGradient>
#include <QPainter>
#include <QPalette>
#include <QRandomGenerator>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>
#include <QTimer>

#include <cmath>

namespace {

/// 返回可写的资源目录（优先程序目录；不可写时回退到用户配置目录）
QString resolveAssetsDir(const QString &appDir)
{
    QDir dir(appDir);
    if (dir.exists(QStringLiteral("Assets")) || dir.mkpath(QStringLiteral("Assets")))
        return dir.absolutePath();

    // 程序目录只读（如 Program Files）时使用用户配置目录
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir cdir(configDir);
    cdir.mkpath(QStringLiteral("Assets"));
    return configDir;
}

/// 查找鼓励语句文件：Assets/quotes.txt -> 程序目录 quotes.txt -> 源码目录 quotes.txt
QString resolveQuotesFile(const QString &appDir)
{
    QStringList candidates;
    candidates << QDir(resolveAssetsDir(appDir)).filePath(QStringLiteral("quotes.txt"));
    candidates << QDir(appDir).filePath(QStringLiteral("quotes.txt"));
#ifdef QUOTE_FALLBACK_PATH
    candidates << QStringLiteral(QUOTE_FALLBACK_PATH);
#endif
    for (const QString &candidate : candidates) {
        if (QFile::exists(candidate))
            return candidate;
    }
    Logger::warning(QStringLiteral("未找到鼓励语句文件，已尝试: %1")
                        .arg(candidates.join(QStringLiteral(", "))));
    return candidates.first(); // 返回首选路径（通常可写），便于编辑页新建文件
}

/// 安全读取 ini 配置（任何异常都不向外传播）
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

} // namespace

// ============================= 构造 / 析构 =============================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    appDir = QCoreApplication::applicationDirPath();
    configPath = QDir(appDir).filePath(QStringLiteral("config.ini"));
    quotesFilePath = resolveQuotesFile(appDir);

    applyHomeBackground();

    // 数码管参数必须在代码里显式设置：
    // .ui 里给自定义控件写的属性是**动态属性**（uic 生成 setProperty），
    // 不会调用 C++ 的 setMinimumDigitCount()/setDigitColor()，
    // 于是控件内部仍按默认值工作 —— 最小位数变成 0、颜色也不会生效。
    // 之前控件被布局拉宽所以看不出问题；改成“宽度跟随数字”后，
    // 就会按一位数计算宽度，三位数会被压缩，因此这里必须显式设置。
    for (SevenSegmentDisplay *lcd : { ui->CountdownGK, ui->CountdownYM }) {
        if (!lcd)
            continue;
        lcd->setMinimumDigitCount(3);          // 预留 3 位，数字变化时不抖动
        lcd->setDigitColor(QColor(255, 60, 60)); // 与界面设计一致的红色
    }

    // 按钮图标规整：Qt 按“图片几何中心”居中图标，而本项目两张图的内容本身
    // 就偏在画布一侧（实测 Settings.png 偏上 12.4px、Widget.png 偏左 7.9px），
    // 所以看起来图标偏向一角。这里裁剪到内容包围盒后重新居中，
    // 与按钮尺寸、样式表都无关，任何缩放下都真正居中。
    if (QPushButton *widgetBtn = ui->pushButton) {
        const QIcon centered = IconUtils::centeredIcon(QStringLiteral(":/icon/Widget.png"), 128);
        if (!centered.isNull())
            widgetBtn->setIcon(centered);
    }
    if (QPushButton *settingsBtn = ui->Settings) {
        const QIcon centered = IconUtils::centeredIcon(QStringLiteral(":/icon/Settings.png"), 128);
        if (!centered.isNull())
            settingsBtn->setIcon(centered);
    }

    // 目标日期：高考为该年 6 月 7 日；一模固定为该年 3 月 17 日
    refreshTargetDates();
    refreshCountdowns();

    // 随机鼓励语
    // 字体沿用原来的楷体；颜色必须显式指定：
    // ScrollingLabel 用 QPainter 自绘文字，默认取调色板 WindowText（Windows 上是纯黑），
    // 而主界面背景是深色星空图 —— 黑色文字基本看不见，看起来就像“没有推送内容”。
    QFont quoteFont(QStringLiteral("楷体"), 20);
    quoteFont.setBold(true);
    ui->label_5->setFont(quoteFont);
    // 浅蓝白：既贴合原设计的蓝色标语，也能压在深色背景上清晰可读
    ui->label_5->setTextColor(QColor(0xE8, 0xF2, 0xFF));
    loadQuotes();
    refreshQuoteLabel();

    // 跨零点自动刷新：倒计时是“按天”变化的，用分钟级定时器即可，
    // 既保证长时间运行不会显示过期数字，又几乎没有开销。
    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(60 * 1000);
    connect(m_countdownTimer, &QTimer::timeout, this, &MainWindow::refreshCountdowns);
    m_countdownTimer->start();

    // ===== 托盘图标（推送通知时才会真正创建）的回调 =====
    connect(&Notifier::instance(), &Notifier::showRequested, this, &MainWindow::bringToFront);
    connect(&Notifier::instance(), &Notifier::quitRequested, this, []() {
        QCoreApplication::quit();
    });

    // ===== 启动推送 =====
    // 说明：首页的鼓励语句与这个开关无关（首页始终显示一条），
    // 本开关只决定“启动时是否再通过系统通知推送一次”。
    // 延迟一会儿再发，等窗口和托盘都就绪，避免通知被后续初始化打断。
    if (readNoticeEnabled()) {
        QTimer::singleShot(900, this, [this]() { pushStartupNotification(); });
    } else {
        Logger::info(QStringLiteral("启动推送鼓励语句已关闭，本次不发送系统通知"));
    }
}

MainWindow::~MainWindow()
{
    delete ui;
    ui = nullptr;
}

void MainWindow::applyHomeBackground()
{
    // 主界面背景由 mainwindow.ui 里的 QSS 负责：
    //     #centralwidget { border-image: url(:/icon/home-background.png); }
    //
    // 这里**不要**再给 centralwidget 设置样式表或调色板背景：
    // 子控件上的 QSS 会完全接管该控件的背景绘制，任何 `background: transparent`
    // 之类的覆盖都会把 border-image 顶掉，导致背景时有时无（Linux 上尤其明显）。
    //
    // 只做一件事：确认资源真的加载成功，失败时留下明确日志
    // （旧版本曾引用已删除的 BackGrand.png，运行时只报 pixmap 创建失败，
    //   界面上表现为“背景无缘无故没了”，很难定位）。
    const QPixmap probe(QStringLiteral(":/icon/home-background.png"));
    if (probe.isNull())
        Logger::warning(QStringLiteral("主界面背景图 :/icon/home-background.png 加载失败，"
                                       "界面将显示为纯色背景"));
    else
        Logger::debug(QStringLiteral("主界面背景图已加载: %1x%2")
                          .arg(probe.width())
                          .arg(probe.height()));
}

// ============================= 工具函数 =============================

int MainWindow::calculateRemainingDays(const QDate &targetDate)
{
    if (!targetDate.isValid())
        return 0;
    const int days = QDate::currentDate().daysTo(targetDate);
    return days > 0 ? days : 0; // 若目标日期已过，返回 0
}

int MainWindow::readAcademyYear() const
{
    const int currentYear = QDate::currentDate().year();
    int year = currentYear;
    withSettings(configPath, [&](QSettings &s) {
        const QString value = s.value(QStringLiteral("Global/academicyear")).toString();
        bool ok = false;
        const int parsed = value.toInt(&ok);
        if (ok && parsed >= currentYear && parsed <= currentYear + 6)
            year = parsed;
    });
    return year;
}

void MainWindow::refreshTargetDates()
{
    const int year = readAcademyYear();
    gaokaoDate = QDate(year, 6, 7);   // 高考通常 6 月 7 日开始
    yimoDate   = QDate(year, 3, 17);  // 一模日期（沿用原设定）
    if (!gaokaoDate.isValid() || !yimoDate.isValid())
        Logger::warning(QStringLiteral("倒计时目标日期非法: year=%1").arg(year));
}

void MainWindow::refreshCountdowns()
{
    ui->CountdownGK->display(calculateRemainingDays(gaokaoDate));
    ui->CountdownYM->display(calculateRemainingDays(yimoDate));
}

bool MainWindow::loadQuotes()
{
    quotes.clear();
    try {
        QFile file(quotesFilePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            Logger::warning(QStringLiteral("无法打开鼓励语句文件: %1 (%2)")
                                .arg(quotesFilePath, file.errorString()));
            return false;
        }
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        while (!stream.atEnd()) {
            const QString line = stream.readLine().trimmed();
            if (!line.isEmpty())
                quotes << line;
        }
        file.close();
        Logger::info(QStringLiteral("已加载鼓励语句 %1 条").arg(quotes.size()));
    } catch (...) {
        Logger::critical(QStringLiteral("读取鼓励语句文件发生异常: %1").arg(quotesFilePath));
        quotes.clear();
        return false;
    }
    return !quotes.isEmpty();
}

QString MainWindow::randomQuote() const
{
    if (quotes.isEmpty())
        return QString();
    const int index = int(QRandomGenerator::global()->bounded(quotes.size()));
    return quotes.at(index);
}

bool MainWindow::readNoticeEnabled() const
{
    // 兼容多种写法：旧版本可能写成 1/0、true/false、yes/no、on/off。
    // 只看前缀，避免 "1 " 之类的空白导致误判。
    bool enabled = true;   // 默认开启（与设置界面的默认勾选一致）
    withSettings(configPath, [&](QSettings &s) {
        const QString raw = s.value(QStringLiteral("Global/Notice"), true).toString()
                                .trimmed().toLower();
        if (raw.isEmpty())
            return;
        enabled = !(raw == QLatin1String("0") || raw == QLatin1String("false")
                    || raw == QLatin1String("no") || raw == QLatin1String("off"));
    });
    return enabled;
}

void MainWindow::refreshQuoteLabel()
{
    // 首页始终显示一条鼓励语句，与“启动时是否推送系统通知”无关：
    // 那个开关只控制通知，不该让首页变成空白。
    // 只有在没有“当前语句”时才随机抽取，保证首页与通知推送的是同一句。
    if (m_pushedQuote.isEmpty())
        m_pushedQuote = randomQuote();

    if (m_pushedQuote.isEmpty()) {
        ui->label_5->setText(tr("请在设置中添加鼓励语"));
        Logger::warning(QStringLiteral("鼓励语句为空（请检查 quotes.txt）"));
        return;
    }

    ui->label_5->setText(m_pushedQuote);
    Logger::debug(QStringLiteral("首页显示语句: %1").arg(m_pushedQuote));
}

void MainWindow::pushStartupNotification()
{
    if (m_pushedQuote.isEmpty()) {
        Logger::warning(QStringLiteral("没有可推送的鼓励语句，跳过启动通知"));
        return;
    }

    // 标题带上倒计时目标与天数，让通知本身也有信息量；
    // 正文与首页显示的语句完全相同。
    bool showYiMo = false;
    withSettings(configPath, [&](QSettings &s) {
        showYiMo = s.value(QStringLiteral("Widget/showcontent"), 0).toInt() == 1;
    });
    const QDate target = (showYiMo && yimoDate.isValid()) ? yimoDate : gaokaoDate;
    const int days = calculateRemainingDays(target);

    const QString title = showYiMo
                              ? tr("距离一模还有 %1 天").arg(days)
                              : tr("距离高考还有 %1 天").arg(days);

    const bool ok = Notifier::instance().notify(title, m_pushedQuote);
    if (ok)
        Logger::info(QStringLiteral("启动推送成功: %1 / %2").arg(title, m_pushedQuote));
    else
        Logger::warning(QStringLiteral("启动推送失败：当前环境不支持系统通知"));
}

// ============================= 桌面小组件 =============================

bool MainWindow::widgetModeActive() const
{
    return m_desktopWidget && m_desktopWidget->isVisible();
}

void MainWindow::destroyDesktopWidget()
{
    if (!m_desktopWidget)
        return;
    QWidget *widget = m_desktopWidget;
    m_desktopWidget = nullptr; // 先置空，避免 destroyed 回调再次进入
    widget->hide();
    widget->close();
    widget->deleteLater();
}

void MainWindow::createDesktopWidget()
{
    const WidgetConfig cfg = WidgetConfig::load(configPath);

    // 销毁旧实例（样式或配色可能已被用户改过）
    destroyDesktopWidget();

    if (cfg.style == WidgetConfig::Style2) {
        // 样式 2：屏幕右下角、无标题栏、完全透明的倒计时区域
        auto *overlay = new OverlayWidget(cfg.textColor, cfg.lcdColor, cfg.showYiMo,
                                          gaokaoDate, yimoDate, cfg.fontSize,
                                          cfg.alwaysOnTop, nullptr);
        connect(overlay, &OverlayWidget::returnToMainRequested, this, &MainWindow::returnToMainWindow);
        m_desktopWidget = overlay;
    } else {
        // 样式 1：圆角卡片小组件
        // 传入 configPath 以启用“位置记忆”：拖动结束后写回配置，
        // 下次创建时恢复上次位置，没有记录则回到屏幕右上角。
        auto *card = new DesktopWidget(cfg.bgColor, cfg.textColor, cfg.lcdColor, cfg.showYiMo,
                                       gaokaoDate, yimoDate, cfg.fontSize,
                                       cfg.alwaysOnTop, configPath, nullptr);
        connect(card, &DesktopWidget::returnToMainRequested, this, &MainWindow::returnToMainWindow);
        m_desktopWidget = card;
    }
}

void MainWindow::enterWidgetMode()
{
    try {
        createDesktopWidget();
        if (!m_desktopWidget)
            return;
        m_desktopWidget->show();
        m_desktopWidget->raise();
        hide();
        const WidgetConfig cfg = WidgetConfig::load(configPath);
        Logger::info(QStringLiteral("已切换到桌面小组件模式（样式 %1）").arg(cfg.style + 1));
    } catch (const std::exception &e) {
        Logger::critical(QStringLiteral("创建桌面小组件时发生异常: %1").arg(QString::fromUtf8(e.what())));
        destroyDesktopWidget();
        show();
    } catch (...) {
        Logger::critical(QStringLiteral("创建桌面小组件时发生未知异常"));
        destroyDesktopWidget();
        show();
    }
}

void MainWindow::returnToMainWindow()
{
    destroyDesktopWidget();
    show();
    refreshCountdowns();
    bringToFront();
}

void MainWindow::bringToFront()
{
    // 关键：被其它实例唤醒、或从小组件返回时，都要真正把窗口抬到前台。
    // 仅 show() 在部分 Windows 场景下仍会被其它启动项窗口压在下面。
    QWidget *target = widgetModeActive() ? m_desktopWidget : static_cast<QWidget *>(this);
    if (!target)
        return;
    if (target->isMinimized())
        target->setWindowState(target->windowState() & ~Qt::WindowMinimized);
    target->show();
    target->raise();
    target->activateWindow();
    Logger::debug(QStringLiteral("已把窗口带到前台（小组件模式=%1）")
                      .arg(widgetModeActive() ? QStringLiteral("是") : QStringLiteral("否")));
}

// ============================= 槽函数 =============================

void MainWindow::on_Settings_clicked()
{
    if (!m_settings) {
        m_settings = new SettingsDialog(configPath, quotesFilePath, this);

        // 小组件配置变化：重建正在运行的小组件，保证“所见即所得”
        connect(m_settings, &SettingsDialog::widgetConfigChanged, this, [this](const WidgetConfig &) {
            if (widgetModeActive()) {
                createDesktopWidget();
                if (m_desktopWidget) {
                    m_desktopWidget->show();
                    m_desktopWidget->raise();
                }
            }
        });
        // 高考年份变化：刷新目标日期、首页倒计时与小组件
        connect(m_settings, &SettingsDialog::targetYearChanged, this, [this](int) {
            refreshTargetDates();
            refreshCountdowns();
            if (widgetModeActive()) {
                createDesktopWidget();
                if (m_desktopWidget) {
                    m_desktopWidget->show();
                    m_desktopWidget->raise();
                }
            }
        });
        // 鼓励语句保存：重新载入并刷新首页显示。
        // 这里清空“当前语句”是有意为之：旧语句可能已被用户删掉，
        // 清空后会重新随机抽取一条，保证首页显示的语句始终存在于文件中。
        connect(m_settings, &SettingsDialog::quotesChanged, this, [this]() {
            loadQuotes();
            m_pushedQuote.clear();
            refreshQuoteLabel();
        });
    }

    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
}

void MainWindow::on_pushButton_clicked()
{
    enterWidgetMode();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 关闭主界面即代表用户想退出程序：此时把小组件一并关掉，
    // 避免留下“没有任何可见窗口”的后台僵尸实例
    // （这正是 Windows 上再次启动看不到窗口的根源之一）。
    if (m_desktopWidget) {
        Logger::info(QStringLiteral("主界面关闭，同时结束桌面小组件"));
        destroyDesktopWidget();
    }
    QMainWindow::closeEvent(event);
}
