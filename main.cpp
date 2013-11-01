#include "mainwindow.h"
#include <QApplication>




int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("Atomidata");
    QCoreApplication::setOrganizationDomain("atomidata.com");
    QCoreApplication::setApplicationName("Webcamforward Client");
    MainWindow w;
    w.show();
    QObject::connect(&a, SIGNAL(aboutToQuit()), &w, SLOT(closing()));
    return a.exec();
}
