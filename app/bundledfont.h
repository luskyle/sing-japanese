#pragma once

#include <QFile>
#include <QFontDatabase>

inline bool loadBundledFont()
{
    const QString path = QStringLiteral(":/fonts/NotoSansCJKjp-Regular.otf");
    return !QFile::exists(path) || QFontDatabase::addApplicationFont(path) >= 0;
}
