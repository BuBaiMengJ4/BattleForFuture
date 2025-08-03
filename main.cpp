#include "mainwindow.h"

#include <QApplication>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    QScreen *desktop =QApplication::primaryScreen();
    w.move((desktop->geometry().width()-w.width())/2,(desktop->geometry().height()-w.height())/2);
    w.show();
    return a.exec();
}
