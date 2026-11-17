#include "romaji.h"
#include "annotator.h"

#include <QHash>
#include <QStringList>

namespace {
const QHash<QString, QString> &syllables()
{
    static const QHash<QString, QString> table = [] {
        QHash<QString, QString> result;
        const QString kana = QStringLiteral(
            "あいうえおかきくけこさしすせそたちつてとなにぬねの"
            "はひふへほまみむめもやゆよらりるれろわゐゑをん"
            "がぎぐげござじずぜぞだぢづでどばびぶべぼぱぴぷぺぽ"
            "ぁぃぅぇぉゃゅょゎゔゕゖ");
        const QStringList latin = QStringLiteral(
            "a i u e o ka ki ku ke ko sa shi su se so ta chi tsu te to na ni nu ne no "
            "ha hi fu he ho ma mi mu me mo ya yu yo ra ri ru re ro wa wi we wo n "
            "ga gi gu ge go za ji zu ze zo da ji zu de do ba bi bu be bo pa pi pu pe po "
            "a i u e o ya yu yo wa vu ka ke").split(' ');
        for (int i = 0; i < kana.size(); ++i)
            result.insert(kana.mid(i, 1), latin.at(i));
        const QString stems = QStringLiteral("きぎしじちぢにひびぴみり");
        const QStringList prefixes = QStringLiteral("ky gy sh j ch j ny hy by py my ry").split(' ');
        for (int i = 0; i < stems.size(); ++i) {
            result.insert(stems.mid(i, 1) + QStringLiteral("ゃ"), prefixes.at(i) + "a");
            result.insert(stems.mid(i, 1) + QStringLiteral("ゅ"), prefixes.at(i) + "u");
            result.insert(stems.mid(i, 1) + QStringLiteral("ょ"), prefixes.at(i) + "o");
        }
        const QStringList foreign = QStringLiteral(
            "いぇ うぃ うぇ うぉ しぇ じぇ ちぇ つぁ つぃ つぇ つぉ "
            "てぃ でぃ とぅ どぅ てゅ でゅ ふぁ ふぃ ふぇ ふぉ ふゅ "
            "ゔぁ ゔぃ ゔぇ ゔぉ ゔゅ くぁ くぃ くぇ くぉ ぐぁ").split(' ');
        const QStringList foreignLatin = QStringLiteral(
            "ye wi we wo she je che tsa tsi tse tso "
            "ti di tu du tyu dyu fa fi fe fo fyu "
            "va vi ve vo vyu kwa kwi kwe kwo gwa").split(' ');
        for (int i = 0; i < foreign.size(); ++i)
            result.insert(foreign.at(i), foreignLatin.at(i));
        return result;
    }();
    return table;
}

QString syllableAt(const QString &kana, int offset, int *length)
{
    *length = 1;
    if (offset >= kana.size())
        return {};
    if (offset + 1 < kana.size() && syllables().contains(kana.mid(offset, 2))) {
        *length = 2;
        return syllables().value(kana.mid(offset, 2));
    }
    return syllables().value(kana.mid(offset, 1));
}
}

QVector<RomajiUnit> romajiUnits(const QString &text)
{
    const QString kana = toHiragana(text);
    QVector<RomajiUnit> result;
    QChar lastVowel;
    for (int offset = 0; offset < kana.size();) {
        int length = 1;
        QString roman = syllableAt(kana, offset, &length);
        const QChar character = kana.at(offset);
        if (character == QChar(0x3063)) {
            int nextLength = 1;
            const QString next = syllableAt(kana, offset + 1, &nextLength);
            if (!next.isEmpty() && !QStringLiteral("aeioun").contains(next.front())) {
                roman = (next.startsWith("ch") ? QStringLiteral("t") : next.left(1)) + next;
                length = 1 + nextLength;
            } else {
                roman = QStringLiteral("tsu");
            }
        } else if (character == QChar(0x3093)) {
            int nextLength = 1;
            const QString next = syllableAt(kana, offset + 1, &nextLength);
            if (!next.isEmpty() && QStringLiteral("aeiouy").contains(next.front()))
                roman = QStringLiteral("n'");
        } else if (character == QChar(0x30fc) && !lastVowel.isNull() && !result.isEmpty()) {
            result.last().text += character;
            result.last().romaji += lastVowel;
            ++offset;
            continue;
        }
        if (roman.isEmpty()) {
            if (character.isHighSurrogate() && offset + 1 < kana.size()
                && kana.at(offset + 1).isLowSurrogate())
                length = 2;
            roman = text.mid(offset, length);
            lastVowel = {};
        } else {
            lastVowel = QStringLiteral("aeiou").contains(roman.back()) ? roman.back() : QChar();
        }
        result.append({text.mid(offset, length), roman});
        offset += length;
    }
    return result;
}

QString toRomaji(const QString &text)
{
    QString result;
    for (const auto &unit : romajiUnits(text))
        result += unit.romaji;
    return result;
}
