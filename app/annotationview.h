#pragma once

#include "annotator.h"

#include <QQuickPaintedItem>
#include <QVariantList>
#include <QImage>
#include <QPointer>
#include <QPageLayout>
#include <QColor>

class QPainter;
class QPrinter;

struct AnnotationOptions {
    bool romaji = false;
    bool fullRomaji = false;
    qreal fontScale = 1;
};

struct AnnotationPlacement {
    Segment segment;
    qreal x = 0;
    qreal width = 0;
    QString romaji = {};
    QString fullRomaji = {};
    qreal textScale = 1;
};

using AnnotationLine = QVector<AnnotationPlacement>;

QSize annotationPaperSize();
QPageLayout annotationPageLayout();
QRect annotationPrintableRect();
qreal annotationContentPadding();
QVector<AnnotationLine> layoutAnnotation(const QVector<Segment> &segments, qreal width,
                                          const QFont &baseFont, const QFont &readingFont,
                                          AnnotationOptions options = {});
QVector<AnnotationLine> layoutAnnotation(const QVector<Segment> &segments, qreal width,
                                          AnnotationOptions options = {});
qreal annotationLineHeight(AnnotationOptions options = {});
int annotationLinesPerPage(qreal width, AnnotationOptions options = {});
QImage renderAnnotationPage(const QVector<AnnotationLine> &lines, int firstLine, int lineCount,
                            qreal width, AnnotationOptions options = {});
void drawAnnotationLines(QPainter &painter, const QVector<AnnotationLine> &lines,
                         int firstLine, int lineCount, AnnotationOptions options = {}, bool darkMode = false);
bool exportAnnotatedPdf(const QString &path, const QVector<Segment> &segments, QString *error,
                        AnnotationOptions options = {}, qreal width = 0);
bool printAnnotatedDocument(QPrinter &printer, const QVector<Segment> &segments, QString *error,
                            AnnotationOptions options = {}, qreal width = 0);

class AnnotationView : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantList segments READ segments WRITE setSegments NOTIFY segmentsChanged)
    Q_PROPERTY(bool romaji READ romaji WRITE setRomaji NOTIFY optionsChanged)
    Q_PROPERTY(bool fullRomaji READ fullRomaji WRITE setFullRomaji NOTIFY optionsChanged)
    Q_PROPERTY(qreal fontScale READ fontScale WRITE setFontScale NOTIFY optionsChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY appearanceChanged)
    Q_PROPERTY(QColor paperColor READ paperColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor NOTIFY appearanceChanged)
    Q_PROPERTY(qreal paperWidth READ paperWidth CONSTANT)
    Q_PROPERTY(qreal paperHeight READ paperHeight CONSTANT)
    Q_PROPERTY(qreal printableWidth READ printableWidth CONSTANT)
    Q_PROPERTY(qreal paperMarginLeft READ paperMarginLeft CONSTANT)
    Q_PROPERTY(qreal paperMarginTop READ paperMarginTop CONSTANT)
    Q_PROPERTY(qreal contentPadding READ contentPadding CONSTANT)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY pageCountChanged)
public:
    explicit AnnotationView(QQuickItem *parent = nullptr);
    QVariantList segments() const { return values_; }
    void setSegments(const QVariantList &segments);
    bool romaji() const { return options_.romaji; }
    bool fullRomaji() const { return options_.fullRomaji; }
    qreal fontScale() const { return options_.fontScale; }
    bool darkMode() const { return darkMode_; }
    QColor paperColor() const;
    QColor textColor() const;
    qreal paperWidth() const { return annotationPaperSize().width(); }
    qreal paperHeight() const { return annotationPaperSize().height(); }
    qreal printableWidth() const { return annotationPrintableRect().width(); }
    qreal paperMarginLeft() const { return annotationPrintableRect().left(); }
    qreal paperMarginTop() const { return annotationPrintableRect().top(); }
    qreal contentPadding() const { return annotationContentPadding(); }
    int pageCount() const { return pageCount_; }
    void setRomaji(bool value);
    void setFullRomaji(bool value);
    void setFontScale(qreal value);
    void setDarkMode(bool value);
    void paint(QPainter *painter) override;
    void paintPage(QPainter *painter, int index) const;
signals:
    void segmentsChanged();
    void optionsChanged();
    void appearanceChanged();
    void pageCountChanged();
    void layoutChanged();

private:
    void updateHeight();
    QVariantList values_;
    QVector<Segment> segments_;
    AnnotationOptions options_;
    bool darkMode_ = false;
    QVector<AnnotationLine> lines_;
    int pageCount_ = 1;
};

class AnnotationPageView : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(AnnotationView *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(int pageIndex READ pageIndex WRITE setPageIndex NOTIFY pageIndexChanged)
public:
    explicit AnnotationPageView(QQuickItem *parent = nullptr);
    AnnotationView *document() const { return document_; }
    int pageIndex() const { return pageIndex_; }
    void setDocument(AnnotationView *document);
    void setPageIndex(int index);
    void paint(QPainter *painter) override;

signals:
    void documentChanged();
    void pageIndexChanged();

private:
    QPointer<AnnotationView> document_;
    int pageIndex_ = 0;
};
