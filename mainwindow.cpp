#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->CountdownGK->display(calculateRemainingDays(QDate(2026,6,7)));
    ui->CountdownYM->display(calculateRemainingDays(QDate(2026,3,17)));

    //从文件中读取鼓励语句(如若后期添加，尽量不超过20字，避免显示不全）
    QFile file(":/text/quotes.txt");
    if(file.open(QIODevice::ReadOnly)) {
        QTextStream stream(&file);
        while(!stream.atEnd()) {
            quotes << stream.readLine();
        }
        file.close();
    }

    QFont font("楷体",20);
    ui->label_5->setFont(font);
    ui->label_5->setText(quotes[QRandomGenerator::global()->bounded(quotes.size())]);  //随机生成一条鼓励语句
}

MainWindow::~MainWindow()
{
    delete ui;
}


//计算剩余天数
int MainWindow::calculateRemainingDays(const QDate &targetDate) {
    QDate currentDate = QDate::currentDate();
    int days = currentDate.daysTo(targetDate);
    return (days > 0) ? days : 0; // 若目标日期已过当天，返回0
}

void MainWindow::on_Settings_clicked()
{
    SettingWindow->setWindowTitle("应用设置");
    SettingWindow->resize(600,400);
    SettingWindow->setMinimumSize(600,400);

    //删除旧的布局样式，避免重复
    if (QLayout* oldLayout = SettingWindow->layout()) {
        delete oldLayout;
    }

    //设置窗口主布局
    QHBoxLayout *mainLayout = new QHBoxLayout(SettingWindow);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 左侧区域（分类列表）
    QFrame *leftFrame = new QFrame;
    leftFrame->setStyleSheet("background-color: #2c3e50; color: white;"); //浅蓝色背景
    leftFrame->setMinimumWidth(200);
    leftFrame->setMaximumWidth(200);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftFrame);
    leftLayout->setContentsMargins(0, 10, 0, 0);//10像素为标题留空间

    // 右侧区域（设置页面）
    QVBoxLayout *rightLayout = new QVBoxLayout;

    // 添加分隔线
    QFrame *divider = new QFrame;
    divider->setFrameShape(QFrame::VLine);
    divider->setFrameShadow(QFrame::Sunken);// 凹陷阴影效果
    divider->setStyleSheet("background-color: #d1d5db;"); // 浅灰色

    //将左右区域和分割线加入主布局
    mainLayout->addWidget(leftFrame);
    mainLayout->addWidget(divider);
    mainLayout->addLayout(rightLayout, 1);

    //加入左侧的导航
    categoryList = createCategoryList();
    leftLayout->addWidget(categoryList);

    //右侧详细内容
    if (!pagesWidget) {
    pagesWidget = new QStackedWidget(SettingWindow);
    pagesWidget->addWidget(createGeneralPage());
    pagesWidget->addWidget(createAppearancePage());
    pagesWidget->addWidget(createAdvancedPage());
    pagesWidget->addWidget(createAboutPage());
    }
    rightLayout->addWidget(pagesWidget);
    pagesWidget->setCurrentIndex(0);

    //窗口切换动画（向左向右滑动）
    QPropertyAnimation* stackAnimation = new QPropertyAnimation(pagesWidget, "pos", this);
    stackAnimation->setDuration(300);
    stackAnimation->setEasingCurve(QEasingCurve::OutQuad); // 平滑曲线
    connect(categoryList, &QListWidget::currentRowChanged, [=](int index) {
        //检测索引是否有效
        if (index < 0 || index >= pagesWidget->count()) return;
        QWidget* currentPage = pagesWidget->currentWidget();
        QWidget* nextPage = pagesWidget->widget(index);

        //根据目标索引判断滑动方向（向右滑动为1，向左滑动为-1）
        int direction = (index > pagesWidget->currentIndex()) ? 1 : -1;
        nextPage->move(direction * pagesWidget->width(), 0);
        nextPage->show();

        QPropertyAnimation* currentAnim = new QPropertyAnimation(currentPage, "pos");//控制当前页移除
        QPropertyAnimation* nextAnim = new QPropertyAnimation(nextPage, "pos");//控制目标页移入

        currentAnim->setDuration(300);nextAnim->setDuration(300);
        //先快后慢的动画效果
        currentAnim->setEasingCurve(QEasingCurve::OutQuad);nextAnim->setEasingCurve(QEasingCurve::OutQuad);

        //设置动画位置
        currentAnim->setStartValue(currentPage->pos());
        currentAnim->setEndValue(QPoint(-direction * pagesWidget->width(), 0));

        nextAnim->setStartValue(nextPage->pos());
        nextAnim->setEndValue(QPoint(0, 0));

        connect(nextAnim, &QPropertyAnimation::finished, [=]() {
            pagesWidget->setCurrentIndex(index);
            currentPage->move(0, 0); // 重置位置，防止位置状态污染
        });

        currentAnim->start();
        nextAnim->start();
    });

    SettingWindow->exec();
}


//创建左侧导航栏
QListWidget* MainWindow::createCategoryList() {
    QListWidget* categoryList =new QListWidget;
    categoryList->  setStyleSheet(
        "QListWidget {"
        "   background-color: #2c3e50;"
        "   border: none;"
        "   font-size: 14px;"
        "   outline: none"   //去掉虚线焦点轮廓
        "}"
        "QListWidget::item {"
        "   padding: 12px 15px;"  //增加点击区域
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
        "常规设置", "界面选项","高级设置", "关于"
    };

    for (const QString &category : categories) {
        QListWidgetItem *item = new QListWidgetItem(category, categoryList);
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);//文本左对齐且垂直居中
    }
    return categoryList;
}

//设置“常规设置”详细内容
QWidget* MainWindow::createGeneralPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignTop);

    QLabel *title = new QLabel("常规设置");
    title->setStyleSheet("font-size: 18px; font-weight: bold; color: #2c3e50; margin-bottom: 15px;");

    // 语言设置
    QGroupBox *languageGroup = new QGroupBox("语言选项");
    languageGroup->setStyleSheet("QGroupBox { font-weight: bold; }");
    QVBoxLayout *languageLayout = new QVBoxLayout(languageGroup);

    QComboBox *languageCombo = new QComboBox;
    languageCombo->addItems({"简体中文", "English", "Español", "Français", "日本語"});
    languageLayout->addWidget(languageCombo);

    // 启动选项
    QGroupBox *startupGroup = new QGroupBox("启动选项");
    startupGroup->setStyleSheet("QGroupBox { font-weight: bold; }");
    QVBoxLayout *startupLayout = new QVBoxLayout(startupGroup);

    QCheckBox *autoStart = new QCheckBox("开机自动启动");
    QCheckBox *checkUpdates = new QCheckBox("启动时自动检查更新");

    startupLayout->addWidget(autoStart);
    startupLayout->addWidget(checkUpdates);

    layout->addWidget(title);
    layout->addWidget(languageGroup);
    layout->addWidget(startupGroup);

    return page;
}

//设置“界面选项”详细内容
QWidget* MainWindow::createAppearancePage() {
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignTop);

    QLabel *title = new QLabel("界面选项");
    title->setStyleSheet("font-size: 18px; font-weight: bold; color: #2c3e50; margin-bottom: 15px;");

    // 主题设置
    QGroupBox *themeGroup = new QGroupBox("主题设置");
    themeGroup->setStyleSheet("QGroupBox { font-weight: bold; }");
    QVBoxLayout *themeLayout = new QVBoxLayout(themeGroup);

    QComboBox *themeCombo = new QComboBox;
    themeCombo->addItems({"浅色主题", "深色主题", "自动（跟随系统）"});
    themeLayout->addWidget(themeCombo);

    // 字体设置
    QGroupBox *fontGroup = new QGroupBox("字体设置");
    fontGroup->setStyleSheet("QGroupBox { font-weight: bold; }");
    QGridLayout *fontLayout = new QGridLayout(fontGroup);

    QLabel *fontLabel = new QLabel("界面字体:");
    QComboBox *fontCombo = new QComboBox;
    fontCombo->addItems(QFontDatabase::families());

    QLabel *sizeLabel = new QLabel("字体大小:");
    QComboBox *sizeCombo = new QComboBox;
    sizeCombo->addItems({"9", "10", "11", "12", "13", "14", "15", "16"});

    QCheckBox *customFont = new QCheckBox("使用自定义字体");

    fontLayout->addWidget(fontLabel, 0, 0);
    fontLayout->addWidget(fontCombo, 0, 1);
    fontLayout->addWidget(sizeLabel, 1, 0);
    fontLayout->addWidget(sizeCombo, 1, 1);
    fontLayout->addWidget(customFont, 2, 0, 1, 2);

    layout->addWidget(title);
    layout->addWidget(themeGroup);
    layout->addWidget(fontGroup);
    layout->addStretch();

    return page;
}

//设置“高级设置”详细内容
QWidget* MainWindow::createAdvancedPage() {
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignTop);

    QLabel *title = new QLabel("高级设置");
    title->setStyleSheet("font-size: 18px; font-weight: bold; color: #2c3e50; margin-bottom: 15px;");

    // 网络设置
    QGroupBox *networkGroup = new QGroupBox("网络设置");
    networkGroup->setStyleSheet("QGroupBox { font-weight: bold; }");
    QGridLayout *networkLayout = new QGridLayout(networkGroup);

    QLabel *proxyLabel = new QLabel("代理服务器:");
    QLineEdit *proxyEdit = new QLineEdit;
    QLabel *portLabel = new QLabel("端口:");
    QLineEdit *portEdit = new QLineEdit;
    QCheckBox *useProxy = new QCheckBox("使用代理服务器");

    networkLayout->addWidget(useProxy, 0, 0, 1, 2);
    networkLayout->addWidget(proxyLabel, 1, 0);
    networkLayout->addWidget(proxyEdit, 1, 1);
    networkLayout->addWidget(portLabel, 2, 0);
    networkLayout->addWidget(portEdit, 2, 1);

    // 开发者选项
    QGroupBox *devGroup = new QGroupBox("开发者选项");
    devGroup->setStyleSheet("QGroupBox { font-weight: bold; }");
    QVBoxLayout *devLayout = new QVBoxLayout(devGroup);

    QCheckBox *debugMode = new QCheckBox("启用调试模式");
    QCheckBox *logVerbose = new QCheckBox("详细日志记录");
    QCheckBox *experimental = new QCheckBox("显示实验性功能");

    devLayout->addWidget(debugMode);
    devLayout->addWidget(logVerbose);
    devLayout->addWidget(experimental);

    layout->addWidget(title);
    layout->addWidget(networkGroup);
    layout->addWidget(devGroup);
    layout->addStretch();

    return page;
}

//设置“关于”详细内容
QWidget* MainWindow::createAboutPage() {
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);

    QLabel *title = new QLabel("关于本软件");
    title->setStyleSheet("font-size: 24px; font-weight: bold; color: #3498db;");
    title->setAlignment(Qt::AlignCenter);

    QLabel *version = new QLabel("版本: 1.0.0 (Build 2025)");
    version->setAlignment(Qt::AlignCenter);

    QLabel *description = new QLabel(
        "这是一个对于2026届高考考生的倒计时工具\n"
        "可以激励你为高考奋勇拼搏"
        );
    description->setAlignment(Qt::AlignCenter);
    description->setWordWrap(true);

    QPushButton *websiteBtn = new QPushButton("访问Github 项目主页");
    websiteBtn->setStyleSheet(
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
    websiteBtn->setCursor(Qt::PointingHandCursor);
    connect(websiteBtn,&QPushButton::clicked,[=]()
            {QDesktopServices::openUrl(QUrl("https://github.com/your-username/your-project"));});

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

void MainWindow::on_pushButton_clicked()
{
    QDialog *WidgetWindow = new QDialog(this);
    WidgetWindow->setObjectName("WidgetWindow");
    WidgetWindow->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool); //隐藏标题栏和任务栏图标

    QScreen *desktop=QApplication::primaryScreen();
    WidgetWindow->resize(250,200);
    WidgetWindow->move(desktop->geometry().width()-WidgetWindow->width(),0);
    WidgetWindow->setStyleSheet("#WidgetWindow {background-color: rgb(255, 212, 0);border: 1px solid #E0D0A0;"
                                "border-radius: 10px;}");  //黄色背景，圆角
    WidgetWindow->show();

    QPushButton* Exit = new QPushButton(WidgetWindow);QPushButton* ReturnMain = new QPushButton(WidgetWindow);
    Exit->setText("退出");ReturnMain->setText("返回主界面");
    Exit->setGeometry(10,179,50,20);ReturnMain->setGeometry(140,179,100,20);
    Exit->show();ReturnMain->show();

    connect(Exit, SIGNAL(clicked(bool)), qApp, SLOT(quit()));
    connect(ReturnMain,SIGNAL(clicked(bool)),this,SLOT(show()));
    connect(ReturnMain,SIGNAL(clicked(bool)),WidgetWindow,SLOT(hide()));

    QFont temp("宋体",20);
    QLCDNumber* LCD = new QLCDNumber(WidgetWindow);
    QLabel* label_6 = new QLabel(WidgetWindow);
    QLabel* label_7 = new QLabel(WidgetWindow);

    LCD->move(30,30);LCD->setDigitCount(3);LCD->setSegmentStyle(QLCDNumber::Flat);
    LCD->setFrameShadow(QFrame::Raised);LCD->resize(200,100);LCD->setFrameShape(QFrame::NoFrame);
    LCD->setStyleSheet("color: rgb(255, 0, 0);");

    label_7->move(70,140);label_6->move(10,0);
    label_6->setFont(temp); label_7->setFont(temp);
    label_6->setText("还有");

    if(calculateRemainingDays(QDate(2026,3,17)))
    {
        LCD->display(calculateRemainingDays(QDate(2026,3,17)));
        label_7->setText("天到一模");
    }
    else
    {
        LCD->display(calculateRemainingDays(QDate(2026,6,7)));
        label_7->setText("天到高考");
    }

    LCD->show();
    label_6->show();
    label_7->show();

    //设置窗口阴影效果
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(WidgetWindow);
    shadow->setBlurRadius(30);
    shadow->setColor(Qt::gray);
    shadow->setOffset(0, 0);
    WidgetWindow->setGraphicsEffect(shadow);

    this->hide();
}

