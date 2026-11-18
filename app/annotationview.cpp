#include "annotationview.h"
#include "romaji.h"

#include <QFontMetricsF>
#include <QDebug>
#include <QPainter>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QPrinter>
#include <QSaveFile>

#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kPadding = 12.0;
constexpr qreal kReadingGap = 4.0;
constexpr qreal kRomajiGap = 12.0;
constexpr qreal kLineGap = 12.0;
constexpr int kPrintResolution = 96;

QColor annotationTextColor(bool darkMode)
{
    return darkMode ? QColor(Qt::white) : QColor(35, 38, 42);
}

QVector<QString> characters(const QString &text)
{
    QVector<QString> result;
    for (uint codePoint : text.toUcs4())
        result.push_back(QString::fromUcs4(&codePoint, 1));
    return result;
}

QFont baseFont(AnnotationOptions options)
{
    QFont font(QStringLiteral("Noto Sans CJK JP"));
    font.setPixelSize(qRound(16 * options.fontScale));
    return font;
}

QFont readingFont(AnnotationOptions options)
{
    QFont font(QStringLiteral("Noto Sans CJK JP"));
    font.setPixelSize(qRound(10 * options.fontScale));
    return font;
}

void drawLine(QPainter &painter, const AnnotationLine &line, qreal top,
              const QFont &base, const QFont &reading, AnnotationOptions options, bool darkMode)
{
    const QFontMetricsF baseMetrics(base);
    const QFontMetricsF readingMetrics(reading);
    const qreal rubyHeight = readingMetrics.height();
    const qreal rowHeight = rubyHeight + kReadingGap;
    const qreal kanaTop = top + (int(options.romaji) + int(options.fullRomaji)) * rowHeight;
    const qreal baseline = kanaTop + rowHeight + baseMetrics.ascent();
    auto drawCentered = [&](const QString &text, const AnnotationPlacement &placement,
                            qreal y, qreal textWidth) {
        painter.save();
        painter.translate(placement.x + (placement.width - textWidth * placement.textScale) / 2, y);
        painter.scale(placement.textScale, placement.textScale);
        painter.drawText(QPointF(0, 0), text);
        painter.restore();
    };
    auto drawReading = [&](const QString &text, const AnnotationPlacement &placement,
                           qreal y, const QColor &color) {
        painter.setFont(reading);
        painter.setPen(color);
        const qreal width = readingMetrics.horizontalAdvance(text);
        drawCentered(text, placement, y + readingMetrics.ascent(), width);
        painter.setFont(base);
        painter.setPen(annotationTextColor(darkMode));
    };
    painter.setFont(base);
    painter.setPen(annotationTextColor(darkMode));
    for (const AnnotationPlacement &placement : line) {
        const QString &text = placement.segment.text;
        if (text.isEmpty() || text == QLatin1String("\n"))
            continue;
        const qreal baseWidth = baseMetrics.horizontalAdvance(text);
        drawCentered(text, placement, baseline, baseWidth);
        if (options.fullRomaji)
            drawReading(placement.fullRomaji, placement, top, darkMode ? QColor(Qt::white) : QColor(49, 91, 126));
        if (options.romaji)
            drawReading(placement.romaji, placement, top + int(options.fullRomaji) * rowHeight,
                        darkMode ? QColor(Qt::white) : QColor(45, 97, 82));
        if (!placement.segment.reading.isEmpty()) {
            drawReading(placement.segment.reading, placement, kanaTop,
                        darkMode ? QColor(Qt::white) : QColor(74, 79, 87));
        } else if (placement.segment.unknown) {
            const qreal y = baseline + 2;
            painter.setPen(QPen(darkMode ? QColor(Qt::white) : QColor(140, 140, 140), 1, Qt::DotLine));
            painter.drawLine(QPointF(placement.x, y), QPointF(placement.x + placement.width, y));
            painter.setPen(annotationTextColor(darkMode));
        }
    }
}
} // namespace

QSize annotationPaperSize()
{
    return annotationPageLayout().fullRectPixels(kPrintResolution).size();
}

QRect annotationPrintableRect()
{
    return annotationPageLayout().paintRectPixels(kPrintResolution);
}

QPageLayout annotationPageLayout()
{
    return QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Portrait,
                       QMarginsF(18, 18, 18, 18), QPageLayout::Millimeter);
}

qreal annotationContentPadding()
{
    return kPadding;
}

QVector<AnnotationLine> layoutAnnotation(const QVector<Segment> &segments, qreal width,
                                          const QFont &font, const QFont &rubyFont,
                                          AnnotationOptions options)
{
    const QFontMetricsF baseMetrics(font);
    const QFontMetricsF rubyMetrics(rubyFont);
    const qreal available = std::max<qreal>(1, width - kPadding * 2);
    QVector<AnnotationLine> lines(1);
    qreal x = 0;

    auto newline = [&] {
        lines.push_back({});
        x = 0;
    };
    auto append = [&](const Segment &segment) {
        const qreal baseWidth = baseMetrics.horizontalAdvance(segment.text);
        const qreal rubyWidth = segment.reading.isEmpty()
                                    ? 0
                                    : rubyMetrics.horizontalAdvance(segment.reading);
        const QString romaji = options.romaji ? toRomaji(segment.reading) : QString();
        const QString fullRomaji = !options.fullRomaji ? QString()
            : segment.unknown ? QStringLiteral("?")
            : toRomaji(!segment.reading.isEmpty() ? segment.reading
                       : !segment.pronunciation.isEmpty() ? segment.pronunciation : segment.text);
        const qreal horizontalGap = !romaji.isEmpty() || !fullRomaji.isEmpty()
            ? kRomajiGap * options.fontScale
            : segment.reading.isEmpty() ? 0 : kReadingGap;
        const qreal itemWidth = std::max({baseWidth, rubyWidth,
                                         rubyMetrics.horizontalAdvance(romaji),
                                         rubyMetrics.horizontalAdvance(fullRomaji)})
            + horizontalGap;
        const qreal textScale = itemWidth > available ? available / itemWidth : 1;
        const qreal placedWidth = itemWidth * textScale;
        if (x > 0 && x + placedWidth > available)
            newline();
        lines.last().push_back({segment, kPadding + x, placedWidth, romaji, fullRomaji, textScale});
        x += placedWidth;
    };

    for (int index = 0; index < segments.size(); ++index) {
        const Segment &segment = segments.at(index);
        if (segment.text == QLatin1String("\n")) {
            newline();
            continue;
        }
        if (options.fullRomaji && segment.reading.isEmpty() && !segment.unknown
            && (segment.pronunciation.isEmpty() || segment.pronunciation == segment.text)) {
            QString text = segment.text;
            while (index + 1 < segments.size()) {
                const auto &next = segments.at(index + 1);
                if (!next.reading.isEmpty() || next.unknown || next.text == QLatin1String("\n")
                    || (!next.pronunciation.isEmpty() && next.pronunciation != next.text))
                    break;
                text += next.text;
                ++index;
            }
            for (const auto &unit : romajiUnits(text)) {
                if (unit.text == QLatin1String("\r"))
                    continue;
                append({unit.text, {}, false, unit.romaji});
            }
        } else if (!options.fullRomaji && segment.reading.isEmpty()
                   && !segment.unknown && segment.text.size() > 1) {
            for (const QString &character : characters(segment.text)) {
                if (character == QLatin1String("\r"))
                    continue;
                append({character, {}, segment.unknown});
            }
        } else {
            append(segment);
        }
    }
    return lines;
}

QVector<AnnotationLine> layoutAnnotation(const QVector<Segment> &segments, qreal width,
                                          AnnotationOptions options)
{
    return layoutAnnotation(segments, width, baseFont(options), readingFont(options), options);
}

qreal annotationLineHeight(AnnotationOptions options)
{
    return QFontMetricsF(baseFont(options)).height() + kLineGap
           + (1 + int(options.romaji) + int(options.fullRomaji))
                 * (QFontMetricsF(readingFont(options)).height() + kReadingGap);
}

int annotationLinesPerPage(qreal width, AnnotationOptions options)
{
    const QRect printable = annotationPrintableRect();
    const qreal scale = std::min<qreal>(1, printable.width() / std::max<qreal>(1, width));
    return std::max(1, int((printable.height() / scale - kPadding * 2 - 16)
                           / annotationLineHeight(options)));
}

QImage renderAnnotationPage(const QVector<AnnotationLine> &lines, int firstLine, int lineCount,
                            qreal width, AnnotationOptions options)
{
    constexpr int resolutionScale = 3;
    const int count = std::min(lineCount, lines.size() - firstLine);
    QImage image(int(std::ceil(width * resolutionScale)),
                 int(std::ceil((count * annotationLineHeight(options) + kPadding * 2) * resolutionScale)),
                 QImage::Format_RGB32);
    if (image.isNull())
        return image;
    image.setDevicePixelRatio(resolutionScale);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.translate(0, kPadding);
    drawAnnotationLines(painter, lines, firstLine, count, options);
    return image;
}

void drawAnnotationLines(QPainter &painter, const QVector<AnnotationLine> &lines,
                         int firstLine, int lineCount, AnnotationOptions options, bool darkMode)
{
    const QFont base = baseFont(options);
    const QFont ruby = readingFont(options);
    for (int index = 0; index < lineCount && firstLine + index < lines.size(); ++index)
        drawLine(painter, lines.at(firstLine + index), index * annotationLineHeight(options),
                 base, ruby, options, darkMode);
}

AnnotationView::AnnotationView(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    connect(this, &QQuickItem::widthChanged, this, &AnnotationView::updateHeight);
    setAntialiasing(true);
}

QColor AnnotationView::paperColor() const
{
    return darkMode_ ? QColor(27, 36, 32) : QColor(Qt::white);
}

QColor AnnotationView::textColor() const
{
    return annotationTextColor(darkMode_);
}

void AnnotationView::setDarkMode(bool value)
{
    if (darkMode_ == value) return;
    darkMode_ = value;
    emit appearanceChanged();
    update();
}

void AnnotationView::setSegments(const QVariantList &segments)
{
    values_ = segments;
    segments_.clear();
    for (const QVariant &value : segments) {
        const auto segment = value.toMap();
        segments_.append({segment.value("text").toString(), segment.value("reading").toString(),
                          segment.value("unknown").toBool(), segment.value("pronunciation").toString()});
    }
    updateHeight();
    emit segmentsChanged();
}

void AnnotationView::setRomaji(bool value)
{
    if (options_.romaji == value) return;
    options_.romaji = value;
    updateHeight();
    emit optionsChanged();
}

void AnnotationView::setFullRomaji(bool value)
{
    if (options_.fullRomaji == value) return;
    options_.fullRomaji = value;
    updateHeight();
    emit optionsChanged();
}

void AnnotationView::setFontScale(qreal value)
{
    if (!std::isfinite(value) || value <= 0) {
        qWarning() << "Invalid annotation font scale:" << value;
        return;
    }
    if (qFuzzyCompare(options_.fontScale, value)) return;
    options_.fontScale = value;
    updateHeight();
    emit optionsChanged();
}

void AnnotationView::updateHeight()
{
    lines_ = layoutAnnotation(segments_, width(), options_);
    const int linesPerPage = annotationLinesPerPage(width(), options_);
    const int pages = std::max(1, (lines_.size() + linesPerPage - 1) / linesPerPage);
    setImplicitHeight(std::max<qreal>(80,
        lines_.size() * annotationLineHeight(options_) + kPadding * 2));
    if (pageCount_ != pages) {
        pageCount_ = pages;
        emit pageCountChanged();
    }
    emit layoutChanged();
    update();
}

void AnnotationView::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), paperColor());
    painter->save();
    painter->translate(0, kPadding);
    drawAnnotationLines(*painter, lines_, 0, lines_.size(), options_, darkMode_);
    painter->restore();
}

void AnnotationView::paintPage(QPainter *painter, int index) const
{
    const QSize paper = annotationPaperSize();
    const QRect printable = annotationPrintableRect();
    painter->fillRect(QRect(QPoint(0, 0), paper), paperColor());
    painter->setPen(darkMode_ ? QColor(67, 87, 74) : QColor(210, 218, 210));
    painter->drawRect(QRectF(0.5, 0.5, paper.width() - 1, paper.height() - 1));
    if (index < 0 || index >= pageCount_)
        return;
    const int linesPerPage = annotationLinesPerPage(width(), options_);
    painter->save();
    painter->setClipRect(printable);
    painter->translate(printable.left(), printable.top() + kPadding);
    drawAnnotationLines(*painter, lines_, index * linesPerPage, linesPerPage, options_, darkMode_);
    painter->restore();
}

AnnotationPageView::AnnotationPageView(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

void AnnotationPageView::setDocument(AnnotationView *document)
{
    if (document_ == document) return;
    if (document_)
        disconnect(document_, nullptr, this, nullptr);
    document_ = document;
    if (document_) {
        connect(document_, &AnnotationView::layoutChanged, this, [this] { update(); });
        connect(document_, &AnnotationView::appearanceChanged, this, [this] { update(); });
        connect(document_, &QObject::destroyed, this, [this] {
            emit documentChanged();
            update();
        });
    }
    emit documentChanged();
    update();
}

void AnnotationPageView::setPageIndex(int index)
{
    if (pageIndex_ == index) return;
    pageIndex_ = index;
    emit pageIndexChanged();
    update();
}

void AnnotationPageView::paint(QPainter *painter)
{
    if (document_)
        document_->paintPage(painter, pageIndex_);
}

namespace {
bool paintDocument(QPagedPaintDevice &device, const QVector<AnnotationLine> &lines,
                   AnnotationOptions options, qreal width, const QVector<int> &pages, QString *error)
{
    QPainter painter(&device);
    if (!painter.isActive()) {
        if (error) *error = QStringLiteral("无法开始文档绘制。请检查输出设备与文件权限。");
        return false;
    }
    const QRect page = device.pageLayout().paintRectPixels(device.logicalDpiX());
    const qreal scale = std::min({device.logicalDpiX() / qreal(kPrintResolution),
                                 page.width() / width,
                                 page.height() / qreal(annotationPrintableRect().height())});
    const int linesPerPage = annotationLinesPerPage(width, options);
    for (int index = 0; index < pages.size(); ++index) {
        if (index > 0 && !device.newPage()) {
            if (error) *error = QStringLiteral("无法创建下一页。请检查打印机或输出文件。");
            return false;
        }
        const int first = pages.at(index) * linesPerPage;
        painter.save();
        painter.setClipRect(QRect(0, 0, page.width(), page.height()));
        painter.scale(scale, scale);
        painter.translate(0, kPadding);
        drawAnnotationLines(painter, lines, first,
                            std::min(linesPerPage, lines.size() - first), options);
        painter.restore();
    }
    if (!painter.end()) {
        if (error) *error = QStringLiteral("文档输出失败。请检查打印机状态或磁盘空间。");
        return false;
    }
    return true;
}
}

bool exportAnnotatedPdf(const QString &path, const QVector<Segment> &segments, QString *error,
                        AnnotationOptions options, qreal width)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("无法创建 PDF：%1").arg(file.errorString());
        return false;
    }
    QPdfWriter writer(&file);
    writer.setResolution(kPrintResolution);
    writer.setPageLayout(annotationPageLayout());
    if (width <= 0) width = annotationPrintableRect().width();
    const auto lines = layoutAnnotation(segments, width, options);
    const int perPage = annotationLinesPerPage(width, options);
    QVector<int> pages;
    for (int first = 0; first < lines.size(); first += perPage)
        pages.append(first / perPage);
    if (!paintDocument(writer, lines, options, width, pages, error))
        return false;
    if (!file.commit()) {
        if (error) *error = QStringLiteral("PDF 写入失败：%1").arg(file.errorString());
        return false;
    }
    return true;
}

bool printAnnotatedDocument(QPrinter &printer, const QVector<Segment> &segments, QString *error,
                            AnnotationOptions options, qreal width)
{
    if (width == 0) width = annotationPrintableRect().width();
    if (segments.isEmpty() || !std::isfinite(width) || width <= 24) {
        if (error) *error = QStringLiteral("没有可打印的注音稿，或排版宽度无效。");
        return false;
    }
    const auto lines = layoutAnnotation(segments, width, options);
    const int perPage = annotationLinesPerPage(width, options);
    const int pageCount = (lines.size() + perPage - 1) / perPage;
    const int first = printer.printRange() == QPrinter::PageRange ? printer.fromPage() : 1;
    const int last = printer.printRange() == QPrinter::PageRange ? printer.toPage() : pageCount;
    if (first < 1 || last < first || first > pageCount
        || (printer.printRange() != QPrinter::AllPages && printer.printRange() != QPrinter::PageRange)) {
        if (error) *error = QStringLiteral("无效的打印页码范围。");
        return false;
    }
    QVector<int> selected;
    for (int page = first - 1; page < std::min(last, pageCount); ++page)
        selected.append(page);
    if (printer.pageOrder() == QPrinter::LastPageFirst)
        std::reverse(selected.begin(), selected.end());
    const int copies = printer.supportsMultipleCopies() ? 1 : std::max(1, printer.copyCount());
    QVector<int> pages;
    if (printer.collateCopies()) {
        for (int copy = 0; copy < copies; ++copy)
            pages += selected;
    } else {
        for (int page : selected)
            for (int copy = 0; copy < copies; ++copy)
                pages.append(page);
    }
    if (!paintDocument(printer, lines, options, width, pages, error))
        return false;
    if (printer.printerState() == QPrinter::Error || printer.printerState() == QPrinter::Aborted) {
        if (error) *error = QStringLiteral("打印任务未完成。请检查打印机状态后重试。");
        return false;
    }
    return true;
}
