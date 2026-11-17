#pragma once

#include <QString>
#include <QVector>

struct RomajiUnit {
    QString text;
    QString romaji;
};

QVector<RomajiUnit> romajiUnits(const QString &text);
QString toRomaji(const QString &text);
