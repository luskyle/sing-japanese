#pragma once

#include <QString>
#include <QVector>

struct Segment {
    QString text;
    QString reading;
    bool unknown = false;
};

QVector<Segment> annotate(const QString &text);
QString toHiragana(const QString &text);
bool containsKanji(const QString &text);
