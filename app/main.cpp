#include "backend.h"
#include "annotationview.h"
#include "bundledfont.h"

#include <QApplication>
#include <QAudioDeviceInfo>
#include <QFont>
#include <QMessageBox>
#include <QDebug>
#include <QTemporaryDir>
#include <QTimer>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("唱日语"));
    application.setOrganizationName("Sing Japanese");
    application.setApplicationVersion(SING_JAPANESE_VERSION);
    if (!loadBundledFont()) {
        QMessageBox::critical(nullptr, QStringLiteral("唱日语"), QStringLiteral("无法加载内置日语字体。请重新安装应用。"));
        return 1;
    }
    application.setFont(QFont("Noto Sans CJK JP", 10));

    qmlRegisterType<AnnotationView>("SingJapanese", 1, 0, "AnnotationView");
    qmlRegisterType<AnnotationPageView>("SingJapanese", 1, 0, "AnnotationPageView");
    Backend backend;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    if (application.arguments().contains(QStringLiteral("--smoke-test"))) {
        QTemporaryDir output;
        if (!output.isValid()) {
            qCritical() << "Cannot create smoke-test output directory";
            return 1;
        }
        backend.setInput(QStringLiteral("私は学校へ帰る。"));
        backend.annotateText();
        backend.setRomaji(true);
        backend.setFullRomaji(true);
        if (!backend.canExport() || !backend.savePdf(output.filePath("test.pdf"))
            || !backend.saveDocx(output.filePath("test.docx"))) {
            qCritical() << backend.error();
            return 1;
        }
        QMediaPlayer audioService;
        if (!audioService.isAvailable()) {
            qCritical() << "Qt Multimedia playback service is unavailable";
            return 1;
        }
        const auto audioDevices = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
        qInfo() << "Smoke-test audio output devices:" << audioDevices.size();
        if (audioDevices.isEmpty())
            qInfo() << "No audio output device; hardware playback skipped, multimedia service verified";
        else
            backend.replay();
        QTimer::singleShot(1000, &application, [&] {
            if (!backend.error().isEmpty())
                qCritical() << backend.error();
            application.exit(backend.error().isEmpty() ? 0 : 1);
        });
        return application.exec();
    }
    return application.exec();
}
