#include "annotator.h"
#include "annotationview.h"
#include "documentexport.h"
#include "romaji.h"
#include "bundledfont.h"

#include <unzip.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontMetricsF>
#include <QPainter>
#include <QProcess>
#include <QPrinter>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "Document test failed: " << message << '\n';
        std::exit(1);
    }
}

QByteArray archiveEntry(unzFile archive, const QByteArray &name)
{
    require(unzLocateFile(archive, name.constData(), 1) == UNZ_OK, "DOCX must contain the requested part");
    require(unzOpenCurrentFile(archive) == UNZ_OK, "DOCX part must open");
    QByteArray result;
    char buffer[8192];
    int count = 0;
    while ((count = unzReadCurrentFile(archive, buffer, sizeof(buffer))) > 0)
        result.append(buffer, count);
    require(count == 0 && unzCloseCurrentFile(archive) == UNZ_OK, "DOCX part must pass its CRC check");
    return result;
}

int pdfPageCount(const QString &path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "printed PDF must be readable");
    const QByteArray data = file.readAll();
    require(data.startsWith("%PDF-"), "printer output must be a PDF document");
    return QString::fromLatin1(data.constData(), data.size()).count(QRegularExpression("/Type\\s*/Page\\b"));
}

bool hasInk(const QImage &image, const QColor &color)
{
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.pixelColor(x, y) == color)
                return true;
    return false;
}

QRect inkBounds(const QImage &image, const QColor &color)
{
    int left = image.width();
    int top = image.height();
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < image.height(); ++y) {
        const auto *pixels = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (pixels[x] != color.rgb())
                continue;
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x);
            bottom = y;
        }
    }
    return right < left ? QRect() : QRect(QPoint(left, top), QPoint(right, bottom));
}

void checkPaperGeometry()
{
    const QSize paper = annotationPaperSize();
    const QRect printable = annotationPrintableRect();
    require(std::abs(paper.width() - 210.0 * 96 / 25.4) <= 1
                && std::abs(paper.height() - 297.0 * 96 / 25.4) <= 1,
            "preview paper must match A4 dimensions at 96 DPI");
    require(std::abs(printable.left() - 18.0 * 96 / 25.4) <= 1
                && std::abs(printable.top() - 18.0 * 96 / 25.4) <= 1
                && std::abs(printable.width() - 174.0 * 96 / 25.4) <= 2,
            "preview and exports must use 18 mm A4 margins");
    QFont base(QStringLiteral("Noto Sans CJK JP"));
    base.setPixelSize(16);
    const auto lines = layoutAnnotation({{QStringLiteral("あ"), {}, false}}, printable.width());
    require(std::abs(lines.first().first().width
                     - QFontMetricsF(base).horizontalAdvance(QStringLiteral("あ"))) < 0.01,
            "annotated base text must use 16 pixels, equivalent to 12 points at print resolution");
}

void checkRomaji()
{
    const QMap<QString, QString> examples{
        {QStringLiteral("がっこう"), "gakkou"}, {QStringLiteral("まっちゃ"), "matcha"},
        {QStringLiteral("しんよう"), "shin'you"}, {QStringLiteral("こんにちは"), "konnichiha"},
        {QStringLiteral("スーパー"), "suupaa"}, {QStringLiteral("コンピューター"), "konpyuutaa"},
        {QStringLiteral("ティッシュ"), "tisshu"}, {QStringLiteral("ヴァイオリン"), "vaiorin"},
        {QStringLiteral("きょう"), "kyou"}, {QStringLiteral("ぢづ"), "jizu"},
        {QStringLiteral("ABC 123、🙂"), QStringLiteral("ABC 123、🙂")}
    };
    for (auto it = examples.cbegin(); it != examples.cend(); ++it) {
        const QString actual = toRomaji(it.key());
        if (actual != it.value())
            std::cerr << it.key().toStdString() << ": " << actual.toStdString() << '\n';
        require(actual == it.value(), "romaji must handle digraphs, gemination, n-apostrophes and long vowels");
        QString source;
        for (const auto &unit : romajiUnits(it.key()))
            source += unit.text;
        require(source == it.key(), "romaji units must preserve every original character");
    }
    require(romajiUnits(QStringLiteral("スーパー")).size() == 2,
            "long vowel marks must stay with their preceding kana when wrapping");
    const auto particles = annotate(QStringLiteral("私は学校へ本を持つ。"));
    QString roman;
    for (const auto &segment : particles)
        roman += toRomaji(!segment.reading.isEmpty() ? segment.reading
                         : !segment.pronunciation.isEmpty() ? segment.pronunciation : segment.text);
    require(roman == QStringLiteral("watashiwagakkouehonomotsu。"),
            "particles ha, he and wo must use their contextual pronunciations");
}

void checkOptionsAndPixels()
{
    const QVector<Segment> sample{
        {QStringLiteral("私"), QStringLiteral("わたし"), false},
        {QStringLiteral("は"), {}, false, QStringLiteral("わ")},
        {QStringLiteral("スーパー"), {}, false},
        {QStringLiteral("𠮷"), {}, true}
    };
    for (const AnnotationOptions options : {AnnotationOptions{}, {true, false}, {false, true},
                                            {true, true}, {true, true, 1.3}}) {
        const auto lines = layoutAnnotation(sample, 640, options);
        require(lines.size() == 1, "short layered example must fit one line");
        require(lines.first().first().romaji == (options.romaji ? "watashi" : ""),
                "kanji romaji checkbox must control only its own layer");
        require(lines.first().first().fullRomaji == (options.fullRomaji ? "watashi" : ""),
                "full romaji checkbox must control only its own layer");
        require(lines.first().at(1).romaji.isEmpty(), "kanji-only romaji must not annotate kana");
        require(lines.first().at(1).fullRomaji == (options.fullRomaji ? "wa" : ""),
                "full romaji must annotate the kana particle");
        require(lines.first().last().fullRomaji == (options.fullRomaji ? "?" : ""),
                "unknown kanji must remain explicitly unknown");
        const QImage rendered = renderAnnotationPage(lines, 0, lines.size(), 640, options);
        require(hasInk(rendered, QColor(35, 38, 42)) && hasInk(rendered, QColor(74, 79, 87)),
                "original text and kana must always remain visible");
        require(hasInk(rendered, QColor(45, 97, 82)) == options.romaji,
                "kanji romaji must add a visible independent row");
        require(hasInk(rendered, QColor(49, 91, 126)) == options.fullRomaji,
                "full romaji must add a visible independent row");

        AnnotationView view;
        QVariantList values;
        for (const auto &segment : sample)
            values.append(QVariantMap{{"text", segment.text}, {"reading", segment.reading},
                                      {"unknown", segment.unknown}, {"pronunciation", segment.pronunciation}});
        view.setWidth(640);
        view.setSegments(values);
        view.setRomaji(options.romaji);
        view.setFullRomaji(options.fullRomaji);
        view.setFontScale(options.fontScale);
        view.setHeight(view.implicitHeight());
        auto paintPreview = [&] {
            QImage image(rendered.size(), QImage::Format_RGB32);
            image.setDevicePixelRatio(3);
            image.fill(view.paperColor());
            QPainter painter(&image);
            view.paint(&painter);
            painter.end();
            return image;
        };
        const QImage preview = paintPreview();
        require(preview == rendered, "document page must be pixel-identical to the annotation preview");
        const qreal height = view.implicitHeight();
        const int pageCount = view.pageCount();
        view.setDarkMode(true);
        const QImage dark = paintPreview();
        require(dark.pixelColor(0, 0) == QColor("#1b2420") && hasInk(dark, QColor(Qt::white))
                    && view.textColor() == QColor(Qt::white),
                "dark annotation paper must have a dark background and white glyphs");
        for (int y = 0; y < preview.height(); ++y) {
            for (int x = 0; x < preview.width(); ++x) {
                const QColor color = preview.pixelColor(x, y);
                if (color == QColor(35, 38, 42) || color == QColor(74, 79, 87)
                    || color == QColor(45, 97, 82) || color == QColor(49, 91, 126)
                    || color == QColor(140, 140, 140))
                    require(dark.pixelColor(x, y) == QColor(Qt::white),
                            "every opaque body, kana, romaji, and unknown-marker pixel must become white in place");
            }
        }
        const QRect lightInk = inkBounds(preview, QColor(35, 38, 42))
            .united(inkBounds(preview, QColor(74, 79, 87)))
            .united(inkBounds(preview, QColor(45, 97, 82)))
            .united(inkBounds(preview, QColor(49, 91, 126)))
            .united(inkBounds(preview, QColor(140, 140, 140)));
        const QRect darkInk = inkBounds(dark, QColor(Qt::white));
        require(view.implicitHeight() == height && view.pageCount() == pageCount
                    && std::abs(lightInk.left() - darkInk.left()) <= 1
                    && std::abs(lightInk.top() - darkInk.top()) <= 1
                    && std::abs(lightInk.right() - darkInk.right()) <= 1
                    && std::abs(lightInk.bottom() - darkInk.bottom()) <= 1,
                "screen color changes must preserve glyph positions, line height, and page count");
        require(renderAnnotationPage(lines, 0, lines.size(), 640, options) == rendered,
                "dark screen appearance must not affect export image colors");
        view.setDarkMode(false);
        require(paintPreview() == preview, "light mode must restore every preview pixel");
    }
    require(annotationLineHeight({true, true}) - annotationLineHeight({true, false})
                == annotationLineHeight({true, false}) - annotationLineHeight(),
            "each checked option must add exactly one row of height");
    const auto wrapped = layoutAnnotation(annotate(QStringLiteral("きゃっきゃスーパーしんよう")), 95, {false, true});
    QString reconstructed;
    QString roman;
    for (const auto &line : wrapped) {
        for (const auto &placement : line) {
            reconstructed += placement.segment.text;
            roman += placement.fullRomaji;
            require(placement.x + placement.width <= 83, "full-text romaji must wrap inside the preview");
        }
    }
    require(reconstructed == QStringLiteral("きゃっきゃスーパーしんよう"),
            "wrapping full-text romaji must preserve original text");
    require(roman == "kyakkyasuupaashin'you", "wrapping must not split digraphs or lose long vowels");
}

void checkRomajiSpacing()
{
    const QVector<Segment> sample{
        {QStringLiteral("学校"), QStringLiteral("がっこう"), false},
        {QStringLiteral("新聞"), QStringLiteral("しんぶん"), false},
        {QStringLiteral("私"), QStringLiteral("わたし"), false},
        {QStringLiteral("きゃっきゃスーパーしんよう"), {}, false}
    };
    for (const AnnotationOptions options : {AnnotationOptions{true, false}, {false, true},
                                            {true, true}, {true, true, 1.3}}) {
        QFont reading(QStringLiteral("Noto Sans CJK JP"));
        reading.setPixelSize(qRound(10 * options.fontScale));
        const QFontMetricsF metrics(reading);
        for (const qreal width : {qreal(180), qreal(640)}) {
            const auto lines = layoutAnnotation(sample, width, options);
            const QImage rendered = renderAnnotationPage(lines, 0, lines.size(), width, options);
            QString original;
            for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
                const auto &line = lines.at(lineIndex);
                for (const auto &placement : line) {
                    original += placement.segment.text;
                    require(placement.x + placement.width <= width - annotationContentPadding() + 0.01,
                            "spaced romaji must wrap inside the printable area");
                }
                for (const QColor &color : {QColor(45, 97, 82), QColor(49, 91, 126)}) {
                    qreal previousRight = -1;
                    int previousInkRight = -1;
                    for (const auto &placement : line) {
                        const QString roman = color == QColor(45, 97, 82)
                            ? placement.romaji : placement.fullRomaji;
                        if (roman.isEmpty())
                            continue;
                        const qreal textWidth = metrics.horizontalAdvance(roman) * placement.textScale;
                        const qreal left = placement.x + (placement.width - textWidth) / 2;
                        if (previousRight >= 0)
                            require(left - previousRight >= 12 * options.fontScale - 0.01,
                                    "adjacent romaji labels must have at least 12 scaled pixels of separation");
                        previousRight = left + textWidth;
                        const int top = qRound((annotationContentPadding()
                            + lineIndex * annotationLineHeight(options)) * 3);
                        const QRect region(qRound(placement.x * 3), top,
                                           int(std::floor(placement.width * 3)),
                                           int(annotationLineHeight(options) * 3));
                        const QRect ink = inkBounds(rendered.copy(region), color);
                        require(!ink.isEmpty(), "each romaji label must render visible ink");
                        const int inkLeft = region.left() + ink.left();
                        const int inkRight = region.left() + ink.right();
                        if (previousInkRight >= 0)
                            require(inkLeft - previousInkRight >= qRound(10 * options.fontScale * 3),
                                    "rendered romaji labels must remain visually separated");
                        require(std::abs((inkLeft + inkRight) / 6.0
                                         - (placement.x + placement.width / 2)) <= 2 * options.fontScale,
                                "each rendered romaji label must stay centered over its original text");
                        previousInkRight = inkRight;
                    }
                }
            }
            QString expected;
            for (const auto &segment : sample)
                expected += segment.text;
            require(original == expected, "adding romaji spacing must preserve all original characters");
        }
    }
}

void checkDocuments(const QString &directory)
{
    QString text;
    for (int i = 0; i < 36; ++i)
        text += QStringLiteral("私の歌は、きょうもスーパーへ。\n");
    text += QStringLiteral("最後");
    const auto segments = annotate(text);
    const AnnotationOptions options{true, true};
    QString error;
    const qreal width = annotationPrintableRect().width();
    const auto pages = createAnnotationPages(segments, width, options, &error);
    const auto lines = layoutAnnotation(segments, width, options);
    const int perPage = annotationLinesPerPage(width, options);
    require(pages.size() == (lines.size() + perPage - 1) / perPage && pages.size() > 1,
            "every annotation line must appear on exactly one document page");
    QVariantList values;
    for (const auto &segment : segments)
        values.append(QVariantMap{{"text", segment.text}, {"reading", segment.reading},
                                  {"unknown", segment.unknown}, {"pronunciation", segment.pronunciation}});
    AnnotationView document;
    document.setWidth(width);
    document.setSegments(values);
    document.setRomaji(options.romaji);
    document.setFullRomaji(options.fullRomaji);
    require(document.pageCount() == pages.size(), "A4 preview and document must have identical page counts");
    AnnotationPageView paper;
    paper.setDocument(&document);
    for (int page = 0; page < pages.size(); ++page) {
        QImage decoded = QImage::fromData(pages.at(page).png).convertToFormat(QImage::Format_RGB32);
        decoded.setDevicePixelRatio(3);
        require(decoded == renderAnnotationPage(lines, page * perPage, perPage, width, options),
                "PNG serialization must preserve the shared renderer's pixels on every page");
        require(pages.at(page).size.height() < 261 * 96 / 25.4, "page content must fit A4 print height");
        QImage sheet(annotationPaperSize() * 3, QImage::Format_RGB32);
        sheet.setDevicePixelRatio(3);
        sheet.fill(Qt::white);
        QPainter painter(&sheet);
        painter.setRenderHint(QPainter::TextAntialiasing);
        paper.setPageIndex(page);
        paper.paint(&painter);
        painter.end();
        const QPoint topLeft = annotationPrintableRect().topLeft() * 3;
        const QImage previewContent = sheet.copy(QRect(topLeft, decoded.size()));
        require(previewContent == decoded,
                "each A4 preview page must match its exported DOCX/clipboard image pixel for pixel");
    }
    document.setDarkMode(true);
    QImage darkSheet(annotationPaperSize() * 3, QImage::Format_RGB32);
    darkSheet.setDevicePixelRatio(3);
    {
        QPainter painter(&darkSheet);
        paper.paint(&painter);
    }
    require(darkSheet.pixelColor(10, 10) == QColor("#1b2420") && hasInk(darkSheet, QColor(Qt::white)),
            "paginated screen pages must use the document's dark appearance");
    const auto darkExport = createAnnotationPages(segments, width, options, &error);
    require(darkExport.size() == pages.size(), "dark appearance must not change exported page counts");
    for (int page = 0; page < pages.size(); ++page)
        require(darkExport.at(page).png == pages.at(page).png,
                "DOCX and clipboard pages must remain white and pixel-identical while the preview is dark");
    const QString path = directory + "/annotated.docx";
    require(exportAnnotatedDocx(path, pages, &error), "multi-page DOCX must export");
    const QString unicodeDirectory = directory + QStringLiteral("/日语文稿");
    require(QDir().mkpath(unicodeDirectory)
                && exportAnnotatedDocx(unicodeDirectory + QStringLiteral("/唱日语.docx"), pages, &error),
            "DOCX must export inside Unicode directories on every platform");
    unzFile archive = unzOpen64(QFile::encodeName(path).constData());
    require(archive != nullptr, "DOCX must be a readable ZIP archive");
    for (const auto &name : {QByteArray("[Content_Types].xml"), QByteArray("_rels/.rels"),
                             QByteArray("word/document.xml"), QByteArray("word/_rels/document.xml.rels")}) {
        QXmlStreamReader xml(archiveEntry(archive, name));
        int images = 0;
        int pageBreaks = 0;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("blip"))
                ++images;
            if (xml.isStartElement() && xml.name() == QLatin1String("pageBreakBefore"))
                ++pageBreaks;
        }
        require(!xml.hasError(), "every DOCX XML part must be well formed");
        if (name == "word/document.xml")
            require(images == pages.size() && pageBreaks == pages.size() - 1,
                    "DOCX must include every page with explicit page breaks");
    }
    for (int page = 0; page < pages.size(); ++page)
        require(archiveEntry(archive, "word/media/page" + QByteArray::number(page + 1) + ".png")
                    == pages.at(page).png, "DOCX must embed the exact preview images, not reflowed text");
    require(unzClose(archive) == UNZ_OK, "DOCX archive must close cleanly");

    const auto mime = annotationClipboard(pages);
    require(mime->hasHtml() && mime->hasFormat("text/rtf"), "clipboard must offer HTML and RTF");
    const QString html = mime->html();
    const QByteArray rtf = mime->data("text/rtf");
    require(html.count("data:image/png;base64,") == pages.size()
                && rtf.count("\\pngblip") == pages.size(), "both clipboard formats must include all pages");
    require(rtf.count("\\page\n") == pages.size() - 1, "clipboard RTF must preserve page breaks");
    for (const auto &page : pages) {
        require(html.contains(QString::fromLatin1(page.png.toBase64()))
                    && rtf.contains(page.png.toHex()), "clipboard must embed the exact preview PNG bytes");
    }
    require(exportAnnotatedPdf(directory + "/annotated.pdf", segments, &error, options, width),
            "layered PDF must export with the preview width");
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setPageLayout(annotationPageLayout());
    printer.setResolution(96);
    printer.setOutputFileName(directory + "/printed.pdf");
    const bool printed = printAnnotatedDocument(printer, segments, &error, options, width);
    if (!printed) std::cerr << "Print error: " << error.toStdString() << '\n';
    require(printed, "system printer renderer must finish successfully");
    const int printedPages = pdfPageCount(printer.outputFileName());
    if (printedPages != pages.size())
        std::cerr << "Printed pages: " << printedPages << ", preview pages: " << pages.size()
                  << ", copies: " << printer.copyCount() << '\n';
    require(printedPages == pages.size(),
            "system printer renderer must preserve every A4 page");
    printer.setResolution(300);
    printer.setOutputFileName(directory + "/printed-300dpi.pdf");
    require(printAnnotatedDocument(printer, segments, &error, options, width)
                && pdfPageCount(printer.outputFileName()) == pages.size(),
            "printer resolution must not change annotation pagination");
    printer.setPrintRange(QPrinter::PageRange);
    printer.setFromTo(2, 2);
    printer.setOutputFileName(directory + "/printed-range.pdf");
    require(printAnnotatedDocument(printer, segments, &error, options, width)
                && pdfPageCount(printer.outputFileName()) == 1,
            "printer page ranges must output only the requested pages");
    printer.setFromTo(1, 2);
    printer.setPageOrder(QPrinter::LastPageFirst);
    printer.setCopyCount(2);
    printer.setCollateCopies(true);
    printer.setOutputFileName(directory + "/printed-copies.pdf");
    require(printAnnotatedDocument(printer, segments, &error, options, width)
                && pdfPageCount(printer.outputFileName()) == 4,
            "printer renderer must honor reverse order and collated copies");
    printer.setCollateCopies(false);
    printer.setOutputFileName(directory + "/printed-uncollated.pdf");
    require(printAnnotatedDocument(printer, segments, &error, options, width)
                && pdfPageCount(printer.outputFileName()) == 4,
            "printer renderer must also support uncollated copies");
    printer.setFromTo(pages.size() + 1, pages.size() + 2);
    error.clear();
    require(!printAnnotatedDocument(printer, segments, &error, options, width) && !error.isEmpty(),
            "invalid printer page ranges must fail explicitly");
    error.clear();
    require(!printAnnotatedDocument(printer, {}, &error) && !error.isEmpty(),
            "empty annotations must never create a print job");
    error.clear();
    require(!printAnnotatedDocument(printer, segments, &error, options,
                                    std::numeric_limits<qreal>::infinity()) && !error.isEmpty(),
            "invalid print geometry must fail explicitly");
    printer.setPrintRange(QPrinter::AllPages);
    printer.setOutputFileName(directory + "/missing/print.pdf");
    error.clear();
    require(!printAnnotatedDocument(printer, segments, &error, options, width) && !error.isEmpty(),
            "printer output failures must be reported instead of appearing successful");
    const QString pdftoppm = QStandardPaths::findExecutable("pdftoppm");
    if (!pdftoppm.isEmpty()) {
        for (const QString &name : {QStringLiteral("printed"), QStringLiteral("printed-300dpi"),
                                    QStringLiteral("printed-range"), QStringLiteral("printed-copies"),
                                    QStringLiteral("printed-uncollated")}) {
            QProcess printRender;
            const QString outputPage = name == "printed-copies" || name == "printed-uncollated" ? "2" : "1";
            printRender.start(pdftoppm, {"-r", "288", "-f", outputPage, "-singlefile", "-png",
                                         directory + "/" + name + ".pdf", directory + "/" + name});
            require(printRender.waitForFinished() && printRender.exitCode() == 0,
                    "printed document must render for comparison with the A4 preview");
            const QImage printed = QImage(directory + "/" + name + ".png").convertToFormat(QImage::Format_RGB32);
            const int expectedPage = name == "printed-range" || name == "printed-uncollated" ? 1 : 0;
            const QImage expected = QImage::fromData(pages.at(expectedPage).png).convertToFormat(QImage::Format_RGB32);
            for (const QColor &color : {QColor(35, 38, 42), QColor(74, 79, 87),
                                        QColor(45, 97, 82), QColor(49, 91, 126)}) {
                const QRect actual = inkBounds(printed, color);
                const QRect preview = inkBounds(expected, color).translated(annotationPrintableRect().topLeft() * 3);
                require(!actual.isEmpty() && !preview.isEmpty()
                            && std::abs(actual.left() - preview.left()) <= 5
                            && std::abs(actual.top() - preview.top()) <= 5
                            && std::abs(actual.right() - preview.right()) <= 5
                            && std::abs(actual.bottom() - preview.bottom()) <= 5,
                        "physical print size, page selection, and all annotation layers must match the A4 preview");
            }
        }
        QProcess render;
        render.start(pdftoppm, {"-r", "288", "-f", "1", "-singlefile", "-png",
                                directory + "/annotated.pdf", directory + "/pdf-page"});
        require(render.waitForFinished() && render.exitCode() == 0,
                "A4 PDF must render for comparison with the preview");
        const QImage pdf = QImage(directory + "/pdf-page.png").convertToFormat(QImage::Format_RGB32);
        const QImage docx = QImage::fromData(pages.first().png).convertToFormat(QImage::Format_RGB32);
        require(!pdf.isNull() && !docx.isNull(), "rendered PDF and DOCX page must be readable");
        const QPoint margin = annotationPrintableRect().topLeft() * 3;
        for (const QColor &color : {QColor(35, 38, 42), QColor(74, 79, 87),
                                    QColor(45, 97, 82), QColor(49, 91, 126)}) {
            const QRect expected = inkBounds(docx, color).translated(margin);
            const QRect actual = inkBounds(pdf, color);
            require(!actual.isEmpty() && !expected.isEmpty()
                        && std::abs(actual.left() - expected.left()) <= 5
                        && std::abs(actual.top() - expected.top()) <= 5
                        && std::abs(actual.right() - expected.right()) <= 5
                        && std::abs(actual.bottom() - expected.bottom()) <= 5,
                    "PDF and DOCX must position every text layer identically on the A4 page");
        }
    }
    const QString pdfinfo = QStandardPaths::findExecutable("pdfinfo");
    if (!pdfinfo.isEmpty()) {
        QProcess inspect;
        inspect.start(pdfinfo, {directory + "/annotated.pdf"});
        require(inspect.waitForFinished() && inspect.exitCode() == 0, "layered PDF must be readable");
        const auto count = QRegularExpression("Pages:\\s+(\\d+)")
            .match(QString::fromUtf8(inspect.readAllStandardOutput()));
        require(count.hasMatch() && count.captured(1).toInt() == pages.size(),
                "PDF, DOCX and clipboard must have identical page counts");
    }
    require(!exportAnnotatedDocx(directory + "/missing/output.docx", pages, &error) && !error.isEmpty(),
            "invalid DOCX destination must fail explicitly");
    error.clear();
    require(createAnnotationPages(segments, 0, options, &error).isEmpty() && !error.isEmpty(),
            "invalid document width must fail explicitly");

    const QString artifacts = qEnvironmentVariable("SING_JAPANESE_DOCUMENTS");
    if (!artifacts.isEmpty()) {
        require(QDir().mkpath(artifacts), "document check output directory must be available");
        require(exportAnnotatedDocx(artifacts + "/annotated.docx", pages, &error), "save inspection DOCX");
        require(exportAnnotatedPdf(artifacts + "/annotated.pdf", segments, &error, options, width),
                "save inspection PDF");
        QFile rtfFile(artifacts + "/clipboard.rtf");
        require(rtfFile.open(QIODevice::WriteOnly) && rtfFile.write(rtf) == rtf.size(), "save inspection RTF");
        QFile htmlFile(artifacts + "/clipboard.html");
        require(htmlFile.open(QIODevice::WriteOnly) && htmlFile.write(html.toUtf8()) == html.toUtf8().size(),
                "save inspection HTML");
    }
}
}

int main(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication application(argc, argv);
    require(loadBundledFont(), "bundled Japanese font must load");
    QTemporaryDir directory;
    require(directory.isValid(), "temporary document directory must be available");
    checkPaperGeometry();
    checkRomaji();
    checkOptionsAndPixels();
    checkRomajiSpacing();
    checkDocuments(directory.path());
    std::cout << "Romaji, pixel identity, DOCX/PDF/clipboard and printer DPI, page range and copy checks passed.\n";
    return 0;
}
