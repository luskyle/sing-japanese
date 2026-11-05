#include "backend.h"
#include "annotationview.h"

#include <QApplication>
#include <QFont>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName("假名小调");
    application.setOrganizationName("Sing Japanese");
    application.setApplicationVersion("0.3.0");
    application.setFont(QFont("Noto Sans CJK JP", 10));

    qmlRegisterType<AnnotationView>("SingJapanese", 1, 0, "AnnotationView");
    Backend backend;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    return application.exec();
}
