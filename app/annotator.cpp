#include "annotator.h"

#include <mecab.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>

namespace {

struct MeCabDeleter {
    void operator()(mecab_t *tokenizer) const
    {
        if (tokenizer)
            mecab_destroy(tokenizer);
    }
};

mecab_t *tokenizer()
{
    static std::unique_ptr<mecab_t, MeCabDeleter> instance([] {
        QString resources = QCoreApplication::applicationDirPath() + "/resources";
#ifdef Q_OS_MACOS
        if (!QDir(resources).exists())
            resources = QCoreApplication::applicationDirPath() + "/../Resources";
#endif
        if (QDir(resources + "/ipadic").exists()) {
            QByteArray name("sing-japanese"), rcOption("-r"), dicOption("-d");
            QByteArray dictionary = QDir::cleanPath(resources + "/ipadic").toUtf8();
            QByteArray config = QDir::cleanPath(resources + "/mecabrc").toUtf8();
            char *args[] = {name.data(), rcOption.data(), config.data(), dicOption.data(), dictionary.data()};
            return mecab_new(5, args);
        }
        return mecab_new2("");
    }());
    if (!instance)
        throw std::runtime_error(QStringLiteral("无法初始化 MeCab：%1。请重新安装应用或安装 mecab-ipadic-utf8。")
            .arg(QString::fromUtf8(mecab_strerror(nullptr))).toStdString());
    return instance.get();
}

QString readingFromFeature(const char *feature)
{
    const QList<QByteArray> fields = QByteArray(feature ? feature : "").split(',');
    if (fields.size() <= 7 || fields.at(7).isEmpty() || fields.at(7) == "*")
        return {};
    return toHiragana(QString::fromUtf8(fields.at(7)));
}

QVector<Segment> alignReading(const QString &text, const QString &reading)
{
    const QVector<Segment> fallback{{text, reading, false}};
    if (reading.isEmpty())
        return fallback;

    QVector<QString> runs;
    QVector<bool> kanji;
    for (uint codePoint : text.toUcs4()) {
        const QString character = QString::fromUcs4(&codePoint, 1);
        const bool isKanji = containsKanji(character);
        if (runs.isEmpty() || kanji.last() != isKanji) {
            runs.append(character);
            kanji.append(isKanji);
        } else {
            runs.last() += character;
        }
    }
    if (runs.size() < 2)
        return fallback;

    // Kana in the surface form anchors the reading. Only split an unambiguous match.
    QVector<QVector<int>> matches(runs.size(), QVector<int>(reading.size() + 1, -1));
    std::function<int(int, int)> countMatches = [&](int run, int offset) -> int {
        if (run == runs.size())
            return offset == reading.size() ? 1 : 0;
        int &count = matches[run][offset];
        if (count >= 0)
            return count;
        count = 0;
        if (!kanji.at(run)) {
            const QString anchor = toHiragana(runs.at(run));
            if (reading.midRef(offset, anchor.size()) == anchor)
                count = countMatches(run + 1, offset + anchor.size());
        } else {
            for (int end = offset + 1; end <= reading.size() && count < 2; ++end)
                count = std::min(2, count + countMatches(run + 1, end));
        }
        return count;
    };
    if (countMatches(0, 0) != 1)
        return fallback;

    QVector<Segment> result;
    int offset = 0;
    for (int run = 0; run < runs.size(); ++run) {
        if (!kanji.at(run)) {
            result.push_back({runs.at(run), {}, false});
            offset += runs.at(run).size();
        } else {
            int end = offset + 1;
            while (countMatches(run + 1, end) == 0)
                ++end;
            result.push_back({runs.at(run), reading.mid(offset, end - offset), false});
            offset = end;
        }
    }
    return result;
}

void appendLine(const QString &line, QVector<Segment> &result)
{
    const QByteArray utf8 = line.toUtf8();
    if (utf8.isEmpty())
        return;
    const mecab_node_t *node = mecab_sparse_tonode(tokenizer(), utf8.constData());
    if (!node)
        throw std::runtime_error(QString::fromUtf8(mecab_strerror(tokenizer())).toStdString());

    int byteCursor = 0;
    while (node) {
        if (node->stat == MECAB_BOS_NODE || node->stat == MECAB_EOS_NODE || node->length == 0) {
            node = node->next;
            continue;
        }
        const QByteArray surface(node->surface, static_cast<int>(node->length));
        const int start = utf8.indexOf(surface, byteCursor);
        if (start < byteCursor)
            throw std::runtime_error("词典返回了无效的文字位置，已停止注音。");
        if (start > byteCursor)
            result.push_back({QString::fromUtf8(utf8.mid(byteCursor, start - byteCursor)), {}, false});

        const QString text = QString::fromUtf8(surface);
        const QString reading = containsKanji(text) ? readingFromFeature(node->feature) : QString();
        if (reading.isEmpty()) {
            QString pronunciation = text;
            const QList<QByteArray> fields = QByteArray(node->feature).split(',');
            if (fields.first() == "助詞") {
                if (text == QStringLiteral("は")) pronunciation = QStringLiteral("わ");
                if (text == QStringLiteral("へ")) pronunciation = QStringLiteral("え");
                if (text == QStringLiteral("を")) pronunciation = QStringLiteral("お");
            }
            result.push_back({text, {}, containsKanji(text), pronunciation});
        } else {
            result += alignReading(text, reading);
        }
        byteCursor = start + surface.size();
        node = node->next;
    }
    if (byteCursor < utf8.size())
        result.push_back({QString::fromUtf8(utf8.mid(byteCursor)), {}, false});
}

} // namespace

bool containsKanji(const QString &text)
{
    static const QRegularExpression kanji(
        QStringLiteral("[\\x{3400}-\\x{4DBF}\\x{4E00}-\\x{9FFF}\\x{F900}-\\x{FAFF}\\x{20000}-\\x{323AF}々〆]"));
    return kanji.match(text).hasMatch();
}

QString toHiragana(const QString &text)
{
    QString result;
    result.reserve(text.size());
    for (uint codePoint : text.toUcs4()) {
        if (codePoint >= 0x30A1 && codePoint <= 0x30F6)
            codePoint -= 0x60;
        result.append(QString::fromUcs4(&codePoint, 1));
    }
    return result;
}

QVector<Segment> annotate(const QString &text)
{
    if (text.toUcs4().size() > 20000)
        throw std::runtime_error("一次最多注音 20,000 个字符，请分段处理。");
    QVector<Segment> result;
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (int index = 0; index < lines.size(); ++index) {
        appendLine(lines.at(index), result);
        if (index + 1 < lines.size())
            result.push_back({QStringLiteral("\n"), {}, false});
    }
    return result;
}
