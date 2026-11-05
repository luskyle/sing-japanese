#include "annotator.h"
#include "annotationview.h"
#include "backend.h"

#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QMediaPlayer>
#include <QMouseEvent>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "Test failed: " << message << '\n';
        std::exit(1);
    }
}

void settle()
{
    QEventLoop loop;
    QTimer::singleShot(100, &loop, &QEventLoop::quit);
    loop.exec();
}

QQuickItem *visualControl(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name)
        return root;
    for (QQuickItem *child : root->childItems())
        if (auto *item = visualControl(child, name))
            return item;
    return nullptr;
}

QObject *control(QObject *root, const char *name)
{
    auto *result = root->findChild<QObject *>(QString::fromLatin1(name));
    if (!result)
        if (auto *window = qobject_cast<QQuickWindow *>(root))
            result = visualControl(window->contentItem(), QString::fromLatin1(name));
    require(result != nullptr, name);
    return result;
}

void click(QObject *root, const char *name)
{
    require(QMetaObject::invokeMethod(control(root, name), "clicked"), "QML button must be clickable");
    settle();
}

void pointerClick(QQuickWindow *window, const char *name)
{
    auto *item = qobject_cast<QQuickItem *>(control(window, name));
    require(item != nullptr, "pointer target must be a visual item");
    const QPointF position = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
    QMouseEvent press(QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &press);
    QCoreApplication::sendEvent(window, &release);
    settle();
}

int kanaCount(const Backend &backend)
{
    int count = 0;
    for (const QVariant &row : backend.kanaRows())
        for (const QVariant &cell : row.toMap().value("cells").toList())
            count += !cell.toMap().isEmpty();
    return count;
}
}

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory must be created");
    qputenv("XDG_CACHE_HOME", directory.path().toUtf8());
    const QString input = QStringLiteral("  今日、学校へ行く。\r\n\nあいうえお  ");
    const QVector<Segment> segments = annotate(input);
    QString reconstructed;
    bool foundSchool = false;
    for (const Segment &segment : segments) {
        reconstructed += segment.text;
        if (segment.text == QStringLiteral("学校") && segment.reading == QStringLiteral("がっこう"))
            foundSchool = true;
    }
    require(reconstructed == input, "annotation must preserve every input character");
    require(foundSchool, "school must be read as がっこう");
    require(toHiragana(QStringLiteral("ガッコウ")) == QStringLiteral("がっこう"),
            "katakana readings must convert to hiragana");
    const QVector<Segment> unknown = annotate(QStringLiteral("𠮷"));
    require(unknown.size() == 1 && unknown.first().unknown, "unknown kanji must remain visible and marked");
    bool rejectedLongInput = false;
    try { annotate(QString(20001, QChar(0x3042))); }
    catch (const std::exception &) { rejectedLongInput = true; }
    require(rejectedLongInput, "input over 20,000 characters must be rejected");

    QString longInput;
    for (int line = 0; line < 80; ++line) longInput += QStringLiteral("学校へ行く。\n");
    const QString pdfPath = directory.filePath(QStringLiteral("test.pdf"));
    QString error;
    require(exportAnnotatedPdf(pdfPath, annotate(longInput), &error), "A4 PDF must export successfully");
    QFile pdf(pdfPath);
    require(pdf.open(QIODevice::ReadOnly) && pdf.read(5) == QByteArray("%PDF-"), "exported file must be a PDF");
    const QString pdfinfo = QStandardPaths::findExecutable(QStringLiteral("pdfinfo"));
    if (!pdfinfo.isEmpty()) {
        QProcess inspect;
        inspect.start(pdfinfo, {pdfPath});
        require(inspect.waitForFinished() && inspect.exitCode() == 0, "pdfinfo must finish successfully");
        const QString details = QString::fromLocal8Bit(inspect.readAllStandardOutput());
        require(details.contains("(A4)"), "exported PDF page size must be A4");
        const auto pageMatch = QRegularExpression("Pages:\\s+(\\d+)").match(details);
        require(pageMatch.hasMatch() && pageMatch.captured(1).toInt() > 1, "long lyrics must span PDF pages");
    }

    {
        Backend backend;
        require(kanaCount(backend) == 71, "kana model must contain 71 entries");
        require(QDir(":/audio").entryList({"*.ogg"}).size() == 71, "all recordings must be bundled");
        backend.setGroupIndex(2);
        require(kanaCount(backend) == 20 && backend.selected().value("character") == QStringLiteral("が"),
                "voiced group must show 20 kana and select が");
        backend.setKatakana(true);
        require(backend.selected().value("character") == QStringLiteral("ガ"), "script switching must update selection");
        backend.setGroupIndex(3);
        require(kanaCount(backend) == 5 && backend.rowIndex() == 0, "group changes must reset the row");
        backend.setGroupIndex(0);
        backend.setKatakana(false);
        backend.setRowIndex(1);
        require(kanaCount(backend) == 5 && backend.selected().value("id") == "a", "row filtering must work");
        backend.setGroupIndex(1);
        require(kanaCount(backend) == 46, "basic group must contain 46 kana");
        backend.setGroupIndex(0);
        backend.setInput(input);
        backend.annotateText();
        require(backend.canExport(), "annotation must enable export");
        require(backend.savePdf(directory.filePath("backend.pdf")), "backend PDF export must work");
        backend.setInput(input + "a");
        require(!backend.canExport(), "editing must invalidate export");
        require(!backend.savePdf(directory.filePath("stale.pdf")), "stale result export must be rejected");
        require(!backend.error().isEmpty(), "invalid export must explain the failure");
    }

    Backend backend;
    qmlRegisterType<AnnotationView>("SingJapanese", 1, 0, "AnnotationView");
    QQmlApplicationEngine engine;
    QList<QQmlError> warnings;
    QObject::connect(&engine, &QQmlEngine::warnings, [&warnings](const QList<QQmlError> &items) {
        warnings.append(items);
    });
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/qml/Main.qml"));
    require(!engine.rootObjects().isEmpty(), "QML window must load");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    require(window, "QML root must be a window");
    require(backend.segments().isEmpty(), "new UI must start with an empty annotation preview");
    settle();
    click(window, "katakanaButton");
    require(backend.katakana(), "QML katakana button must update the backend");
    click(window, "hiraganaButton");
    require(!backend.katakana(), "QML hiragana button must update the backend");
    auto *group = control(window, "groupFilter");
    group->setProperty("currentIndex", 2);
    require(QMetaObject::invokeMethod(group, "activated", Q_ARG(int, 2)), "filter must expose activated");
    require(backend.groupIndex() == 2, "QML group filter must update the backend");
    auto *row = control(window, "rowFilter");
    row->setProperty("currentIndex", 2);
    require(QMetaObject::invokeMethod(row, "activated", Q_ARG(int, 2)), "row filter must expose activated");
    require(backend.rowIndex() == 2 && kanaCount(backend) == 5, "QML row filter must show one row");
    backend.setGroupIndex(0);
    settle();
    pointerClick(window, "romajiToggle");
    require(!control(window, "selectedRomaji")->property("visible").toBool(), "romaji toggle must hide readings");
    pointerClick(window, "romajiToggle");
    require(control(window, "selectedRomaji")->property("visible").toBool(), "romaji toggle must restore readings");
    pointerClick(window, "kana_ka");
    require(backend.selected().value("id") == "ka", "clicking a kana must select its pronunciation");
    click(window, "replayButton");
    require(backend.error().isEmpty(), "offline playback must have an available multimedia service");
    backend.stopAudio();

    const QString screenshots = qEnvironmentVariable("SING_JAPANESE_SCREENSHOTS");
    for (const QSize size : {QSize(1120, 800), QSize(800, 620)}) {
        window->resize(size);
        window->setProperty("page", 0);
        settle();
        auto *chart = qobject_cast<QQuickItem *>(control(window, "kanaChart"));
        require(chart->width() > 250 && chart->height() > 150, "kana chart must fit both window sizes");
        auto *details = qobject_cast<QQuickItem *>(control(window, "pronunciationScroll"));
        require(details && details->clip(), "pronunciation details must scroll without overflowing");
        if (!screenshots.isEmpty()) {
            require(QDir().mkpath(screenshots), "screenshot directory must be available");
            require(window->grabWindow().save(screenshots + "/kana-" + QString::number(size.width()) + ".png"),
                    "kana screenshot must render");
        }
        click(window, "lyricsTab");
        require(window->property("page").toInt() == 1, "lyrics navigation must work");
        auto *editor = control(window, "lyricsInput");
        editor->setProperty("text", QStringLiteral("今日、学校へ行く。\n空を見上げる。"));
        click(window, "annotateButton");
        require(backend.canExport(), "QML annotation action must enable PDF export");
        require(control(window, "exportButton")->property("enabled").toBool(), "QML export must be enabled");
        auto *preview = qobject_cast<AnnotationView *>(control(window, "annotationPreview"));
        require(preview && preview->segments() == backend.segments(), "preview must use the backend result");
        require(preview->width() > 200 && preview->implicitHeight() > 0, "preview must have a usable size");
        if (!screenshots.isEmpty())
            require(window->grabWindow().save(screenshots + "/lyrics-" + QString::number(size.width()) + ".png"),
                    "lyrics screenshot must render");
        editor->setProperty("text", QStringLiteral("変更"));
        require(!control(window, "exportButton")->property("enabled").toBool(), "edits must disable stale export");
        editor->setProperty("text", longInput);
        click(window, "annotateButton");
        auto *output = control(window, "annotationScroll");
        require(output->property("contentHeight").toReal() > output->property("height").toReal(),
                "long annotations must have scrollable preview content");
        click(window, "kanaTab");
    }
    require(warnings.isEmpty(), "QML must not emit binding, layout, or runtime warnings");
    std::cout << "QML UI, kana filtering, offline audio, annotation and A4 PDF checks passed.\n";
    return 0;
}
