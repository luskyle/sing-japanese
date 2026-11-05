#include "annotator.h"

#include <mecab.h>

#include <QByteArray>
#include <QRegularExpression>

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
    static std::unique_ptr<mecab_t, MeCabDeleter> instance(mecab_new2(""));
    if (!instance)
        throw std::runtime_error("无法初始化 MeCab。请安装 mecab-ipadic-utf8。");
    return instance.get();
}

QString readingFromFeature(const char *feature)
{
    const QList<QByteArray> fields = QByteArray(feature ? feature : "").split(',');
    if (fields.size() <= 7 || fields.at(7).isEmpty() || fields.at(7) == "*")
        return {};
    return toHiragana(QString::fromUtf8(fields.at(7)));
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
        result.push_back({text, reading, containsKanji(text) && reading.isEmpty()});
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
