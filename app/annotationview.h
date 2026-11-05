#pragma once

#include "annotator.h"

#include <QQuickPaintedItem>
#include <QVariantList>

class QPainter;

struct AnnotationPlacement {
    Segment segment;
    qreal x = 0;
    qreal width = 0;
};

using AnnotationLine = QVector<AnnotationPlacement>;

QVector<AnnotationLine> layoutAnnotation(const QVector<Segment> &segments, qreal width,
                                          const QFont &baseFont, const QFont &readingFont);
void drawAnnotationLines(QPainter &painter, const QVector<AnnotationLine> &lines,
                         int firstLine, int lineCount, qreal lineHeight);
bool exportAnnotatedPdf(const QString &path, const QVector<Segment> &segments, QString *error);

class AnnotationView : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantList segments READ segments WRITE setSegments NOTIFY segmentsChanged)
public:
    explicit AnnotationView(QQuickItem *parent = nullptr);
    QVariantList segments() const { return values_; }
    void setSegments(const QVariantList &segments);
    void paint(QPainter *painter) override;
signals:
    void segmentsChanged();

private:
    void updateHeight();
    QVariantList values_;
    QVector<Segment> segments_;
};
