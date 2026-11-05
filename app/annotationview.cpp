#include "annotationview.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>

#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kLineHeight = 42.0;
constexpr qreal kPadding = 12.0;

QVector<QString> characters(const QString &text)
{
    QVector<QString> result;
    for (uint codePoint : text.toUcs4())
        result.push_back(QString::fromUcs4(&codePoint, 1));
    return result;
}

QFont baseFont()
{
    QFont font(QStringLiteral("Noto Sans CJK JP"));
    font.setPointSize(13);
    return font;
}

QFont readingFont()
{
    QFont font(QStringLiteral("Noto Sans CJK JP"));
    font.setPointSize(8);
    return font;
}

void drawLine(QPainter &painter, const AnnotationLine &line, qreal top,
              const QFont &base, const QFont &reading)
{
    const QFontMetricsF baseMetrics(base);
    const QFontMetricsF readingMetrics(reading);
    const qreal rubyHeight = readingMetrics.height();
    const qreal baseline = top + rubyHeight + 4 + baseMetrics.ascent();
    painter.setFont(base);
    painter.setPen(QColor(35, 38, 42));
    for (const AnnotationPlacement &placement : line) {
        const QString &text = placement.segment.text;
        if (text.isEmpty() || text == QLatin1String("\n"))
            continue;
        const qreal baseWidth = baseMetrics.horizontalAdvance(text);
        const qreal baseX = placement.x + (placement.width - baseWidth) / 2;
        painter.drawText(QPointF(baseX, baseline), text);
        if (!placement.segment.reading.isEmpty()) {
            painter.setFont(reading);
            painter.setPen(QColor(74, 79, 87));
            const qreal readingWidth = readingMetrics.horizontalAdvance(placement.segment.reading);
            painter.drawText(QPointF(placement.x + (placement.width - readingWidth) / 2,
                                     top + readingMetrics.ascent()),
                             placement.segment.reading);
            painter.setFont(base);
            painter.setPen(QColor(35, 38, 42));
        } else if (placement.segment.unknown) {
            const qreal y = baseline + 2;
            painter.setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
            painter.drawLine(QPointF(placement.x, y), QPointF(placement.x + placement.width, y));
            painter.setPen(QColor(35, 38, 42));
        }
    }
}
} // namespace

QVector<AnnotationLine> layoutAnnotation(const QVector<Segment> &segments, qreal width,
                                          const QFont &font, const QFont &rubyFont)
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
        const qreal itemWidth = std::max(baseWidth, rubyWidth);
        if (x > 0 && x + itemWidth > available)
            newline();
        lines.last().push_back({segment, kPadding + x, itemWidth});
        x += itemWidth;
    };

    for (const Segment &segment : segments) {
        if (segment.text == QLatin1String("\n")) {
            newline();
            continue;
        }
        if (segment.reading.isEmpty() && !segment.unknown && segment.text.size() > 1) {
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

void drawAnnotationLines(QPainter &painter, const QVector<AnnotationLine> &lines,
                         int firstLine, int lineCount, qreal lineHeight)
{
    const QFont base = baseFont();
    const QFont ruby = readingFont();
    for (int index = 0; index < lineCount && firstLine + index < lines.size(); ++index)
        drawLine(painter, lines.at(firstLine + index), index * lineHeight, base, ruby);
}

AnnotationView::AnnotationView(QWidget *parent) : QWidget(parent)
{
    setMinimumWidth(300);
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    setAutoFillBackground(true);
    QPalette colors = palette();
    colors.setColor(QPalette::Window, Qt::white);
    setPalette(colors);
}

void AnnotationView::setSegments(const QVector<Segment> &segments)
{
    segments_ = segments;
    updateGeometry();
    update();
}

int AnnotationView::heightForWidth(int width) const
{
    return std::max(80, static_cast<int>(std::ceil(
        layoutAnnotation(segments_, width, baseFont(), readingFont()).size() * kLineHeight + kPadding * 2)));
}

QSize AnnotationView::sizeHint() const
{
    return {480, heightForWidth(width() > 0 ? width() : 480)};
}

void AnnotationView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);
    const auto lines = layoutAnnotation(segments_, width(), baseFont(), readingFont());
    painter.save();
    painter.translate(0, kPadding);
    painter.setClipRect(rect().adjusted(0, kPadding, 0, 0));
    drawAnnotationLines(painter, lines, 0, lines.size(), kLineHeight);
    painter.restore();
}

bool exportAnnotatedPdf(const QString &path, const QVector<Segment> &segments, QString *error)
{
    QPdfWriter writer(path);
    writer.setResolution(96);
    writer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Portrait,
                                     QMarginsF(18, 18, 18, 18), QPageLayout::Millimeter));
    QPainter painter(&writer);
    if (!painter.isActive()) {
        if (error)
            *error = QStringLiteral("无法创建 PDF 文件。");
        return false;
    }

    const QRect page = writer.pageLayout().paintRectPixels(writer.resolution());
    const auto lines = layoutAnnotation(segments, page.width(), baseFont(), readingFont());
    const int linesPerPage = std::max(1, static_cast<int>(page.height() / kLineHeight));
    for (int first = 0; first < lines.size(); first += linesPerPage) {
        if (first > 0 && !writer.newPage()) {
            if (error)
                *error = QStringLiteral("无法创建下一页。");
            return false;
        }
        painter.save();
        painter.translate(page.left(), page.top());
        painter.setClipRect(QRect(0, 0, page.width(), page.height()));
        drawAnnotationLines(painter, lines, first,
                            std::min(linesPerPage, lines.size() - first), kLineHeight);
        painter.restore();
    }
    painter.end();
    return true;
}
