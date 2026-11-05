#include "annotator.h"
#include "annotationview.h"
#include "mainwindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QMediaPlayer>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QRadioButton>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>

#include <iostream>
#include <cstdlib>

namespace {
void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "Test failed: " << message << '\n';
        std::exit(1);
    }
}
} // namespace

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    const QString input = QStringLiteral("  今日、学校へ行く。\r\n\nあいうえお  ");
    const QVector<Segment> segments = annotate(input);
    QString reconstructed;
    bool foundSchool = false;
    for (const Segment &segment : segments) {
        reconstructed += segment.text;
        if (segment.text == QStringLiteral("学校") && segment.reading == QStringLiteral("がっこう"))
            foundSchool = true;
    }
    require(reconstructed == input, "annotation must preserve every input character");
    require(foundSchool, "school must be read as がっこう");
    require(toHiragana(QStringLiteral("ガッコウ")) == QStringLiteral("がっこう"),
            "katakana readings must convert to hiragana");
    const QVector<Segment> unknown = annotate(QStringLiteral("𠮷"));
    require(unknown.size() == 1 && unknown.first().text == QStringLiteral("𠮷") && unknown.first().unknown,
            "unknown kanji must remain visible and be marked");

    bool rejectedLongInput = false;
    try {
        annotate(QString(20001, QChar(0x3042)));
    } catch (const std::exception &) {
        rejectedLongInput = true;
    }
    require(rejectedLongInput, "input over 20,000 characters must be rejected");

    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory must be created");
    QString longInput;
    for (int line = 0; line < 80; ++line)
        longInput += QStringLiteral("学校へ行く。\n");
    const QVector<Segment> longSegments = annotate(longInput);
    QString error;
    const QString pdfPath = directory.filePath(QStringLiteral("test.pdf"));
    require(exportAnnotatedPdf(pdfPath, longSegments, &error), "A4 PDF must export successfully");
    QFile pdf(directory.filePath(QStringLiteral("test.pdf")));
    require(pdf.open(QIODevice::ReadOnly), "exported PDF must be readable");
    require(pdf.read(5) == QByteArray("%PDF-"), "exported file must be a PDF");

    const QString pdfinfo = QStandardPaths::findExecutable(QStringLiteral("pdfinfo"));
    if (!pdfinfo.isEmpty()) {
        QProcess inspect;
        inspect.start(pdfinfo, {pdf.fileName()});
        require(inspect.waitForFinished(), "pdfinfo must finish");
        const QString details = QString::fromLocal8Bit(inspect.readAllStandardOutput());
        require(details.contains(QStringLiteral("Page size:")) && details.contains(QStringLiteral("(A4)")),
                "exported PDF page size must be A4");
        const auto pageMatch = QRegularExpression(QStringLiteral("Pages:\\s+(\\d+)")).match(details);
        require(pageMatch.hasMatch() && pageMatch.captured(1).toInt() > 1,
                "long lyrics must span multiple PDF pages");
    }

    MainWindow window;
    const auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("mainTabs"));
    require(tabs && tabs->count() == 2, "the app must expose only kana and lyrics tools");
    const auto *kanaGrid = window.findChild<QWidget *>(QStringLiteral("kanaGrid"));
    require(kanaGrid && kanaGrid->findChildren<QPushButton *>().size() == 71,
            "kana learning must contain 71 kana buttons");
    const QDir audio(QStringLiteral(":/audio"));
    require(audio.entryList({QStringLiteral("*.ogg")}).size() == 71,
            "all 71 offline pronunciation recordings must be bundled");
    auto *selectedKana = window.findChild<QLabel *>(QStringLiteral("selectedKana"));
    auto *kanaGroup = window.findChild<QComboBox *>(QStringLiteral("kanaGroup"));
    auto *kanaRow = window.findChild<QComboBox *>(QStringLiteral("kanaRow"));
    auto *showRomaji = window.findChild<QCheckBox *>(QStringLiteral("showRomaji"));
    auto *hiragana = window.findChild<QRadioButton *>(QStringLiteral("hiragana"));
    auto *katakana = window.findChild<QRadioButton *>(QStringLiteral("katakana"));
    require(selectedKana && kanaGroup && kanaRow && showRomaji && hiragana && katakana,
            "kana study controls must be available");
    kanaGroup->setCurrentIndex(2);
    require(selectedKana->text() == QStringLiteral("が"), "filtering must select the first kana in the group");
    katakana->setChecked(true);
    require(selectedKana->text() == QStringLiteral("ガ"), "script switching must update the selected kana");
    kanaGroup->setCurrentIndex(3);
    require(selectedKana->text() == QStringLiteral("パ"), "half-voiced kana filtering must work");
    require(kanaGrid->findChildren<QPushButton *>().size() == 5,
            "half-voiced kana filter must show five buttons");
    kanaGroup->setCurrentIndex(0);
    hiragana->setChecked(true);
    kanaRow->setCurrentIndex(1);
    require(kanaGrid->findChildren<QPushButton *>().size() == 5,
            "row filter must show a single kana row");
    require(selectedKana->text() == QStringLiteral("あ"),
            "row filter must select the first kana in that row");
    auto *aButton = kanaGrid->findChild<QPushButton *>();
    require(aButton && aButton->text().contains(QLatin1Char('\n')),
            "kana grid should show romanization by default");
    showRomaji->setChecked(false);
    aButton = kanaGrid->findChild<QPushButton *>();
    require(aButton && !aButton->text().contains(QLatin1Char('\n')),
            "romanization can be hidden");
    aButton->click();
    const auto *player = window.findChild<QMediaPlayer *>();
    const QUrl audioUrl = player ? player->currentMedia().request().url() : QUrl();
    require(player && audioUrl.toString().endsWith(QStringLiteral("/a.ogg")),
            "clicking a kana must load its bundled offline recording");
    require(QFile::exists(audioUrl.toLocalFile()),
            "bundled pronunciation must be cached as a local file for playback");
    auto *lyrics = window.findChild<QPlainTextEdit *>(QStringLiteral("lyricsInput"));
    auto *annotateButton = window.findChild<QPushButton *>(QStringLiteral("annotateButton"));
    auto *exportButton = window.findChild<QPushButton *>(QStringLiteral("exportButton"));
    require(lyrics && annotateButton && exportButton, "lyrics controls must be available");
    lyrics->setPlainText(QStringLiteral("学校へ行く"));
    annotateButton->click();
    require(exportButton->isEnabled(), "PDF export must be enabled after annotation");
    lyrics->appendPlainText(QStringLiteral("明日"));
    require(!exportButton->isEnabled(), "PDF export must require fresh annotation after edits");

    std::cout << "Annotation and A4 PDF export checks passed.\n";
    return 0;
}
