#include "mainwindow.h"
#include <QTranslator>
#include <QApplication>


int main(int argc, char * argv[]) {
    QApplication a(argc, argv);

    MainWindow w;

    //若配置文件中为空，也默认不加载翻译
    QSettings * settings = new QSettings(QCoreApplication::applicationDirPath() + "//config.ini", QSettings::IniFormat);
    QTranslator * translator = new QTranslator(qApp);
    settings -> beginGroup("Global");
    if (settings -> value("language").toBool()) {
        qDebug() << "fish";
        if (translator -> load(":/Translation/BattleForFuture_zh_HK.qm"))
            QCoreApplication::installTranslator(translator);
    }
    settings -> endGroup();
    delete settings;

    //将窗口显示于屏幕中心
    QScreen * desktop = QApplication::primaryScreen();
    w.move((desktop -> geometry().width() - w.width()) / 2, (desktop -> geometry().height() - w.height()) / 2);
    return a.exec();
}
