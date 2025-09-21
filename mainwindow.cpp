#include "mainwindow.h"

#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget * parent): QMainWindow(parent), ui(new Ui::MainWindow) {
    ui -> setupUi(this);

    //读取设置内容，并判断需要呈现的窗口
    if (QFile::exists(appDir + "//config.ini")) {
        QSettings settings(appDir + "//config.ini", QSettings::IniFormat);
        if (settings.value("Start/StartAsWidget").toBool())
            this -> show();
        else
            on_pushButton_clicked();
    } else
        this -> show();


    ui -> CountdownGK -> display(calculateRemainingDays(QDate(2026, 6, 7)));
    ui -> CountdownYM -> display(calculateRemainingDays(QDate(2026, 3, 17)));

    //从文件中读取鼓励语句(如若后期添加，尽量不超过20字，避免显示不全）
    QFont font("楷体", 20);
    ui -> label_5 -> setFont(font);
    if(!QFile::exists(filePath)){
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream stream( & file);
        while (!stream.atEnd()) {
            quotes << stream.readLine();
        }
        file.close();
    }
    ui -> label_5 -> setText(quotes[QRandomGenerator::global() -> bounded(quotes.size())]); //随机生成一条鼓励语句
    }
    else
    {
    ui -> label_5 ->setText("请在设置中添加鼓励语");
    }


}

MainWindow::~MainWindow() {
    delete ui;
}


//计算剩余天数
int MainWindow::calculateRemainingDays(const QDate & targetDate) {
    QDate currentDate = QDate::currentDate();
    int days = currentDate.daysTo(targetDate);
    return (days > 0) ? days : 0; // 若目标日期已过当天，返回0
}

void MainWindow::on_Settings_clicked() {
    SettingWindow -> setWindowTitle(tr("应用设置"));
    SettingWindow -> resize(600, 400);
    SettingWindow -> setMinimumSize(600, 400);

    //删除旧的布局样式，避免重复
    if (QLayout * oldLayout = SettingWindow -> layout()) {
        delete oldLayout;
    }

    //设置窗口主布局
    QHBoxLayout * mainLayout = new QHBoxLayout(SettingWindow);
    mainLayout -> setSpacing(0);
    mainLayout -> setContentsMargins(0, 0, 0, 0);

    // 左侧区域（分类列表）
    QFrame * leftFrame = new QFrame;
    leftFrame -> setStyleSheet("background-color: #2c3e50; color: white;"); //浅蓝色背景
    leftFrame -> setMinimumWidth(200);
    leftFrame -> setMaximumWidth(200);
    QVBoxLayout * leftLayout = new QVBoxLayout(leftFrame);
    leftLayout -> setContentsMargins(0, 10, 0, 0); //10像素为标题留空间

    // 右侧区域（设置页面）
    QVBoxLayout * rightLayout = new QVBoxLayout;

    // 添加分隔线
    QFrame * divider = new QFrame;
    divider -> setFrameShape(QFrame::VLine);
    divider -> setFrameShadow(QFrame::Sunken); // 凹陷阴影效果
    divider -> setStyleSheet("background-color: #d1d5db;"); // 浅灰色

    //将左右区域和分割线加入主布局
    mainLayout -> addWidget(leftFrame);
    mainLayout -> addWidget(divider);
    mainLayout -> addLayout(rightLayout, 1);

    //加入左侧的导航
    categoryList = createCategoryList();
    leftLayout -> addWidget(categoryList);

    //右侧详细内容
    if (!pagesWidget) {
        pagesWidget = new QStackedWidget(SettingWindow);
        pagesWidget -> addWidget(createGeneralPage());
        pagesWidget -> addWidget(createAppearancePage());
        pagesWidget -> addWidget(createWordEditPage());
        pagesWidget -> addWidget(createAboutPage());
    }
    rightLayout -> addWidget(pagesWidget);

    pagesWidget -> setCurrentIndex(0);
    //窗口切换动画（向左向右滑动）
    QPropertyAnimation * stackAnimation = new QPropertyAnimation(pagesWidget, "pos", this);
    stackAnimation -> setDuration(300);
    stackAnimation -> setEasingCurve(QEasingCurve::OutQuad); // 平滑曲线
    connect(categoryList, & QListWidget::currentRowChanged, [ = ](int index) {
        //检测索引是否有效
        if (index < 0 || index >= pagesWidget -> count()) return;
        QWidget * currentPage = pagesWidget -> currentWidget();
        QWidget * nextPage = pagesWidget -> widget(index);

        //根据目标索引判断滑动方向（向右滑动为1，向左滑动为-1）
        int direction = (index > pagesWidget -> currentIndex()) ? 1 : -1;
        nextPage -> move(direction * pagesWidget -> width(), 0);
        nextPage -> show();

        QPropertyAnimation * currentAnim = new QPropertyAnimation(currentPage, "pos"); //控制当前页移除
        QPropertyAnimation * nextAnim = new QPropertyAnimation(nextPage, "pos"); //控制目标页移入

        currentAnim -> setDuration(300);
        nextAnim -> setDuration(300);
        //先快后慢的动画效果
        currentAnim -> setEasingCurve(QEasingCurve::OutQuad);
        nextAnim -> setEasingCurve(QEasingCurve::OutQuad);

        //设置动画位置
        currentAnim -> setStartValue(currentPage -> pos());
        currentAnim -> setEndValue(QPoint(-direction * pagesWidget -> width(), 0));

        nextAnim -> setStartValue(nextPage -> pos());
        nextAnim -> setEndValue(QPoint(0, 0));

        connect(nextAnim, & QPropertyAnimation::finished, [ = ]() {
            pagesWidget -> setCurrentIndex(index);
            currentPage -> move(0, 0); // 重置位置，防止位置状态污染
        });

        currentAnim -> start();
        nextAnim -> start();
    });

    SettingWindow -> exec();
}


//创建左侧导航栏
QListWidget * MainWindow::createCategoryList() {
    QListWidget * categoryList = new QListWidget;
    categoryList -> setStyleSheet(
        "QListWidget {"
        "   background-color: #2c3e50;"
        "   border: none;"
        "   font-size: 14px;"
        "   outline: none" //去掉虚线焦点轮廓
        "}"
        "QListWidget::item {"
        "   padding: 12px 15px;" //增加点击区域
        "   border-bottom: 1px solid #34495e;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: #3498db;"
        "   color: white;"
        "}"
        "QListWidget::item:hover:!selected {"
        "   background-color: #34495e;"
        "}"
        );

    // 添加分类项
    QStringList categories = {
        tr("常规设置"),
        tr("外观设置"),
        tr("文字编辑"),
        tr("关于")
    };

    for (const QString & category: categories) {
        QListWidgetItem * item = new QListWidgetItem(category, categoryList);
        item -> setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter); //文本左对齐且垂直居中
    }

    categoryList -> setCurrentRow(0); //显示菜单栏的第一栏被点击，优化视觉体验
    return categoryList;
}

//设置“常规设置”详细内容
QWidget * MainWindow::createGeneralPage() {
    QWidget * page = new QWidget;
    QVBoxLayout * layout = new QVBoxLayout(page);
    layout -> setAlignment(Qt::AlignTop);

    QLabel * title = new QLabel(tr("常规设置"));
    title -> setStyleSheet("font-size: 18px; font-weight: bold; color: #2c3e50; margin-bottom: 15px;");

    // 启动选项
    QGroupBox * startupGroup = new QGroupBox(tr("启动选项"));
    startupGroup -> setStyleSheet(
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
        "}"
        );
    QVBoxLayout * startupLayout = new QVBoxLayout(startupGroup);

    QCheckBox * autoStart = new QCheckBox(tr("开机自动启动"));
    QCheckBox * smallAsWidget = new QCheckBox(tr("启动时自动打开到小部件"));
    startupLayout -> addWidget(autoStart);
    startupLayout -> addWidget(smallAsWidget);

    //读取设置并设置复选框初始状态
    QSettings initSettings(appDir + "//config.ini", QSettings::IniFormat);
    initSettings.beginGroup("Start");
    bool startAsWidget = initSettings.value("StartAsWidget", 1).toBool();
    smallAsWidget -> setChecked(!startAsWidget);
    initSettings.endGroup();

    QSettings Startsetting("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    QString appName = QCoreApplication::applicationName();
    QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    bool autostart = Startsetting.contains(appName);
    autoStart -> setChecked(autostart);

    QHBoxLayout * showlanguageLayout = new QHBoxLayout;
    QLabel * showlanguage = new QLabel;
    showlanguage -> setText(tr("显示语言为："));
    QComboBox * showlanguageCombo = new QComboBox;
    showlanguageCombo -> addItem("简体中文");
    showlanguageCombo -> addItem("繁體中文");
    showlanguageCombo -> setStyleSheet(
        "QComboBox {"
        "   padding: 5px;"
        "   border: 1px solid #dcdcdc;"
        "   border-radius: 3px;"
        "   background-color: white;"
        "}"
        "QComboBox::drop-down {"
        "   border: none;"
        "}"
        "QComboBox QAbstractItemView {"
        "padding: 5px;"
        "border: 1px solid rgb(31, 156, 220);"
        "border-radius: 3px;"
        "color: black;"
        "background-color: white;"
        "selection-background-color: rgb(90, 90, 90);"
        "}"
        );
    showlanguageLayout -> addWidget(showlanguage);
    showlanguageLayout -> addWidget(showlanguageCombo);

    connect(showlanguageCombo,&QComboBox::currentIndexChanged,showlanguageCombo,[=](int state)
    {
        QSettings *read = new QSettings(appDir+"\\config.ini",QSettings::IniFormat);
        read->beginGroup("Global");
        showlanguageCombo->setCurrentIndex(read->value("language").toInt());
        read->endGroup();
        delete read;
    });

    //设置高考年份，最多从当前年份向后推6年
    QLabel* showacademicyear = new QLabel;
    QHBoxLayout* showacademicyearlayout =new QHBoxLayout;
    QComboBox* academicyear = new QComboBox;

    showacademicyear->setText("请选择你的参加高考年份：");
    int year = QDateTime::currentDateTime().date().year();
    for (int i=year;i<=year+6;i++)
    {
        academicyear->addItem(QString::number(i));
    };
    academicyear -> setStyleSheet(
        "QComboBox {"
        "   padding: 5px;"
        "   border: 1px solid #dcdcdc;"
        "   border-radius: 3px;"
        "   background-color: white;"
        "}"
        "QComboBox::drop-down {"
        "   border: none;"
        "}"
        "QComboBox QAbstractItemView {"
        "padding: 5px;"
        "border: 1px solid rgb(31, 156, 220);"
        "border-radius: 3px;"
        "color: black;"
        "background-color: white;"
        "selection-background-color: rgb(90, 90, 90);"
        "}"
        );
    showacademicyearlayout->addWidget(showacademicyear);
    showacademicyearlayout->addWidget(academicyear);

    //写入高考年份设置
    connect(academicyear,&QComboBox::currentTextChanged,academicyear,[=](QString text)
    {
    QSettings* write = new QSettings(appDir+"\\config.ini",QSettings::IniFormat);
    write ->beginGroup("Global");
    write ->setValue("academicyear",text);
    write ->endGroup();
    delete write;
    });

    //设置开机自启动
    connect(autoStart, & QCheckBox::stateChanged, pagesWidget, [ = ](int state) {
        // 防止递归调用
        if (m_isSettingAutoStart) return;

        //Windows下的开机自启动
        #ifdef Q_OS_WINDOWS
        QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
        if (state == Qt::Checked) {
            m_isSettingAutoStart = true;
            QMessageBox::StandardButton reply = QMessageBox::question(pagesWidget, tr("确认"), tr("确定要设置为开机自启动吗？"), QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                settings.setValue(appName, appPath);
            } else {
                // 延迟设置，避免立即触发信号
                QMetaObject::invokeMethod(pagesWidget, [ = ] {
                    autoStart -> setChecked(false);
                    m_isSettingAutoStart = false;
                }, Qt::QueuedConnection);
                return; // 直接返回，不执行后续代码
            }
            //重置状态
            m_isSettingAutoStart = false;
        } else {
            settings.remove(appName);
        }
        #endif

    });


    //写入“开机自动缩小为小部件”配置
    connect(smallAsWidget, & QCheckBox::stateChanged, [ = ](int state) {
        QSettings *settings = new QSettings(appDir + "//config.ini", QSettings::IniFormat);
        settings->beginGroup("Start");
        if (state == Qt::Checked) {
            settings->setValue("StartAsWidget", 0);
        } else {
            settings->setValue("StartAsWidget", 1);
        }
        settings->endGroup();
        settings->sync();
        delete settings;
    });

    //写入语言配置
    connect(showlanguageCombo,&QComboBox::currentIndexChanged,[=](int index){
        QSettings *settings = new QSettings(appDir + "//config.ini", QSettings::IniFormat);
        settings->beginGroup("Global");
        settings->setValue("language", index);
        settings->endGroup();
        settings->sync();
        delete settings;
    });

    layout -> addWidget(title);
    layout -> addLayout(showlanguageLayout);
    layout -> addLayout(showacademicyearlayout);
    layout -> addWidget(startupGroup);
    layout -> addSpacing(3);
    return page;
}

//设置“外观设置”选项
QWidget * MainWindow::createAppearancePage() {
    QWidget * page = new QWidget;
    QVBoxLayout * layout = new QVBoxLayout(page);
    layout -> setAlignment(Qt::AlignTop);

    // 标题标签
    QLabel * title = new QLabel(tr("外观设置"));
    title -> setStyleSheet("font-size: 18px; font-weight: bold; color: #2c3e50; margin-bottom: 15px;");
    layout -> addWidget(title);

    // 小组件设置组
    QGroupBox * widgetSet = new QGroupBox(tr("小组件设置"));
    widgetSet -> setStyleSheet(
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
        "}"
        );
    QVBoxLayout * widgetSetLayout = new QVBoxLayout(widgetSet);
    QHBoxLayout * contentSetLayout = new QHBoxLayout();
    QHBoxLayout * backgrandSetLayout = new QHBoxLayout();

    // 显示内容选择
    QLabel * showContentLabel = new QLabel(tr("请选择显示内容："));
    QComboBox * showChoice = new QComboBox();
    showChoice -> addItem(tr("高考"));
    showChoice -> addItem(tr("一模"));
    showChoice -> setStyleSheet(
        "QComboBox {"
        "   padding: 5px;"
        "   border: 1px solid #dcdcdc;"
        "   border-radius: 3px;"
        "   background-color: white;"
        "}"
        "QComboBox::drop-down {"
        "   border: none;"
        "}"
        "QComboBox QAbstractItemView {"
        "padding: 5px;"
        "border: 1px solid rgb(31, 156, 220);"
        "border-radius: 3px;"
        "color: black;"
        "background-color: white;"
        "selection-background-color: rgb(90, 90, 90);"
        "}"
        );
    QSettings * read = new QSettings(appDir + "//config.ini", QSettings::IniFormat);
    read -> beginGroup("Widget");
    showChoice -> setCurrentIndex(read -> value("showcontent").toInt());
    read -> endGroup();
    delete read;

    connect(showChoice, & QComboBox::currentIndexChanged, pagesWidget, [ = ](int index) {
        QSettings* write = new QSettings(appDir + "//config.ini", QSettings::IniFormat);
        write->beginGroup("Widget");
        write->setValue("showcontent", index);
        write->endGroup();
        write->sync();
    });

    // 背景色设置
    QLabel * setBackgroundColorLabel = new QLabel(tr("设置小组件背景色："));
    QPushButton * backgroundColorBtn = new QPushButton(tr("选择颜色"));
    backgroundColorBtn -> setObjectName("backgroundColorBtn");


    // 添加布局
    contentSetLayout -> addWidget(showContentLabel);
    contentSetLayout -> addWidget(showChoice);
    widgetSetLayout -> addSpacing(10);
    backgrandSetLayout -> addWidget(setBackgroundColorLabel);
    backgrandSetLayout -> addWidget(backgroundColorBtn);
    widgetSetLayout -> addLayout(contentSetLayout);
    widgetSetLayout -> addLayout(backgrandSetLayout);
    layout -> addWidget(widgetSet);
    layout -> addStretch(1);

    // 从设置加载初始颜色
    QSettings settings(appDir + "//config.ini", QSettings::IniFormat);
    settings.beginGroup("Widget");
    QColor initialColor = settings.value("BackGrandColor", QColor(255, 0, 0)).value < QColor > (); //若为读取到，使用默认颜色（255,0,0）
    settings.endGroup();


    // 设置初始样式
    updateButtonColor(backgroundColorBtn, initialColor);

    connect(backgroundColorBtn, & QPushButton::clicked, [this, backgroundColorBtn]() {
        QColor currentColor = backgroundColorBtn -> palette().button().color();
        QColor color = QColorDialog::getColor(currentColor, this, tr("请选择颜色"));

        if (color.isValid()) {
            // 更新按钮颜色显示
            updateButtonColor(backgroundColorBtn, color);

            // 保存颜色设置
            QSettings settings(appDir + "//config.ini", QSettings::IniFormat);
            settings.beginGroup("Widget");
            settings.setValue("BackGrandColor", color);
            settings.endGroup();
        }
    });

    return page;
}

// 辅助函数：更新按钮颜色显示
void MainWindow::updateButtonColor(QPushButton * button,
                                   const QColor & color) {
    QString style = QString("QPushButton#backgroundColorBtn { "
                            "background-color: %1; "
                            "color: %2; "
                            "border: 1px solid #cccccc; "
                            "padding: 5px; "
                            "border-radius: 3px; }")
                        .arg(color.name())
                        .arg(color.lightness() > 128 ? "black" : "white"); //确保了文本在任何背景色上都有良好的可读性

    button -> setStyleSheet(style);
    button -> setText(QString(tr("当前颜色: %1")).arg(color.name()));
}

//设置“文字编辑”详细内容
QWidget * MainWindow::createWordEditPage() {
    QWidget * page = new QWidget;
    QVBoxLayout * layout = new QVBoxLayout(page);
    layout -> setAlignment(Qt::AlignTop);

    QLabel * title = new QLabel(tr("文字编辑"));
    title -> setStyleSheet("font-size: 18px; font-weight: bold; color: #2c3e50; margin-bottom: 15px;");
    title -> show();

    //编辑鼓励话语
    QLabel * EncourageText = new QLabel;
    EncourageText -> setText(tr("编辑鼓励语句（在首页中出现）"));
    EncourageText -> setStyleSheet("font-size: 14px; font-weight: bold; color: #2c3e50; margin-bottom: 15px;");
    QPushButton * save = new QPushButton;
    save -> setText(tr("保存"));
    save -> setStyleSheet(
        "QPushButton {"
        "   background-color: #aaff7f;"
        "   color: balck;"
        "   padding: 8px 16px;"
        "   border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #55ff00;"
        "}"
        );

    QTextEdit * TextForEncourage = new QTextEdit;
    TextForEncourage -> setStyleSheet(
        "QTextEdit{"
        "   padding: 5px;"
        "   border: 1px solid #000000;"
        "   border-radius: 3px;"
        "   background-color: white;}");

    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream stream( & file);
        while (!stream.atEnd()) {
            TextForEncourage -> setText(stream.readAll());
        }
        file.close();
    }

    QVBoxLayout* pushlayout = new QVBoxLayout;

    connect(save, & QPushButton::clicked, [ = ]() {
        QString Write = TextForEncourage -> toPlainText();

        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream( & file);
            stream.setEncoding(QStringConverter::Utf8);
            stream << Write;
            file.close();
            QMessageBox::information(pagesWidget, tr("成功"), tr("文件保存成功！"));
        };
    });

    QCheckBox * notice = new QCheckBox(tr("启动时推送鼓励语句"));
    connect(notice, & QCheckBox::stateChanged, [ = ](int state) {
        QSettings *settings = new QSettings(appDir + "//config.ini", QSettings::IniFormat);
        settings->beginGroup("Global");
        if (state == Qt::Checked) {
            settings->setValue("Notice", 0);
        } else {
            settings->setValue("Notice", 1);
        }
        settings->endGroup();
        settings->sync();
        delete settings;
    });

    notice->setStyleSheet(
        "QCheckBox {"
        "    spacing: 10px;"
        "    color: #202020;"
        "    font-size: 14px;"
        "    background: transparent;"
        "}"
        "QCheckBox::indicator {"
        "    width: 18px;"
        "    height: 18px;"
        "    border-radius: 5px;"
        "    border: 1.5px solid #cccccc;"
        "    background-color: white;"
        "}"
        "QCheckBox::indicator:hover {"
        "    border-color: #0078d7;"
        "    background-color: #f0f7ff;"
        "}"
        "QCheckBox::indicator:pressed {"
        "    background-color: #e5f1ff;"
        "}"
        "QCheckBox::indicator:checked {"
        "    background-color: #0078d7;"
        "    border-color: #0078d7;"
        "    image: url(:/images/checkmark.svg);"
        "}"
        "QCheckBox::indicator:checked:hover {"
        "    background-color: #0066c0;"
        "    border-color: #0066c0;"
        "}"
        "QCheckBox::indicator:disabled {"
        "    border: 1.5px solid #eeeeee;"
        "    background-color: #f9f9f9;"
        "}"
        "QCheckBox::indicator:checked:disabled {"
        "    background-color: #cccccc;"
        "    border-color: #cccccc;"
        "}"
        "QCheckBox:disabled {"
        "    color: #a0a0a0;"
        "}"
        );
    layout -> addWidget(title);
    layout -> addWidget(EncourageText);
    layout -> addWidget(TextForEncourage, 1);
    layout -> addWidget(notice);
    layout -> addWidget(save);
    layout -> addStretch();

    return page;
}


//设置“关于”详细内容
QWidget * MainWindow::createAboutPage() {
    QWidget * page = new QWidget;
    QVBoxLayout * layout = new QVBoxLayout(page);
    layout -> setAlignment(Qt::AlignCenter);

    QLabel * title = new QLabel(tr("关于本软件"));
    title -> setStyleSheet("font-size: 24px; font-weight: bold; color: #3498db;");
    title -> setAlignment(Qt::AlignCenter);

    QLabel * version = new QLabel(tr("版本: 2.0.0 (Build 2025)"));
    version -> setAlignment(Qt::AlignCenter);

    QLabel * description = new QLabel(
        tr("这是一个对于高考考生的倒计时工具\n"
           "可以激励你为高考奋勇拼搏")
        );
    description -> setAlignment(Qt::AlignCenter);
    description -> setWordWrap(true);

    QPushButton * websiteBtn = new QPushButton(tr("访问Github 项目主页"));
    websiteBtn -> setStyleSheet(
        "QPushButton {"
        "   background-color: #3498db;"
        "   color: white;"
        "   padding: 8px 16px;"
        "   border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #2980b9;"
        "}"
        );
    websiteBtn -> setCursor(Qt::PointingHandCursor);
    connect(websiteBtn, & QPushButton::clicked, [ = ]() {
        QDesktopServices::openUrl(QUrl("https://github.com/BuBaiMengJ4/BattleForFuture"));
    });

    layout -> addStretch();
    layout -> addWidget(title);
    layout -> addSpacing(10);
    layout -> addWidget(version);
    layout -> addSpacing(20);
    layout -> addWidget(description);
    layout -> addSpacing(20);
    layout -> addWidget(websiteBtn, 0, Qt::AlignCenter);
    layout -> addStretch();

    return page;
}

void MainWindow::on_pushButton_clicked() {
    QDialog * WidgetWindow = new QDialog(this);
    WidgetWindow -> setObjectName("WidgetWindow");
    WidgetWindow -> setWindowFlags(Qt::FramelessWindowHint | Qt::Tool); //隐藏标题栏和任务栏图标
    setAttribute(Qt::WA_Hover); //启动鼠标追踪，能够处理鼠标事件

    QScreen * desktop = QApplication::primaryScreen();
    WidgetWindow -> resize(250, 200);
    WidgetWindow -> move(desktop -> geometry().width() - WidgetWindow -> width(), 0);
    //读取用户预设样式
    QSettings* settings = new QSettings(appDir + "//config.ini", QSettings::IniFormat);
    settings->beginGroup("Widget");
    QColor initialColor = settings->value("BackGrandColor", QColor(255, 0, 0)).value < QColor > ();
    bool showcontent = settings->value("showcontent").toBool();
    settings->endGroup();
    QString style = QString("#WidgetWindow {background-color: %1;border: 1px solid #E0D0A0;border-radius: 10px;}").arg(initialColor.name());
    WidgetWindow -> setStyleSheet(style);
    WidgetWindow -> show();

    QPushButton * Exit = new QPushButton(WidgetWindow);
    QPushButton * ReturnMain = new QPushButton(WidgetWindow);
    Exit -> setText(tr("退出"));
    ReturnMain -> setText(tr("返回主界面"));
    Exit -> setGeometry(10, 179, 50, 20);
    ReturnMain -> setGeometry(140, 179, 100, 20);
    Exit -> show();
    ReturnMain -> show();

    connect(Exit, SIGNAL(clicked(bool)), qApp, SLOT(quit()));
    connect(ReturnMain, SIGNAL(clicked(bool)), this, SLOT(show()));
    connect(ReturnMain, SIGNAL(clicked(bool)), WidgetWindow, SLOT(hide()));

    QFont temp("宋体", 20);
    QLCDNumber * LCD = new QLCDNumber(WidgetWindow);
    QLabel * label_6 = new QLabel(WidgetWindow);
    QLabel * label_7 = new QLabel(WidgetWindow);

    LCD -> move(30, 30);
    LCD -> setDigitCount(3);
    LCD -> setSegmentStyle(QLCDNumber::Flat);
    LCD -> setFrameShadow(QFrame::Raised);
    LCD -> resize(200, 100);
    LCD -> setFrameShape(QFrame::NoFrame);
    LCD -> setStyleSheet("color: rgb(255, 0, 0);");

    label_7 -> move(70, 140);
    label_6 -> move(10, 0);
    label_6 -> setFont(temp);
    label_7 -> setFont(temp);
    label_6 -> setText(tr("还有"));

    //根据用户配置更改倒计时显示
    if (showcontent) {
        LCD -> display(calculateRemainingDays(QDate(2026, 3, 17)));
        label_7 -> setText(tr("天到一模"));
    } else {
        LCD -> display(calculateRemainingDays(QDate(2026, 6, 7)));
        label_7 -> setText(tr("天到高考"));
    }

    LCD -> show();
    label_6 -> show();
    label_7 -> show();

    //设置窗口阴影效果
    QGraphicsDropShadowEffect * shadow = new QGraphicsDropShadowEffect(WidgetWindow);
    shadow -> setBlurRadius(30);
    shadow -> setColor(Qt::gray);
    shadow -> setOffset(0, 0);
    WidgetWindow -> setGraphicsEffect(shadow);

    this -> hide();
}
