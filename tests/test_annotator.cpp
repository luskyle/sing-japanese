#include "annotator.h"
#include "annotationview.h"
#include "backend.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QImage>
#include <QKeyEvent>
#include <QMediaPlayer>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QScreen>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

class PreviewHandler : public QObject
{
    Q_OBJECT
public:
    QUrl url;
public slots:
    void open(const QUrl &value) { url = value; }
};

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

void requireInside(QQuickItem *item, QQuickItem *viewport, const char *message)
{
    require(item && viewport && item->isVisible(), message);
    const QRectF bounds = item->mapRectToItem(viewport, item->boundingRect());
    require(bounds.left() >= -1 && bounds.top() >= -1
                && bounds.right() <= viewport->width() + 1
                && bounds.bottom() <= viewport->height() + 1, message);
}

void pointerClick(QQuickWindow *window, const char *name)
{
    auto *item = qobject_cast<QQuickItem *>(control(window, name));
    requireInside(item, window->contentItem(), "pointer targets must be visible without page scrolling");
    const QPointF position = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
    QMouseEvent press(QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &press);
    QCoreApplication::sendEvent(window, &release);
    settle();
}

void checkReadableText(QQuickItem *item)
{
    if (!item->isVisible())
        return;
    const QVariant text = item->property("text");
    const QVariant font = item->property("font");
    if (text.isValid() && !text.toString().trimmed().isEmpty() && font.canConvert<QFont>()) {
        const int size = font.value<QFont>().pixelSize();
        if (size > 0 && size < 14) {
            std::cerr << "Unreadably small text: " << text.toString().toStdString()
                      << " (" << size << "px)\n";
            require(false, "visible interface text must be at least 14 logical pixels");
        }
    }
    for (QQuickItem *child : item->childItems())
        checkReadableText(child);
}

int countPaperViews(QQuickItem *item)
{
    int count = item->objectName().startsWith(QLatin1String("annotationPaper_")) ? 1 : 0;
    for (QQuickItem *child : item->childItems())
        count += countPaperViews(child);
    return count;
}

int kanaCount(const Backend &backend)
{
    int count = 0;
    for (const QVariant &row : backend.kanaRows())
        for (const QVariant &cell : row.toMap().value("cells").toList())
            count += !cell.toMap().isEmpty();
    return count;
}

void checkKanaOverview(QQuickWindow *window, const Backend &backend)
{
    auto *chart = qobject_cast<QQuickItem *>(control(window, "kanaChart"));
    requireInside(chart, window->contentItem(), "the complete kana chart must fit within the window");
    const int rowsPerBlock = chart->property("rowsPerBlock").toInt();
    require(rowsPerBlock > 0, "kana blocks must contain rows");
    const auto rows = backend.kanaRows();
    for (int row = 0; row < rows.size(); ++row) {
        const auto cells = rows.at(row).toMap().value("cells").toList();
        for (int column = 0; column < cells.size(); ++column) {
            const auto cell = cells.at(column).toMap();
            if (cell.isEmpty())
                continue;
            auto *button = qobject_cast<QQuickItem *>(control(window,
                ("kana_" + cell.value("id").toString()).toUtf8().constData()));
            requireInside(button, chart, "every kana must be visible at once without chart scrolling");
            auto *content = button->property("contentItem").value<QQuickItem *>();
            require(content, "kana buttons must expose their rendered text");
            for (auto *text : content->childItems()) {
                if (!text->isVisible())
                    continue;
                const bool fits = text->property("contentWidth").toReal() <= text->width() + 1
                    && text->property("contentHeight").toReal() <= text->height() + 1;
                if (!fits)
                    std::cerr << "Clipped kana text " << text->property("text").toString().toStdString()
                              << " in window " << window->width() << 'x' << window->height()
                              << ": content " << text->property("contentWidth").toReal() << 'x'
                              << text->property("contentHeight").toReal() << ", area "
                              << text->width() << 'x' << text->height() << '\n';
                require(fits, "kana and romaji must fit their cells without clipping");
            }
            const QString vowel = QStringLiteral("aiueo").mid(column, 1);
            auto *heading = qobject_cast<QQuickItem *>(control(window,
                ("vowel_" + QString::number(row / rowsPerBlock) + "_" + vowel).toUtf8().constData()));
            require(std::abs(heading->mapToScene(QPointF(heading->width() / 2, 0)).x()
                             - button->mapToScene(QPointF(button->width() / 2, 0)).x()) < 1,
                    "every block must align kana with the correct vowel heading");
        }
    }
}

QRect inkBounds(const QImage &image, const QRect &area, const QColor &color)
{
    QRect bounds;
    for (int y = area.top(); y <= area.bottom(); ++y)
        for (int x = area.left(); x <= area.right(); ++x)
            if (image.pixelColor(x, y) == color)
                bounds = bounds.united(QRect(x, y, 1, 1));
    return bounds;
}

void checkAnnotationLayout(const QString &directory)
{
    QFont base(QStringLiteral("Noto Sans CJK JP"));
    base.setPixelSize(16);
    QFont ruby = base;
    ruby.setPixelSize(10);
    const QFontMetricsF baseMetrics(base);
    const QFontMetricsF rubyMetrics(ruby);
    require(annotationLineHeight() >= baseMetrics.height() + rubyMetrics.height() + 16,
            "line height must reserve space for both fonts, ruby gap and interline gap");

    const QVector<Segment> sample{
        {QStringLiteral("私"), QStringLiteral("わたし"), false},
        {QStringLiteral("帰"), QStringLiteral("かえ"), false},
        {QStringLiteral("る"), {}, false},
        {QStringLiteral("家"), QStringLiteral("いえ"), false},
        {QStringLiteral("\n"), {}, false},
        {QStringLiteral("声"), QStringLiteral("こえ"), false}
    };
    const auto lines = layoutAnnotation(sample, 400, base, ruby);
    require(lines.size() == 2 && lines.first().size() == 4, "explicit lines must remain intact");
    constexpr int renderScale = 3;
    QImage image(400 * renderScale, static_cast<int>(std::ceil(2 * annotationLineHeight() * renderScale)),
                 QImage::Format_ARGB32);
    image.setDevicePixelRatio(renderScale);
    image.fill(Qt::white);
    QPainter painter(&image);
    drawAnnotationLines(painter, lines, 0, lines.size());
    painter.end();
    int previousBottom = -1;
    for (int index = 0; index < lines.size(); ++index) {
        int bottom = previousBottom;
        qreal previousRight = 0;
        for (const auto &placement : lines.at(index)) {
            require(placement.x >= previousRight, "neighboring annotation slots must not overlap");
            previousRight = placement.x + placement.width;
            const QRect area(static_cast<int>(std::floor(placement.x * renderScale)),
                             static_cast<int>(std::floor(index * annotationLineHeight() * renderScale)),
                             static_cast<int>(std::ceil(placement.width * renderScale)),
                             static_cast<int>(std::floor(annotationLineHeight() * renderScale)));
            const QRect textInk = inkBounds(image, area, QColor(35, 38, 42));
            require(!textInk.isEmpty(), "base text must be painted within its own slot");
            bottom = std::max(bottom, textInk.bottom());
            if (placement.segment.reading.isEmpty())
                continue;
            const QRect rubyInk = inkBounds(image, area, QColor(74, 79, 87));
            require(!rubyInk.isEmpty(), "ruby must be painted within its own slot");
            require(rubyInk.bottom() < textInk.top(), "ruby must be strictly above its base text");
            require(index == 0 || rubyInk.top() > previousBottom + 8 * renderScale,
                    "successive lines must have a clear visual gap");
            if (std::abs(rubyInk.center().x() - textInk.center().x()) > 3 * renderScale)
                std::cerr << "Ruby center for " << placement.segment.text.toStdString()
                          << ": base " << textInk.center().x() << ", ruby " << rubyInk.center().x() << '\n';
            require(std::abs(rubyInk.center().x() - textInk.center().x()) <= 3 * renderScale,
                    "painted ruby must be horizontally centered above its own kanji");
        }
        previousBottom = bottom;
    }

    const auto wrapped = layoutAnnotation(sample, 90, base, ruby);
    require(wrapped.size() > lines.size(), "narrow preview must wrap ruby and base together");
    QString reconstructed;
    int readingCount = 0;
    for (const auto &line : wrapped) {
        for (const auto &placement : line) {
            reconstructed += placement.segment.text;
            readingCount += !placement.segment.reading.isEmpty();
            require(placement.x + placement.width <= 78, "wrapped slots must fit within the right padding");
        }
    }
    require(reconstructed == QStringLiteral("私帰る家声") && readingCount == 4,
            "wrapping must not lose, duplicate or detach readings");

    const QString pdfPath = directory + "/alignment.pdf";
    QString error;
    const QVector<Segment> pdfSample{sample.at(0), sample.at(4), sample.at(5)};
    require(exportAnnotatedPdf(pdfPath, pdfSample, &error), "alignment PDF must export");
    const QString pdftoppm = QStandardPaths::findExecutable(QStringLiteral("pdftoppm"));
    if (!pdftoppm.isEmpty()) {
        QProcess render;
        const QString output = directory + "/alignment";
        constexpr int scale = 3;
        render.start(pdftoppm, {"-r", QString::number(96 * scale), "-singlefile", "-png", pdfPath, output});
        require(render.waitForFinished() && render.exitCode() == 0, "PDF must render successfully");
        const QImage pdfImage(output + ".png");
        require(!pdfImage.isNull(), "rendered PDF must be readable");
        const QRect allRuby = inkBounds(pdfImage, pdfImage.rect(), QColor(74, 79, 87));
        require(!allRuby.isEmpty(), "PDF must contain visible readings");
        int previousBottom = -1;
        for (int line = 0; line < 2; ++line) {
            const QRect area(0, allRuby.top() + static_cast<int>(line * annotationLineHeight() * scale),
                             pdfImage.width(), static_cast<int>((annotationLineHeight() - 8) * scale));
            const QRect text = inkBounds(pdfImage, area, QColor(35, 38, 42));
            const QRect reading = inkBounds(pdfImage, area, QColor(74, 79, 87));
            require(!text.isEmpty() && !reading.isEmpty(), "PDF must paint both base and reading");
            require(reading.bottom() < text.top(), "PDF ruby must be above its corresponding base");
            require(std::abs(reading.center().x() - text.center().x()) <= 3 * scale,
                    "PDF ruby must be centered above its corresponding base");
            require(reading.top() > previousBottom + 8 * scale, "PDF lines must not overlap");
            previousBottom = text.bottom();
        }
    }
}
}

int main(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
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
    const QVector<Segment> expectedReadings{
        {QStringLiteral("帰"), QStringLiteral("かえ"), false},
        {QStringLiteral("打"), QStringLiteral("う"), false},
        {QStringLiteral("思"), QStringLiteral("おも"), false},
        {QStringLiteral("出"), QStringLiteral("だ"), false},
        {QStringLiteral("祝"), QStringLiteral("いわ"), false},
        {QStringLiteral("今日"), QStringLiteral("きょう"), false}
    };
    const QString mixedInput = QStringLiteral("帰る。打たれて。思い出す。お祝い。今日。");
    const auto mixed = annotate(mixedInput);
    reconstructed.clear();
    for (const auto &segment : mixed)
        reconstructed += segment.text;
    require(reconstructed == mixedInput, "splitting okurigana must preserve the original text");
    for (const auto &expected : expectedReadings) {
        bool found = false;
        for (const auto &segment : mixed)
            found |= segment.text == expected.text && segment.reading == expected.reading;
        require(found, "readings must attach only to kanji, retaining compound readings as a group");
    }
    checkAnnotationLayout(directory.path());

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
        require(!backend.previewPdf() && !backend.previewDocx() && !backend.copyForWord(),
                "document actions must reject empty annotation results");
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
        backend.setRomaji(true);
        backend.setFullRomaji(true);
        require(backend.canExport(), "layer options must not invalidate current annotation");
        require(backend.saveDocx(directory.filePath("backend.docx")), "backend DOCX export must work");
        PreviewHandler previewHandler;
        QDesktopServices::setUrlHandler("file", &previewHandler, "open");
        require(backend.previewPdf() && previewHandler.url.toLocalFile().endsWith(".pdf")
                    && QFileInfo::exists(previewHandler.url.toLocalFile()),
                "PDF preview must open an actual generated PDF");
        require(backend.previewDocx() && previewHandler.url.toLocalFile().endsWith(".docx")
                    && QFileInfo::exists(previewHandler.url.toLocalFile()),
                "DOCX preview must open an actual generated DOCX");
        QDesktopServices::unsetUrlHandler("file");
        auto *clipboard = QApplication::clipboard();
        auto *originalClipboard = new QMimeData;
        if (const auto *current = clipboard->mimeData())
            for (const QString &format : current->formats())
                originalClipboard->setData(format, current->data(format));
        require(backend.copyForWord() && clipboard->mimeData()->hasHtml()
                    && clipboard->mimeData()->hasFormat("text/rtf"), "copy must populate the system clipboard");
        backend.setInput(input + "a");
        require(!backend.canExport(), "editing must invalidate export");
        require(!backend.savePdf(directory.filePath("stale.pdf")), "stale result export must be rejected");
        require(!backend.error().isEmpty(), "invalid export must explain the failure");
        const QString copied = clipboard->mimeData()->html();
        require(!backend.copyForWord() && clipboard->mimeData()->html() == copied,
                "stale copy must fail without replacing the clipboard");
        require(!backend.saveDocx(directory.filePath("stale.docx"))
                    && !backend.previewDocx() && !backend.previewPdf(),
                "all document actions must reject stale annotation");
        clipboard->setMimeData(originalClipboard);
    }

    Backend backend;
    qmlRegisterType<AnnotationView>("SingJapanese", 1, 0, "AnnotationView");
    qmlRegisterType<AnnotationPageView>("SingJapanese", 1, 0, "AnnotationPageView");
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
    require(qFuzzyCompare(window->devicePixelRatio(), window->screen()->devicePixelRatio()),
            "window must respect the screen DPI including native and application scale factors");
    const QSize available = window->screen()->availableGeometry().size();
    const QSize minimumSize(std::min(1120, available.width()), std::min(800, available.height()));
    require(window->minimumSize() == minimumSize,
            "window minimum must protect the readable layout while fitting the available screen");
    if (window->width() != std::min(1360, available.width())
        || window->height() != std::min(900, available.height()))
        std::cerr << "Initial window " << window->width() << 'x' << window->height()
                  << ", available screen " << available.width() << 'x' << available.height()
                  << ", device pixel ratio " << window->devicePixelRatio() << '\n';
    require(window->width() == std::min(1360, available.width())
                && window->height() == std::min(900, available.height()),
            "default window must be enlarged without exceeding the available screen");
    const QSize initialSize = window->size();
    window->resize(minimumSize - QSize(100, 100));
    settle();
    if (QGuiApplication::platformName() == QLatin1String("xcb"))
        require(window->width() >= minimumSize.width() && window->height() >= minimumSize.height(),
                "native window manager must reject resizing below the minimum dimensions");
    window->resize(initialSize);
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
    qreal fixedLayoutHeight = 0;
    for (const QSize size : {QSize(1920, 1080), QSize(1360, 900), QSize(1120, 800), minimumSize}) {
        window->resize(size);
        window->setProperty("page", 0);
        settle();
        auto *chart = qobject_cast<QQuickItem *>(control(window, "kanaChart"));
        require(chart->width() > 250 && chart->height() > 150, "kana chart must fit every supported window size");
        auto *details = qobject_cast<QQuickItem *>(control(window, "pronunciationDetails"));
        requireInside(details, window->contentItem(), "pronunciation details must fit without scrolling");
        require(control(window, "hiraganaButton")->property("font").value<QFont>().pixelSize() >= 16
                    && control(window, "groupFilter")->property("font").value<QFont>().pixelSize() >= 16,
                "kana controls must use readable font sizes");
        for (const bool katakana : {false, true}) {
            backend.setKatakana(katakana);
            for (const int groupIndex : {0, 1, 2, 3}) {
                backend.setGroupIndex(groupIndex);
                settle();
                checkKanaOverview(window, backend);
            }
            backend.setGroupIndex(0);
            settle();
            require(kanaCount(backend) == 71, "the overview must include all 71 kana");
            checkKanaOverview(window, backend);
            pointerClick(window, "kana_po");
            require(backend.selected().value("id") == "po",
                    "the last kana must be clickable without scrolling");
            backend.stopAudio();
            pointerClick(window, "romajiToggle");
            checkKanaOverview(window, backend);
            pointerClick(window, "romajiToggle");
        }
        backend.setKatakana(false);
        backend.setRowIndex(1);
        settle();
        checkKanaOverview(window, backend);
        backend.setRowIndex(0);
        settle();
        const QPointF chartPos = chart->mapToScene(QPointF());
        const QPointF detailPos = details->mapToScene(QPointF());
        require(detailPos.y() >= chartPos.y() + chart->height(),
                "pronunciation details must sit below the full-width kana overview");
        checkReadableText(window->contentItem());
        if (!screenshots.isEmpty()) {
            require(QDir().mkpath(screenshots), "screenshot directory must be available");
            require(window->grabWindow().save(screenshots + "/kana-" + QString::number(size.width()) + ".png"),
                    "kana screenshot must render");
        }
        click(window, "lyricsTab");
        require(window->property("page").toInt() == 1, "lyrics navigation must work");
        auto *workspace = qobject_cast<QQuickItem *>(control(window, "lyricsWorkspace"));
        auto *actions = qobject_cast<QQuickItem *>(control(window, "lyricsActions"));
        auto *panels = qobject_cast<QQuickItem *>(control(window, "lyricsPanels"));
        requireInside(panels, workspace, "lyrics panels must fit the available workspace");
        requireInside(actions, workspace, "lyrics actions must always remain visible");
        const qreal bottomGap = workspace->height()
            - actions->mapToItem(workspace, QPointF(0, actions->height())).y();
        require(bottomGap >= 0 && bottomGap <= 24
                    && panels->height() >= workspace->height() - 48 - 110 * window->property("uiScale").toReal()
                    && actions->y() - (panels->y() + panels->height()) <= 12,
                "paper panels must fill the remaining height with actions pinned to the bottom");
        for (const char *name : {"paperZoomButton", "annotateButton", "previewDocxButton",
                                 "previewPdfButton", "copyWordButton", "exportDocxButton", "exportButton"}) {
            auto *button = qobject_cast<QQuickItem *>(control(window, name));
            require(button && button->height() <= 46 * window->property("uiScale").toReal(),
                    "lyrics page action buttons must use compact sizing");
        }
        auto *editor = control(window, "lyricsInput");
        require(editor->property("font").value<QFont>().pixelSize() == 16,
                "input lyrics must use A4 print-scale 12-point text");
        const QString sample = QStringLiteral("私の帰る家は\nあなたの声のする街角\n冬の雨に打たれて\nあなたの足音をさがすのよ\n")
                               + QString(60, QChar(0x3042));
        editor->setProperty("text", sample);
        auto *inputViewport = qobject_cast<QQuickItem *>(control(window, "originalPaperScroll"));
        auto *editorItem = qobject_cast<QQuickItem *>(editor);
        inputViewport->setProperty("contentX", 0);
        inputViewport->setProperty("contentY", 0);
        settle();
        const QPointF editPosition = editorItem->mapToScene(QPointF(8, 8));
        QMouseEvent editPress(QEvent::MouseButtonPress, editPosition, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent editRelease(QEvent::MouseButtonRelease, editPosition, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &editPress);
        QCoreApplication::sendEvent(window, &editRelease);
        QKeyEvent keyPress(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
        QKeyEvent keyRelease(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
        QCoreApplication::sendEvent(window, &keyPress);
        QCoreApplication::sendEvent(window, &keyRelease);
        require(editorItem->hasActiveFocus() && backend.input().size() == sample.size() + 1,
                "scaled A4 original paper must remain clickable and editable with the keyboard");
        editor->setProperty("text", sample);
        click(window, "annotateButton");
        require(backend.canExport(), "QML annotation action must enable PDF export");
        require(control(window, "exportButton")->property("enabled").toBool(), "QML export must be enabled");
        auto *preview = qobject_cast<AnnotationView *>(control(window, "annotationPreview"));
        require(preview && preview->segments() == backend.segments(), "preview must use the backend result");
        require(preview->width() == annotationPrintableRect().width() && preview->implicitHeight() > 0,
                "preview layout must use the A4 printable width regardless of window size");
        require(qFuzzyCompare(backend.previewWidth(), preview->width())
                    && qFuzzyCompare(backend.previewFontScale(), qreal(1))
                    && qFuzzyCompare(preview->fontScale(), backend.previewFontScale()),
                "preview and exports must share a fixed A4 layout and print font size");
        if (fixedLayoutHeight == 0)
            fixedLayoutHeight = preview->implicitHeight();
        else
            require(qFuzzyCompare(preview->implicitHeight(), fixedLayoutHeight),
                    "resizing must not change the A4 line breaks");
        auto *originalPaper = qobject_cast<QQuickItem *>(control(window, "originalPaper"));
        auto *annotationPaper = qobject_cast<AnnotationPageView *>(control(window, "annotationPaper_0"));
        require(originalPaper && annotationPaper
                    && originalPaper->width() == annotationPaperSize().width()
                    && originalPaper->height() == annotationPaperSize().height()
                    && annotationPaper->width() == annotationPaperSize().width()
                    && annotationPaper->height() == annotationPaperSize().height(),
                "short input and annotated output must appear on A4-sized paper");
        require(originalPaper->scale() <= 1 && annotationPaper->scale() <= 1
                    && originalPaper->scale() > 0 && annotationPaper->scale() > 0,
                "A4 pages must fit their available viewport width by default");
        const auto editorPos = qobject_cast<QQuickItem *>(editor)->mapToScene(QPointF());
        const auto previewPos = annotationPaper->mapToScene(QPointF());
        require(size.width() < 760 ? previewPos.y() > editorPos.y() : previewPos.x() > editorPos.x(),
                "lyric panels must stack in narrow windows and sit side by side in wide windows");
        checkReadableText(window->contentItem());
        const qreal kanaHeight = preview->implicitHeight();
        pointerClick(window, "romajiOption");
        require(backend.romaji() && !backend.fullRomaji() && preview->romaji()
                    && preview->implicitHeight() > kanaHeight, "first checkbox must add a romaji row");
        pointerClick(window, "fullRomajiOption");
        require(backend.romaji() && backend.fullRomaji() && preview->fullRomaji(),
                "second checkbox must add a layer without removing the first");
        const int pageCount = preview->pageCount();
        pointerClick(window, "paperZoomButton");
        require(qFuzzyCompare(originalPaper->scale(), qreal(1))
                    && qFuzzyCompare(annotationPaper->scale(), qreal(1))
                    && preview->pageCount() == pageCount
                    && backend.previewWidth() == annotationPrintableRect().width()
                    && qFuzzyCompare(backend.previewFontScale(), qreal(1)),
                "100% zoom must enlarge only the display, not reflow the document");
        auto *outputItem = qobject_cast<QQuickItem *>(control(window, "annotationScroll"));
        if (inputViewport->width() < originalPaper->width()) {
            require(inputViewport->property("contentWidth").toReal() > inputViewport->width()
                        && outputItem->property("contentWidth").toReal() > outputItem->width(),
                    "100% zoom must allow horizontal scrolling on both A4 pages");
            inputViewport->setProperty("contentX", inputViewport->property("contentWidth").toReal()
                                       - inputViewport->width());
            outputItem->setProperty("contentX", outputItem->property("contentWidth").toReal()
                                    - outputItem->width());
        }
        pointerClick(window, "paperZoomButton");
        require(originalPaper->scale() <= 1 && annotationPaper->scale() <= 1,
                "fit-to-width zoom must restore the paper preview");
        require(inputViewport->property("contentX").toReal() <= 1
                    && outputItem->property("contentX").toReal() <= 1,
                "returning to fit-to-width must restore the left edge of both pages");
        require(outputItem && outputItem->height()
                    >= annotationLineHeight({true, true, backend.previewFontScale()}) + 24,
                "small windows must show at least one complete line with all annotation layers");
        for (const char *name : {"previewDocxButton", "previewPdfButton", "copyWordButton",
                                 "exportDocxButton", "exportButton"}) {
            auto *button = qobject_cast<QQuickItem *>(control(window, name));
            const QPointF position = button->mapToScene(QPointF(0, 0));
            require(button->isEnabled() && position.x() >= 0 && position.y() >= 0
                        && position.x() + button->width() <= window->width()
                        && position.y() + button->height() <= window->height(),
                    "every document action must be enabled and reachable within the window");
        }
        if (!screenshots.isEmpty()) {
            require(window->grabWindow().save(screenshots + "/lyrics-" + QString::number(size.width()) + ".png"),
                    "lyrics screenshot must render");
        }
        pointerClick(window, "romajiOption");
        require(!backend.romaji() && backend.fullRomaji(), "unchecking one option must leave the other active");
        pointerClick(window, "fullRomajiOption");
        require(!backend.romaji() && !backend.fullRomaji(), "unchecking both must restore kana-only rendering");
        editor->setProperty("text", QStringLiteral("変更"));
        require(!control(window, "exportButton")->property("enabled").toBool(), "edits must disable stale export");
        for (const char *name : {"previewDocxButton", "previewPdfButton", "copyWordButton", "exportDocxButton"})
            require(!control(window, name)->property("enabled").toBool(), "edits must disable stale document actions");
        editor->setProperty("text", longInput);
        editorItem->forceActiveFocus();
        editor->setProperty("cursorPosition", longInput.size());
        settle();
        const QRectF cursor = editor->property("cursorRectangle").toRectF();
        const QPointF cursorBottom = editorItem->mapToItem(inputViewport, cursor.bottomRight());
        require(originalPaper->height() > annotationPaperSize().height()
                    && cursorBottom.y() >= 0 && cursorBottom.y() <= inputViewport->height() + 1,
                "long original text must remain editable and scroll to the caret");
        click(window, "annotateButton");
        auto *output = control(window, "annotationScroll");
        require(output->property("contentHeight").toReal() > output->property("height").toReal(),
                "long annotations must have scrollable preview content");
        require(preview->pageCount() > 1, "long annotations must show multiple A4 pages");
        output->setProperty("contentY", output->property("contentHeight").toReal()
                            - output->property("height").toReal());
        settle();
        require(control(window, ("annotationPaper_" + QString::number(preview->pageCount() - 1))
                                  .toUtf8().constData()),
                "the last A4 preview page must remain reachable");
        click(window, "kanaTab");
    }
    click(window, "lyricsTab");
    auto *stressEditor = control(window, "lyricsInput");
    constexpr int stressLines = 9999;
    const QString stressText = QStringLiteral("あ\n").repeated(stressLines) + QStringLiteral("終");
    stressEditor->setProperty("text", stressText);
    auto *stressEditorItem = qobject_cast<QQuickItem *>(stressEditor);
    auto *stressInput = qobject_cast<QQuickItem *>(control(window, "originalPaperScroll"));
    stressEditorItem->forceActiveFocus();
    stressEditor->setProperty("cursorPosition", stressText.size());
    settle();
    const QRectF stressCursor = stressEditor->property("cursorRectangle").toRectF();
    const QPointF stressCursorBottom = stressEditorItem->mapToItem(stressInput, stressCursor.bottomRight());
    require(stressCursorBottom.y() >= 0 && stressCursorBottom.y() <= stressInput->height() + 1,
            "near-limit original text must scroll to the active caret");
    QKeyEvent stressKeyPress(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
    QKeyEvent stressKeyRelease(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
    QCoreApplication::sendEvent(window, &stressKeyPress);
    QCoreApplication::sendEvent(window, &stressKeyRelease);
    require(backend.input().size() == 20000 && backend.input().endsWith(QStringLiteral("終a")),
            "the original editor must accept keyboard input through the 20,000-character limit");
    click(window, "annotateButton");
    auto *stressPreview = qobject_cast<AnnotationView *>(control(window, "annotationPreview"));
    auto *stressOutput = control(window, "annotationScroll");
    require(stressPreview && stressPreview->height() <= 1,
            "the hidden annotation layout source must not allocate a document-sized surface");
    require(backend.canExport() && stressPreview->pageCount()
                == (stressLines + 1 + annotationLinesPerPage(annotationPrintableRect().width()) - 1)
                    / annotationLinesPerPage(annotationPrintableRect().width()),
            "long original text must produce all of its A4 preview pages");
    stressOutput->setProperty("contentY", stressOutput->property("contentHeight").toReal()
                              - stressOutput->property("height").toReal());
    settle();
    require(control(window, ("annotationPaper_" + QString::number(stressPreview->pageCount() - 1))
                              .toUtf8().constData()),
            "the last page of a near-limit annotation must be reachable");
    require(countPaperViews(window->contentItem()) < 20,
            "A4 previews must instantiate only visible pages, not one image per document page");
    require(warnings.isEmpty(), "QML must not emit binding, layout, or runtime warnings");
    std::cout << "QML UI, kana filtering, audio, annotation layers, document previews and PDF/DOCX/clipboard checks passed.\n";
    return 0;
}

#include "test_annotator.moc"
