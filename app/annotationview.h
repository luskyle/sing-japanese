#pragma once

#include "annotator.h"

#include <QWidget>

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

class AnnotationView final : public QWidget
{
public:
    explicit AnnotationView(QWidget *parent = nullptr);
    void setSegments(const QVector<Segment> &segments);
    QSize sizeHint() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<Segment> segments_;
};
