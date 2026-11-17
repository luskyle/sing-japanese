#pragma once

#include "annotationview.h"

#include <QByteArray>
#include <QMimeData>
#include <QSizeF>
#include <memory>

struct AnnotationPage {
    QByteArray png;
    QSizeF size;
};

QVector<AnnotationPage> createAnnotationPages(const QVector<Segment> &segments, qreal width,
                                             AnnotationOptions options, QString *error);
bool exportAnnotatedDocx(const QString &path, const QVector<AnnotationPage> &pages, QString *error);
std::unique_ptr<QMimeData> annotationClipboard(const QVector<AnnotationPage> &pages);
