#include "mainwindow.h"

#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName("假名小调");
    application.setOrganizationName("Sing Japanese");
    application.setApplicationVersion("0.2.0");
    application.setFont(QFont("Noto Sans CJK JP", 10));

    MainWindow window;
    window.show();
    return application.exec();
}
