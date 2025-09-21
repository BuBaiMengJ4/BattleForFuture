#include "mainwindow.h"

#include <QApplication>
#include <QTranslator>
#include "mainwindow.h"


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    //QTranslator *translator = new QTranslator;
    //translator->load(appDir);
    //a.installTranslator(translator);

    MainWindow w;

    //将窗口显示于屏幕中心
    QScreen *desktop =QApplication::primaryScreen();
    w.move((desktop->geometry().width()-w.width())/2,(desktop->geometry().height()-w.height())/2);      
    return a.exec();
}
