#include "backend.h"
#include "annotationview.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
struct KanaRow {
    const char *name;
    const char *hiragana;
    const char *ids;
    int group;
};
const KanaRow rows[] = {
    {"元音", "あいうえお", "a i u e o", 1},
    {"K 行", "かきくけこ", "ka ki ku ke ko", 1},
    {"S 行", "さしすせそ", "sa shi su se so", 1},
    {"T 行", "たちつてと", "ta chi tsu te to", 1},
    {"N 行", "なにぬねの", "na ni nu ne no", 1},
    {"H 行", "はひふへほ", "ha hi fu he ho", 1},
    {"M 行", "まみむめも", "ma mi mu me mo", 1},
    {"Y 行", "やゆよ", "ya yu yo", 1},
    {"R 行", "らりるれろ", "ra ri ru re ro", 1},
    {"W 行", "わを", "wa wo", 1},
    {"拨音", "ん", "n", 1},
    {"G 行", "がぎぐげご", "ga gi gu ge go", 2},
    {"Z 行", "ざじずぜぞ", "za ji zu ze zo", 2},
    {"D 行", "だぢづでど", "da di du de do", 2},
    {"B 行", "ばびぶべぼ", "ba bi bu be bo", 2},
    {"P 行", "ぱぴぷぺぽ", "pa pi pu pe po", 3},
};

QString noteFor(const QString &id)
{
    if (id == "wo") return QStringLiteral("现代标准语通常读作 o，主要用作助词。");
    if (id == "ji" || id == "di") return QStringLiteral("じ／ジ 与 ぢ／ヂ 在现代标准语中通常同音。");
    if (id == "zu" || id == "du") return QStringLiteral("ず／ズ 与 づ／ヅ 在现代标准语中通常同音。");
    if (id == "n") return QStringLiteral("ん／ン 是一个音拍，实际发音受后续声音影响。");
    if (id == "u") return QStringLiteral("嘴唇通常不明显前突，注意听原音。");
    if (id == "fu") return QStringLiteral("辅音通常由双唇靠近发出，不等同于英语 f。");
    if (id == "ha") return QStringLiteral("单独读 ha，作主题助词时读 wa。");
    if (id == "he") return QStringLiteral("单独读 he，作方向助词时读 e。");
    if (id.startsWith('r')) return QStringLiteral("舌尖轻触上齿龈，不等同于英语 r 或 l。");
    return QStringLiteral("先听原音，再跟读一遍。隐藏罗马音，尝试从字形想起声音。");
}

QVariantMap entry(const KanaRow &row, int index, bool katakana)
{
    const QString id = QString::fromLatin1(row.ids).split(' ').at(index);
    const QChar hira = QString::fromUtf8(row.hiragana).at(index);
    const QChar kata(hira.unicode() + 0x60);
    return {{"id", id}, {"character", QString(katakana ? kata : hira)},
            {"counterpart", QString(katakana ? hira : kata)},
            {"romaji", id == "di" ? "ji" : id == "du" ? "zu" : id},
            {"note", noteFor(id)}};
}
}

Backend::Backend(QObject *parent) : QObject(parent)
{
    connect(&player_, &QMediaPlayer::stateChanged, this, &Backend::playingChanged);
    connect(&player_, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error), this,
            [this](QMediaPlayer::Error) {
        fail(QStringLiteral("录音播放失败：%1。请确认 Qt Multimedia 插件和 GStreamer 已安装。")
                 .arg(player_.errorString()));
    });
}

QStringList Backend::rowNames() const
{
    QStringList names{QStringLiteral("全部行")};
    for (const KanaRow &row : rows)
        if (groupIndex_ == 0 || row.group == groupIndex_)
            names.append(QString::fromUtf8(row.name));
    return names;
}

QVariantList Backend::kanaRows() const
{
    QVariantList result;
    const QString wanted = rowNames().at(rowIndex_);
    for (const KanaRow &row : rows) {
        if ((groupIndex_ != 0 && row.group != groupIndex_)
            || (rowIndex_ > 0 && QString::fromUtf8(row.name) != wanted))
            continue;
        QVariantList cells;
        const int count = QString::fromUtf8(row.hiragana).size();
        int next = 0;
        for (int column = 0; column < 5; ++column) {
            const bool occupied = count == 5 || (count == 3 && column % 2 == 0)
                || (count == 2 && (column == 0 || column == 4)) || (count == 1 && column == 0);
            cells.append(occupied ? entry(row, next++, katakana_) : QVariantMap{});
        }
        result.append(QVariantMap{{"name", QString::fromUtf8(row.name)}, {"cells", cells}});
    }
    return result;
}

QVariantMap Backend::selected() const
{
    for (const KanaRow &row : rows) {
        const int index = QString::fromLatin1(row.ids).split(' ').indexOf(selectedId_);
        if (index >= 0)
            return entry(row, index, katakana_);
    }
    return {};
}

void Backend::refreshSelection()
{
    const auto visible = kanaRows();
    for (const QVariant &row : visible)
        for (const QVariant &cell : row.toMap().value("cells").toList())
            if (cell.toMap().value("id").toString() == selectedId_)
                return;
    selectedId_ = visible.first().toMap().value("cells").toList().first().toMap().value("id").toString();
}

void Backend::setKatakana(bool value)
{
    if (katakana_ == value) return;
    stopAudio();
    katakana_ = value;
    emit kanaChanged();
}

void Backend::setGroupIndex(int value)
{
    if (value == groupIndex_) return;
    if (value < 0 || value > 3) { fail(QStringLiteral("无效的假名分组。")); return; }
    stopAudio();
    groupIndex_ = value;
    rowIndex_ = 0;
    refreshSelection();
    emit kanaChanged();
}

void Backend::setRowIndex(int value)
{
    if (value == rowIndex_) return;
    if (value < 0 || value >= rowNames().size()) { fail(QStringLiteral("无效的假名行。")); return; }
    stopAudio();
    rowIndex_ = value;
    refreshSelection();
    emit kanaChanged();
}

void Backend::selectKana(const QString &id)
{
    bool found = false;
    for (const QVariant &row : kanaRows())
        for (const QVariant &cell : row.toMap().value("cells").toList())
            found = found || cell.toMap().value("id").toString() == id;
    if (!found || id.isEmpty()) { fail(QStringLiteral("找不到所选假名。")); return; }
    selectedId_ = id;
    emit kanaChanged();
    replay();
}

void Backend::replay()
{
    stopAudio();
    clearError();
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (!QDir().mkpath(directory)) { fail(QStringLiteral("无法创建录音缓存目录。")); return; }
    const QString path = directory + "/" + selectedId_ + ".ogg";
    if (!QFileInfo::exists(path)) {
        QFile source(":/audio/" + selectedId_ + ".ogg");
        if (!source.open(QIODevice::ReadOnly)) { fail(QStringLiteral("找不到本地发音录音。")); return; }
        const QByteArray bytes = source.readAll();
        QSaveFile target(path);
        if (!target.open(QIODevice::WriteOnly) || target.write(bytes) != bytes.size() || !target.commit()) {
            fail(QStringLiteral("无法准备本地录音：%1").arg(target.errorString()));
            return;
        }
    }
    player_.setMedia(QUrl::fromLocalFile(path));
    player_.play();
}

void Backend::stopAudio() { player_.stop(); }
void Backend::clearError() { if (!error_.isEmpty()) { error_.clear(); emit errorChanged(); } }
void Backend::fail(const QString &message) { error_ = message; emit errorChanged(); }

void Backend::setInput(const QString &value)
{
    if (input_ == value) return;
    input_ = value;
    if (!annotated_.isEmpty())
        status_ = input_ == annotatedInput_ ? QStringLiteral("注音完成")
                                          : QStringLiteral("原文已修改，请重新注音。");
    emit lyricsChanged();
}

QVariantList Backend::segments() const
{
    QVariantList result;
    for (const Segment &segment : annotated_)
        result.append(QVariantMap{{"text", segment.text}, {"reading", segment.reading},
                                  {"unknown", segment.unknown}});
    return result;
}

void Backend::annotateText()
{
    clearError();
    if (input_.trimmed().isEmpty()) { fail(QStringLiteral("请先输入日语文本。")); return; }
    try {
        annotated_ = annotate(input_);
        annotatedInput_ = input_;
        int unknown = 0;
        for (const Segment &segment : annotated_) unknown += segment.unknown;
        status_ = unknown ? QStringLiteral("%1 个词未找到读音，已用点线标记。").arg(unknown)
                          : QStringLiteral("注音完成");
        emit resultChanged();
        emit lyricsChanged();
    } catch (const std::exception &error) {
        fail(QStringLiteral("注音失败：%1").arg(QString::fromUtf8(error.what())));
    }
}

void Backend::choosePdf()
{
    if (!canExport()) { fail(QStringLiteral("请先完成当前原文的注音。")); return; }
    QString path = QFileDialog::getSaveFileName(nullptr, QStringLiteral("导出 A4 PDF"),
                                               QStringLiteral("lyrics.pdf"), QStringLiteral("PDF 文件 (*.pdf)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(".pdf", Qt::CaseInsensitive)) path += ".pdf";
    savePdf(path);
}

bool Backend::savePdf(const QString &path)
{
    clearError();
    if (!canExport()) { fail(QStringLiteral("请先完成当前原文的注音。")); return false; }
    QString error;
    if (!exportAnnotatedPdf(path, annotated_, &error)) { fail(error); return false; }
    status_ = QStringLiteral("已导出：%1").arg(QFileInfo(path).fileName());
    emit lyricsChanged();
    return true;
}
