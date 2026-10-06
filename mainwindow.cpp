#include "mainwindow.h"
#include "desktopwidget.h"
#include "logger.h"
#include "ui_mainwindow.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QGroupBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScreen>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTextEdit>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>
#include <QScopeGuard>

namespace {

// ---------------- 配置读写小工具：统一路径分隔符、统一错误日志 ----------------

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

/// 查找鼓励语句文件：Assets/quotes.txt -> 资源目录 quotes.txt -> 源码目录 quotes.txt
QString resolveQuotesFile(const QString &appDir)
{
    QStringList candidates;
    candidates << QDir(resolveAssetsDir(appDir)).filePath(QStringLiteral("quotes.txt"));
    candidates << QDir(appDir).filePath(QStringLiteral("quotes.txt"));
#ifdef QUOTE_FALLBACK_PATH
    candidates << QStringLiteral(QUOTE_FALLBACK_PATH);
#endif
    for (const QString &c : candidates) {
        if (QFile::exists(c))
            return c;
    }
    Logger::warning(QStringLiteral("未找到鼓励语句文件，已尝试: %1").arg(candidates.join(QStringLiteral(", "))));
    return candidates.first(); // 返回首选路径（通常可写），便于“文字编辑”页保存新建文件
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

/// 统一的 ComboBox / GroupBox 样式，避免大段重复字符串
const char *const kComboStyle =
    "QComboBox {"
    "   padding: 5px;"
    "   border: 1px solid #dcdcdc;"
    "   border-radius: 3px;"
    "   background-color: white;"
    "}"
    "QComboBox::drop-down { border: none; }"
    "QComboBox QAbstractItemView {"
    "   padding: 5px;"
    "   border: 1px solid rgb(31, 156, 220);"
    "   border-radius: 3px;"
    "   color: black;"
    "   background-color: white;"
    "   selection-background-color: rgb(90, 90, 90);"
    "}";

const char *const kGroupBoxStyle =
    "QGroupBox {"
    "   font-weight: bold;"
    "   border: 2px solid #cccccc;"
    "   border-radius: 8px;"
    "   margin-top: 1ex;"
    "   padding-top: 10px;"
    "}"
    "QGroupBox::title {"
    "   subcontrol-origin: margin;"
    "   left: 10px;"
    "   padding: 0 5px 0 5px;"
    "}";

} // namespace

// ============================= 构造 / 析构 =============================

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    appDir = QCoreApplication::applicationDirPath();
    configPath = QDir(appDir).filePath(QStringLiteral("config.ini"));

    // 鼓励语句文件：<assets>/quotes.txt，找不到时尝试程序目录/源码目录
    quotesFilePath = resolveQuotesFile(appDir);

    // 目标日期：高考为配置的“参加高考年份”的 6 月 7 日；一模固定为该年 3 月 17 日
    refreshTargetDates();
    updateCountdowns();

    // 随机鼓励语
    QFont quoteFont(QStringLiteral("楷体"), 20);
    ui->label_5->setFont(quoteFont);
    if (loadQuotes())
        ui->label_5->setText(randomQuote());
    else
        ui->label_5->setText(tr("请在设置中添加鼓励语"));

    // 读取设置，决定启动形态（主窗口 or 桌面小组件）
    bool startAsWidget = false;
    if (QFile::exists(configPath)) {
        withSettings(configPath, [&](QSettings &s) {
            startAsWidget = !s.value(QStringLiteral("Start/StartAsWidget"), true).toBool();
        });
    }

    if (startAsWidget) {
        Logger::info(QStringLiteral("根据配置以桌面小组模式启动"));
        on_pushButton_clicked();
    } else {
        show();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
    ui = nullptr;
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
    int year = QDate::currentDate().year();
    withSettings(configPath, [&](QSettings &s) {
        const QString v = s.value(QStringLiteral("Global/academicyear")).toString();
        bool ok = false;
        const int parsed = v.toInt(&ok);
        if (ok && parsed >= year && parsed <= year + 6)
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

void MainWindow::updateCountdowns()
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
    const int idx = QRandomGenerator::global()->bounded(quotes.size());
    return quotes.at(idx);
}

// ============================= 桌面小组件 =============================

void MainWindow::on_pushButton_clicked()
{
    try {
        QColor bgColor(255, 0, 0);
        bool showYiMo = false;
        withSettings(configPath, [&](QSettings &s) {
            s.beginGroup(QStringLiteral("Widget"));
            bgColor = s.value(QStringLiteral("BackGrandColor"), bgColor).value<QColor>();
            showYiMo = s.value(QStringLiteral("showcontent"), 0).toInt() == 1;
            s.endGroup();
        });

        if (!m_desktopWidget) {
            m_desktopWidget = new DesktopWidget(bgColor, showYiMo, gaokaoDate, yimoDate, this);
            connect(m_desktopWidget, &DesktopWidget::returnToMainRequested, this, [this]() {
                if (m_desktopWidget)
                    m_desktopWidget->hide();
                show();
                updateCountdowns(); // 返回主界面时刷新倒计时
            });
            // 关闭小组件（例如退出）时清空指针
            m_desktopWidget->setAttribute(Qt::WA_DeleteOnClose);
            connect(m_desktopWidget, &QObject::destroyed, this,
                    [this]() { m_desktopWidget = nullptr; });
        } else {
            // 复用已有实例前，按最新配置刷新外观与内容
            m_desktopWidget->close();
            m_desktopWidget = nullptr;
            on_pushButton_clicked();
            return;
        }

        m_desktopWidget->show();
        hide();
    } catch (...) {
        Logger::critical(QStringLiteral("创建桌面小组件时发生异常"));
        QMessageBox::warning(this, tr("错误"), tr("无法创建桌面小组件，请查看日志。"));
        show();
    }
}

// ============================= 设置对话框 =============================

void MainWindow::on_Settings_clicked()
{
    if (!settingDialog) {
        settingDialog = new QDialog(this);
        settingDialog->setWindowTitle(tr("应用设置"));
        settingDialog->resize(600, 400);
        settingDialog->setMinimumSize(600, 400);
        buildSettingDialog();
    }
    settingDialog->exec();
}

void MainWindow::buildSettingDialog()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(settingDialog);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 左侧导航区
    QFrame *leftFrame = new QFrame(settingDialog);
    leftFrame->setStyleSheet(QStringLiteral("background-color: #2c3e50; color: white;"));
    leftFrame->setMinimumWidth(200);
    leftFrame->setMaximumWidth(200);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftFrame);
    leftLayout->setContentsMargins(0, 10, 0, 0);

    // 分隔线
    QFrame *divider = new QFrame(settingDialog);
    divider->setFrameShape(QFrame::VLine);
    divider->setFrameShadow(QFrame::Sunken);
    divider->setStyleSheet(QStringLiteral("background-color: #d1d5db;"));

    // 右侧内容区
    QVBoxLayout *rightLayout = new QVBoxLayout;

    mainLayout->addWidget(leftFrame);
    mainLayout->addWidget(divider);
    mainLayout->addLayout(rightLayout, 1);

    categoryList = createCategoryList();
    leftLayout->addWidget(categoryList);

    pagesWidget = new QStackedWidget(settingDialog);
    pagesWidget->addWidget(createGeneralPage());
    pagesWidget->addWidget(createAppearancePage());
    pagesWidget->addWidget(createWordEditPage());
    pagesWidget->addWidget(createAboutPage());
    rightLayout->addWidget(pagesWidget);

    pagesWidget->setCurrentIndex(0);

    // 页面切换动画（滑入滑出）。动画对象由 this 持有，避免泄漏。
    connect(categoryList, &QListWidget::currentRowChanged, this, [this](int index) {
        if (index < 0 || index >= pagesWidget->count())
            return;
        QWidget *currentPage = pagesWidget->currentWidget();
        QWidget *nextPage = pagesWidget->widget(index);
        if (!currentPage || !nextPage || currentPage == nextPage)
            return;

        const int direction = (index > pagesWidget->currentIndex()) ? 1 : -1;
        const int w = pagesWidget->width();
        nextPage->move(direction * w, 0);
        nextPage->show();

        QPropertyAnimation *currentAnim = new QPropertyAnimation(currentPage, "pos", this);
        QPropertyAnimation *nextAnim = new QPropertyAnimation(nextPage, "pos", this);
        for (QPropertyAnimation *anim : {currentAnim, nextAnim}) {
            anim->setDuration(300);
            anim->setEasingCurve(QEasingCurve::OutQuad);
        }
        currentAnim->setStartValue(currentPage->pos());
        currentAnim->setEndValue(QPoint(-direction * w, 0));
        nextAnim->setStartValue(nextPage->pos());
        nextAnim->setEndValue(QPoint(0, 0));

        connect(nextAnim, &QPropertyAnimation::finished, this, [this, currentPage, index]() {
            pagesWidget->setCurrentIndex(index);
            currentPage->move(0, 0); // 重置位置，防止状态污染
        });
        // 动画结束后自毁，防止反复切换累积内存
        connect(currentAnim, &QPropertyAnimation::finished, currentAnim, &QObject::deleteLater);
        connect(nextAnim, &QPropertyAnimation::finished, nextAnim, &QObject::deleteLater);

        currentAnim->start(QAbstractAnimation::DeleteWhenStopped);
        nextAnim->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

QListWidget *MainWindow::createCategoryList()
{
    QListWidget *list = new QListWidget;
    list->setStyleSheet(
        "QListWidget {"
        "   background-color: #2c3e50;"
        "   border: none;"
        "   font-size: 14px;"
        "   outline: none;" // 去掉虚线焦点轮廓
        "}"
        "QListWidget::item {"
        "   padding: 12px 15px;"
        "   border-bottom: 1px solid #34495e;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: #3498db;"
        "   color: white;"
        "}"
        "QListWidget::item:hover:!selected {"
        "   background-color: #34495e;"
        "}");

    const QStringList categories = {tr("常规设置"), tr("外观设置"), tr("文字编辑"), tr("关于")};
    for (const QString &category : categories) {
        QListWidgetItem *item = new QListWidgetItem(category, list);
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    list->setCurrentRow(0);
    return list;
}

// ---------------- 常规设置 ----------------

QWidget *MainWindow::createGeneralPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignTop);

    QLabel *title = new QLabel(tr("常规设置"));
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold; color: #2c3e50; margin-bottom: 15px;"));

    // 启动选项
    QGroupBox *startupGroup = new QGroupBox(tr("启动选项"));
    startupGroup->setStyleSheet(QLatin1String(kGroupBoxStyle));
    QVBoxLayout *startupLayout = new QVBoxLayout(startupGroup);

    QCheckBox *autoStart = new QCheckBox(tr("开机自动启动"));
    QCheckBox *smallAsWidget = new QCheckBox(tr("启动时自动打开到小部件"));
    startupLayout->addWidget(autoStart);
    startupLayout->addWidget(smallAsWidget);

    // 读取初始状态
    bool startAsWidget = true;
    withSettings(configPath, [&](QSettings &s) {
        startAsWidget = s.value(QStringLiteral("Start/StartAsWidget"), true).toBool();
    });
    smallAsWidget->setChecked(!startAsWidget);

#ifdef Q_OS_WINDOWS
    {
        QSettings runSettings(QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"),
                              QSettings::NativeFormat);
        autoStart->setChecked(runSettings.contains(QCoreApplication::applicationName()));
    }
#endif

    // 显示语言
    QHBoxLayout *languageLayout = new QHBoxLayout;
    QLabel *languageLabel = new QLabel(tr("显示语言为："));
    QComboBox *languageCombo = new QComboBox;
    languageCombo->addItem(QStringLiteral("简体中文"));
    languageCombo->addItem(QStringLiteral("繁體中文"));
    languageCombo->setStyleSheet(QLatin1String(kComboStyle));
    int savedLang = 0;
    withSettings(configPath, [&](QSettings &s) {
        savedLang = s.value(QStringLiteral("Global/language"), 0).toInt();
    });
    languageCombo->setCurrentIndex(qBound(0, savedLang, languageCombo->count() - 1));
    languageLayout->addWidget(languageLabel);
    languageLayout->addWidget(languageCombo);

    // 高考年份
    QHBoxLayout *yearLayout = new QHBoxLayout;
    QLabel *yearLabel = new QLabel(tr("请选择你的参加高考年份："));
    QComboBox *yearCombo = new QComboBox;
    yearCombo->setStyleSheet(QLatin1String(kComboStyle));
    const int currentYear = QDate::currentDate().year();
    for (int y = currentYear; y <= currentYear + 6; ++y)
        yearCombo->addItem(QString::number(y));
    const int savedYear = readAcademyYear();
    const int yearIdx = qBound(0, savedYear - currentYear, yearCombo->count() - 1);
    yearCombo->setCurrentIndex(yearIdx);
    yearLayout->addWidget(yearLabel);
    yearLayout->addWidget(yearCombo);

    // --- 信号槽（全部使用 this 作为 context，随窗口销毁自动断开）---

    // 开机自启动（仅 Windows）
    connect(autoStart, &QCheckBox::stateChanged, this, [=](int state) {
        if (m_isSettingAutoStart)
            return; // 防止递归
#ifdef Q_OS_WINDOWS
        try {
            QSettings runSettings(
                QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"),
                QSettings::NativeFormat);
            const QString appName = QCoreApplication::applicationName();
            const QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

            if (state == Qt::Checked) {
                const QMessageBox::StandardButton reply = QMessageBox::question(
                    this, tr("确认"), tr("确定要设置为开机自启动吗？"),
                    QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    runSettings.setValue(appName, appPath);
                    Logger::info(QStringLiteral("已设置开机自启动: %1").arg(appPath));
                } else {
                    m_isSettingAutoStart = true;
                    QMetaObject::invokeMethod(this, [=] {
                        autoStart->setChecked(false);
                        m_isSettingAutoStart = false;
                    }, Qt::QueuedConnection);
                    return;
                }
            } else {
                runSettings.remove(appName);
                Logger::info(QStringLiteral("已取消开机自启动"));
            }
        } catch (...) {
            Logger::critical(QStringLiteral("修改开机自启动注册表项失败"));
            QMessageBox::warning(this, tr("错误"), tr("无法修改开机自启动设置，请检查权限。"));
        }
#else
        Q_UNUSED(state);
        Logger::warning(QStringLiteral("当前平台不支持开机自启动设置"));
#endif
    });

    // 启动时是否为小部件（勾选 = 不进入小部件模式，保持原语义）
    connect(smallAsWidget, &QCheckBox::stateChanged, this, [=](int state) {
        withSettings(configPath, [&](QSettings &s) {
            s.setValue(QStringLiteral("Start/StartAsWidget"), state != Qt::Checked);
            s.sync();
        });
        Logger::info(QStringLiteral("启动模式更新: %1")
                         .arg(state == Qt::Checked ? QStringLiteral("主窗口") : QStringLiteral("小部件")));
    });

    // 语言配置
    connect(languageCombo, &QComboBox::currentIndexChanged, this, [=](int index) {
        withSettings(configPath, [&](QSettings &s) {
            s.setValue(QStringLiteral("Global/language"), index);
            s.sync();
        });
        Logger::info(QStringLiteral("语言设置更新为索引 %1").arg(index));
    });

    // 高考年份配置
    connect(yearCombo, &QComboBox::currentTextChanged, this, [=](const QString &text) {
        withSettings(configPath, [&](QSettings &s) {
            s.setValue(QStringLiteral("Global/academicyear"), text);
            s.sync();
        });
        refreshTargetDates();
        updateCountdowns();
        Logger::info(QStringLiteral("高考年份更新为 %1").arg(text));
    });

    layout->addWidget(title);
    layout->addLayout(languageLayout);
    layout->addLayout(yearLayout);
    layout->addWidget(startupGroup);
    layout->addSpacing(3);
    return page;
}

// ---------------- 外观设置 ----------------

QWidget *MainWindow::createAppearancePage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignTop);

    QLabel *title = new QLabel(tr("外观设置"));
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold; color: #2c3e50; margin-bottom: 15px;"));
    layout->addWidget(title);

    QGroupBox *widgetSet = new QGroupBox(tr("小组件设置"));
    widgetSet->setStyleSheet(QLatin1String(kGroupBoxStyle));
    QVBoxLayout *widgetSetLayout = new QVBoxLayout(widgetSet);
    QHBoxLayout *contentSetLayout = new QHBoxLayout;
    QHBoxLayout *backgroundSetLayout = new QHBoxLayout;

    // 显示内容选择
    QLabel *showContentLabel = new QLabel(tr("请选择显示内容："));
    QComboBox *showChoice = new QComboBox;
    showChoice->addItem(tr("高考"));
    showChoice->addItem(tr("一模"));
    showChoice->setStyleSheet(QLatin1String(kComboStyle));

    int savedContent = 0;
    withSettings(configPath, [&](QSettings &s) {
        savedContent = s.value(QStringLiteral("Widget/showcontent"), 0).toInt();
    });
    showChoice->setCurrentIndex(qBound(0, savedContent, 1));

    connect(showChoice, &QComboBox::currentIndexChanged, this, [=](int index) {
        withSettings(configPath, [&](QSettings &s) {
            s.setValue(QStringLiteral("Widget/showcontent"), index);
            s.sync();
        });
        Logger::info(QStringLiteral("小组件显示内容更新: %1").arg(index == 1 ? tr("一模") : tr("高考")));
    });

    // 背景色设置
    QLabel *bgColorLabel = new QLabel(tr("设置小组件背景色："));
    QPushButton *bgColorBtn = new QPushButton(tr("选择颜色"));
    bgColorBtn->setObjectName(QStringLiteral("backgroundColorBtn"));

    QColor initialColor(255, 0, 0);
    withSettings(configPath, [&](QSettings &s) {
        initialColor = s.value(QStringLiteral("Widget/BackGrandColor"), initialColor).value<QColor>();
    });
    updateButtonColor(bgColorBtn, initialColor);

    connect(bgColorBtn, &QPushButton::clicked, this, [this, bgColorBtn]() {
        try {
            const QColor color = QColorDialog::getColor(
                bgColorBtn->property("currentColor").value<QColor>(),
                this, tr("请选择颜色"));
            if (!color.isValid())
                return; // 用户取消
            updateButtonColor(bgColorBtn, color);
            withSettings(configPath, [&](QSettings &s) {
                s.setValue(QStringLiteral("Widget/BackGrandColor"), color);
                s.sync();
            });
            Logger::info(QStringLiteral("小组件背景色更新为 %1").arg(color.name()));
        } catch (...) {
            Logger::critical(QStringLiteral("更改小组件背景色失败"));
        }
    });

    contentSetLayout->addWidget(showContentLabel);
    contentSetLayout->addWidget(showChoice);
    backgroundSetLayout->addWidget(bgColorLabel);
    backgroundSetLayout->addWidget(bgColorBtn);
    widgetSetLayout->addSpacing(10);
    widgetSetLayout->addLayout(contentSetLayout);
    widgetSetLayout->addLayout(backgroundSetLayout);
    layout->addWidget(widgetSet);
    layout->addStretch(1);

    return page;
}

void MainWindow::updateButtonColor(QPushButton *button, const QColor &color)
{
    if (!button)
        return;
    button->setProperty("currentColor", color); // 供取色对话框读取当前颜色

    const QString style = QStringLiteral(
        "QPushButton#backgroundColorBtn {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: 1px solid #cccccc;"
        "   padding: 5px;"
        "   border-radius: 3px;"
        "}")
        .arg(color.name(), color.lightness() > 128 ? QStringLiteral("black") : QStringLiteral("white"));

    button->setStyleSheet(style);
    button->setText(tr("当前颜色: %1").arg(color.name()));
}

// ---------------- 文字编辑 ----------------

QWidget *MainWindow::createWordEditPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignTop);

    QLabel *title = new QLabel(tr("文字编辑"));
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold; color: #2c3e50; margin-bottom: 15px;"));

    QLabel *encourageLabel = new QLabel(tr("编辑鼓励语句（在首页中出现，每行一条）"));
    encourageLabel->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold; color: #2c3e50; margin-bottom: 15px;"));

    QTextEdit *textEdit = new QTextEdit;
    textEdit->setStyleSheet(
        "QTextEdit {"
        "   padding: 5px;"
        "   border: 1px solid #000000;"
        "   border-radius: 3px;"
        "   background-color: white;"
        "}");

    // 载入现有内容（一次性读全文，而不是循环 setText —— 修复原实现的低效写法）
    try {
        QFile file(quotesFilePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            textEdit->setPlainText(stream.readAll());
            file.close();
        } else {
            Logger::warning(QStringLiteral("编辑页无法打开鼓励语句文件: %1").arg(quotesFilePath));
        }
    } catch (...) {
        Logger::critical(QStringLiteral("载入鼓励语句到编辑框失败"));
    }

    QPushButton *save = new QPushButton(tr("保存"));
    save->setStyleSheet(
        "QPushButton {"
        "   background-color: #aaff7f;"
        "   color: black;"           // 修正原来拼错的 "balck"
        "   padding: 8px 16px;"
        "   border-radius: 4px;"
        "}"
        "QPushButton:hover { background-color: #55ff00; }");

    connect(save, &QPushButton::clicked, this, [this, textEdit, save]() {
        save->setEnabled(false); // 防抖：避免重复点击并发写入
        const auto restore = qScopeGuard([save] { save->setEnabled(true); });
        try {
            const QString content = textEdit->toPlainText();

            QFile file(quotesFilePath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                Logger::critical(QStringLiteral("保存鼓励语句失败: %1 (%2)")
                                     .arg(quotesFilePath, file.errorString()));
                QMessageBox::warning(this, tr("错误"),
                                     tr("文件保存失败：%1").arg(file.errorString()));
                return;
            }
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            stream << content;
            file.close();

            loadQuotes(); // 同步内存缓存
            ui->label_5->setText(randomQuote());
            Logger::info(QStringLiteral("鼓励语句已保存到 %1").arg(quotesFilePath));
            QMessageBox::information(this, tr("成功"), tr("文件保存成功！"));
        } catch (...) {
            Logger::critical(QStringLiteral("保存鼓励语句时发生异常"));
            QMessageBox::warning(this, tr("错误"), tr("文件保存时发生未知错误。"));
        }
    });

    QCheckBox *notice = new QCheckBox(tr("启动时推送鼓励语句"));
    bool noticeChecked = true;
    withSettings(configPath, [&](QSettings &s) {
        noticeChecked = s.value(QStringLiteral("Global/Notice"), true).toBool();
    });
    notice->setChecked(noticeChecked);
    connect(notice, &QCheckBox::stateChanged, this, [=](int state) {
        withSettings(configPath, [&](QSettings &s) {
            s.setValue(QStringLiteral("Global/Notice"), state == Qt::Checked);
            s.sync();
        });
        Logger::info(QStringLiteral("启动推送鼓励语句: %1").arg(state == Qt::Checked ? tr("开") : tr("关")));
    });
    notice->setStyleSheet(
        "QCheckBox {"
        "    spacing: 10px;"
        "    color: #202020;"
        "    font-size: 14px;"
        "    background: transparent;"
        "}"
        "QCheckBox::indicator {"
        "    width: 18px; height: 18px;"
        "    border-radius: 5px;"
        "    border: 1.5px solid #cccccc;"
        "    background-color: white;"
        "}"
        "QCheckBox::indicator:hover { border-color: #0078d7; background-color: #f0f7ff; }"
        "QCheckBox::indicator:pressed { background-color: #e5f1ff; }"
        "QCheckBox::indicator:checked { background-color: #0078d7; border-color: #0078d7; }"
        "QCheckBox::indicator:checked:hover { background-color: #0066c0; border-color: #0066c0; }"
        "QCheckBox::indicator:disabled { border: 1.5px solid #eeeeee; background-color: #f9f9f9; }"
        "QCheckBox::indicator:checked:disabled { background-color: #cccccc; border-color: #cccccc; }"
        "QCheckBox:disabled { color: #a0a0a0; }");

    layout->addWidget(title);
    layout->addWidget(encourageLabel);
    layout->addWidget(textEdit, 1);
    layout->addWidget(notice);
    layout->addWidget(save);
    layout->addStretch();

    return page;
}

// ---------------- 关于 ----------------

QWidget *MainWindow::createAboutPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);

    QLabel *title = new QLabel(tr("关于本软件"));
    title->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: bold; color: #3498db;"));
    title->setAlignment(Qt::AlignCenter);

    QLabel *version = new QLabel(tr("版本: %1").arg(QStringLiteral(APP_VERSION)));
    version->setAlignment(Qt::AlignCenter);

    QLabel *description = new QLabel(tr("这是一个对于高考考生的倒计时工具\n可以激励你为高考奋勇拼搏"));
    description->setAlignment(Qt::AlignCenter);
    description->setWordWrap(true);

    QPushButton *websiteBtn = new QPushButton(tr("访问 Github 项目主页"));
    websiteBtn->setStyleSheet(
        "QPushButton {"
        "   background-color: #3498db;"
        "   color: white;"
        "   padding: 8px 16px;"
        "   border-radius: 4px;"
        "}"
        "QPushButton:hover { background-color: #2980b9; }");
    websiteBtn->setCursor(Qt::PointingHandCursor);
    connect(websiteBtn, &QPushButton::clicked, this, []() {
        const bool ok = QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/BuBaiMengJ4/BattleForFuture")));
        if (!ok)
            Logger::warning(QStringLiteral("无法打开项目主页链接"));
    });

    layout->addStretch();
    layout->addWidget(title);
    layout->addSpacing(10);
    layout->addWidget(version);
    layout->addSpacing(20);
    layout->addWidget(description);
    layout->addSpacing(20);
    layout->addWidget(websiteBtn, 0, Qt::AlignCenter);
    layout->addStretch();

    return page;
}
