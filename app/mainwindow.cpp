#include "mainwindow.h"

#include "annotationview.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <array>

namespace {
struct KanaRow {
    const char *row;
    const char *hira;
    const char *kata;
    const char *const *romaji;
    int count;
    const char *group;
};

const char *vowels[] = {"a", "i", "u", "e", "o"};
const char *k[] = {"ka", "ki", "ku", "ke", "ko"};
const char *s[] = {"sa", "shi", "su", "se", "so"};
const char *t[] = {"ta", "chi", "tsu", "te", "to"};
const char *n[] = {"na", "ni", "nu", "ne", "no"};
const char *h[] = {"ha", "hi", "fu", "he", "ho"};
const char *m[] = {"ma", "mi", "mu", "me", "mo"};
const char *y[] = {"ya", "yu", "yo"};
const char *r[] = {"ra", "ri", "ru", "re", "ro"};
const char *w[] = {"wa", "wo"};
const char *nasal[] = {"n"};
const char *g[] = {"ga", "gi", "gu", "ge", "go"};
const char *z[] = {"za", "ji", "zu", "ze", "zo"};
const char *d[] = {"da", "ji", "zu", "de", "do"};
const char *b[] = {"ba", "bi", "bu", "be", "bo"};
const char *p[] = {"pa", "pi", "pu", "pe", "po"};

const KanaRow rows[] = {
    {"元音", "あいうえお", "アイウエオ", vowels, 5, "清音"},
    {"K 行", "かきくけこ", "カキクケコ", k, 5, "清音"},
    {"S 行", "さしすせそ", "サシスセソ", s, 5, "清音"},
    {"T 行", "たちつてと", "タチツテト", t, 5, "清音"},
    {"N 行", "なにぬねの", "ナニヌネノ", n, 5, "清音"},
    {"H 行", "はひふへほ", "ハヒフヘホ", h, 5, "清音"},
    {"M 行", "まみむめも", "マミムメモ", m, 5, "清音"},
    {"Y 行", "やゆよ", "ヤユヨ", y, 3, "清音"},
    {"R 行", "らりるれろ", "ラリルレロ", r, 5, "清音"},
    {"W 行", "わを", "ワヲ", w, 2, "清音"},
    {"拨音", "ん", "ン", nasal, 1, "清音"},
    {"G 行", "がぎぐげご", "ガギグゲゴ", g, 5, "浊音"},
    {"Z 行", "ざじずぜぞ", "ザジズゼゾ", z, 5, "浊音"},
    {"D 行", "だぢづでど", "ダヂヅデド", d, 5, "浊音"},
    {"B 行", "ばびぶべぼ", "バビブベボ", b, 5, "浊音"},
    {"P 行", "ぱぴぷぺぽ", "パピプペポ", p, 5, "半浊音"},
};

QString noteFor(const QString &id)
{
    if (id == "wo") return QStringLiteral("を／ヲ：现代标准语通常读作 o，主要用作助词。");
    if (id == "ji" || id == "di") return QStringLiteral("じ／ジ 与 ぢ／ヂ 在现代标准语中通常同音。");
    if (id == "zu" || id == "du") return QStringLiteral("ず／ズ 与 づ／ヅ 在现代标准语中通常同音。");
    if (id == "n") return QStringLiteral("ん／ン 是一个音拍，实际发音会受后续声音影响。");
    if (id == "u") return QStringLiteral("う／ウ 的嘴唇通常不明显前突。");
    if (id == "fu") return QStringLiteral("ふ／フ 的辅音通常由双唇靠近发出。");
    if (id == "shi") return QStringLiteral("し／シ 的罗马音写作 shi。");
    if (id == "chi") return QStringLiteral("ち／チ 的罗马音写作 chi。");
    if (id == "tsu") return QStringLiteral("つ／ツ 的罗马音写作 tsu。");
    if (id == "ha") return QStringLiteral("は 单独读 ha，作主题助词时读 wa。");
    if (id == "he") return QStringLiteral("へ 单独读 he，作方向助词时读 e。");
    return QStringLiteral("点击假名听音，熟悉后可隐藏罗马音回想读音。");
}

QVector<KanaEntry> makeEntries()
{
    QVector<KanaEntry> result;
    for (const KanaRow &row : rows) {
        const QVector<uint> h = QString::fromUtf8(row.hira).toUcs4();
        const QVector<uint> k = QString::fromUtf8(row.kata).toUcs4();
        for (int index = 0; index < row.count; ++index) {
            const uint hCode = h.at(index);
            const uint kCode = k.at(index);
            QString audioId = QString::fromLatin1(row.romaji[index]);
            if (QString::fromUtf8(row.row) == QStringLiteral("D 行") && index == 1)
                audioId = QStringLiteral("di");
            else if (QString::fromUtf8(row.row) == QStringLiteral("D 行") && index == 2)
                audioId = QStringLiteral("du");
            result.push_back({
                audioId,
                QString::fromUcs4(&hCode, 1),
                QString::fromUcs4(&kCode, 1),
                QString::fromLatin1(row.romaji[index]),
                QString::fromUtf8(row.row),
                QString::fromUtf8(row.group),
            });
        }
    }
    return result;
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), entries_(makeEntries()), player_(new QMediaPlayer(this))
{
    setWindowTitle(QStringLiteral("假名小调"));
    resize(920, 680);
    setMinimumSize(720, 560);
    auto *root = new QWidget(this);
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(18, 14, 18, 16);
    layout->addWidget(new QLabel(QStringLiteral("假名小调"), root));
    tabs_ = new QTabWidget(root);
    tabs_->setObjectName(QStringLiteral("mainTabs"));
    tabs_->addTab(createKanaPage(), QStringLiteral("假名学习"));
    tabs_->addTab(createLyricsPage(), QStringLiteral("歌词注音"));
    layout->addWidget(tabs_);
    setCentralWidget(root);
    connect(tabs_, &QTabWidget::currentChanged, player_, &QMediaPlayer::stop);
    connect(player_, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error), this,
            [this](QMediaPlayer::Error) {
        pronunciation_->setText(QStringLiteral("录音播放失败：%1").arg(player_->errorString()));
    });

    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #f5f5f4; }"
        "QWidget { color: #242424; }"
        "QTabWidget::pane { border: 1px solid #d6d3d1; background: white; }"
        "QTabBar::tab { padding: 8px 18px; }"
        "QPushButton { padding: 6px 12px; }"
        "QPlainTextEdit, QComboBox { border: 1px solid #c9c7c4; padding: 6px; background: white; }"
        "QPushButton[kanaButton=\"true\"] { min-width: 62px; min-height: 48px; }"
    ));
    rebuildKanaGrid();
}

QWidget *MainWindow::createKanaPage()
{
    auto *page = new QWidget(this);
    auto *outer = new QVBoxLayout(page);
    auto *controls = new QHBoxLayout;
    hiragana_ = new QRadioButton(QStringLiteral("平假名"), page);
    hiragana_->setObjectName(QStringLiteral("hiragana"));
    katakana_ = new QRadioButton(QStringLiteral("片假名"), page);
    katakana_->setObjectName(QStringLiteral("katakana"));
    hiragana_->setChecked(true);
    auto *scriptGroup = new QButtonGroup(page);
    scriptGroup->addButton(hiragana_);
    scriptGroup->addButton(katakana_);
    group_ = new QComboBox(page);
    group_->setObjectName(QStringLiteral("kanaGroup"));
    group_->addItems({QStringLiteral("全部"), QStringLiteral("清音"), QStringLiteral("浊音"), QStringLiteral("半浊音")});
    row_ = new QComboBox(page);
    row_->setObjectName(QStringLiteral("kanaRow"));
    row_->addItem(QStringLiteral("全部"));
    for (const KanaRow &row : rows)
        row_->addItem(QString::fromUtf8(row.row));
    controls->addWidget(hiragana_);
    controls->addWidget(katakana_);
    controls->addSpacing(20);
    controls->addWidget(new QLabel(QStringLiteral("字表"), page));
    controls->addWidget(group_);
    controls->addWidget(new QLabel(QStringLiteral("行"), page));
    controls->addWidget(row_);
    showRomaji_ = new QCheckBox(QStringLiteral("显示罗马音"), page);
    showRomaji_->setObjectName(QStringLiteral("showRomaji"));
    showRomaji_->setChecked(true);
    controls->addWidget(showRomaji_);
    controls->addStretch();
    outer->addLayout(controls);

    auto *gridHost = new QWidget(page);
    gridHost->setObjectName(QStringLiteral("kanaGrid"));
    auto *grid = new QGridLayout(gridHost);
    grid->setContentsMargins(4, 12, 4, 12);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    auto *gridScroll = new QScrollArea(page);
    gridScroll->setObjectName(QStringLiteral("kanaGridScroll"));
    gridScroll->setWidgetResizable(true);
    gridScroll->setWidget(gridHost);
    outer->addWidget(gridScroll, 1);
    connect(hiragana_, &QRadioButton::toggled, this, &MainWindow::rebuildKanaGrid);
    connect(katakana_, &QRadioButton::toggled, this, &MainWindow::rebuildKanaGrid);
    connect(group_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        row_->blockSignals(true);
        row_->clear();
        row_->addItem(QStringLiteral("全部"));
        for (const KanaRow &row : rows) {
            if (group_->currentIndex() == 0 || QString::fromUtf8(row.group) == group_->currentText())
                row_->addItem(QString::fromUtf8(row.row));
        }
        row_->blockSignals(false);
        rebuildKanaGrid();
    });
    connect(row_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::rebuildKanaGrid);
    connect(showRomaji_, &QCheckBox::toggled, this, &MainWindow::rebuildKanaGrid);

    auto *details = new QHBoxLayout;
    selectedCharacter_ = new QLabel(QStringLiteral("あ"), page);
    selectedCharacter_->setObjectName(QStringLiteral("selectedKana"));
    selectedCharacter_->setMinimumWidth(72);
    selectedCharacter_->setStyleSheet(QStringLiteral("font-size: 42px;"));
    romaji_ = new QLabel(QStringLiteral("a"), page);
    romaji_->setMinimumWidth(40);
    pronunciation_ = new QLabel(QStringLiteral("点击假名听音。"), page);
    pronunciation_->setWordWrap(true);
    details->addWidget(selectedCharacter_);
    details->addWidget(romaji_);
    details->addWidget(pronunciation_, 1);
    outer->addLayout(details);
    return page;
}

QWidget *MainWindow::createLyricsPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    auto *labels = new QHBoxLayout;
    labels->addWidget(new QLabel(QStringLiteral("日语原文"), page));
    labels->addStretch();
    labels->addWidget(new QLabel(QStringLiteral("最多 20,000 字符"), page));
    layout->addLayout(labels);

    auto *columns = new QHBoxLayout;
    input_ = new QPlainTextEdit(page);
    input_->setObjectName(QStringLiteral("lyricsInput"));
    input_->setPlaceholderText(QStringLiteral("在此粘贴日语歌词或文本…"));
    input_->setTabChangesFocus(false);
    input_->setLineWrapMode(QPlainTextEdit::NoWrap);
    result_ = new AnnotationView(page);
    resultScroll_ = new QScrollArea(page);
    resultScroll_->setWidgetResizable(true);
    resultScroll_->setWidget(result_);
    columns->addWidget(input_, 1);
    columns->addWidget(resultScroll_, 1);
    layout->addLayout(columns, 1);

    auto *actions = new QHBoxLayout;
    auto *annotateButton = new QPushButton(QStringLiteral("注音"), page);
    annotateButton->setObjectName(QStringLiteral("annotateButton"));
    exportButton_ = new QPushButton(QStringLiteral("导出 A4 PDF…"), page);
    exportButton_->setObjectName(QStringLiteral("exportButton"));
    exportButton_->setEnabled(false);
    resultStatus_ = new QLabel(QStringLiteral("注音结果会显示在右侧。"), page);
    actions->addWidget(annotateButton);
    actions->addWidget(exportButton_);
    actions->addWidget(resultStatus_, 1);
    layout->addLayout(actions);
    connect(annotateButton, &QPushButton::clicked, this, &MainWindow::annotateText);
    connect(exportButton_, &QPushButton::clicked, this, &MainWindow::exportPdf);
    connect(input_, &QPlainTextEdit::textChanged, this, [this] {
        if (input_->toPlainText() != annotatedInput_) {
            exportButton_->setEnabled(false);
            resultStatus_->setText(QStringLiteral("原文已修改，请重新注音后导出。"));
        }
    });
    return page;
}

QString MainWindow::currentCharacter(const KanaEntry &entry) const
{
    return hiragana_->isChecked() ? entry.hiragana : entry.katakana;
}

void MainWindow::rebuildKanaGrid()
{
    auto *host = findChild<QWidget *>(QStringLiteral("kanaGrid"));
    auto *grid = qobject_cast<QGridLayout *>(host->layout());
    while (QLayoutItem *item = grid->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    int rowIndex = 0;
    for (const KanaRow &row : rows) {
        const QString group = QString::fromUtf8(row.group);
        const int selectedGroup = group_->currentIndex();
        if (selectedGroup > 0 && group != group_->currentText())
            continue;
        if (row_->currentIndex() > 0 && QString::fromUtf8(row.row) != row_->currentText())
            continue;
        auto *label = new QLabel(QString::fromUtf8(row.row), host);
        grid->addWidget(label, rowIndex, 0);
        const QVector<uint> chars = QString::fromUtf8(hiragana_->isChecked() ? row.hira : row.kata).toUcs4();
        for (int index = 0; index < row.count; ++index) {
            const uint codePoint = chars.at(index);
            const QString character = QString::fromUcs4(&codePoint, 1);
            const QString roman = QString::fromLatin1(row.romaji[index]);
            auto *button = new QPushButton(
                showRomaji_->isChecked() ? character + QLatin1Char('\n') + roman : character, host);
            button->setProperty("kanaButton", true);
            button->setAccessibleName(character + QStringLiteral(" ") + roman);
            const QString id = QString::fromLatin1(row.romaji[index])
                == QStringLiteral("ji") && QString::fromUtf8(row.row) == QStringLiteral("D 行")
                ? QStringLiteral("di")
                : QString::fromLatin1(row.romaji[index])
                    == QStringLiteral("zu") && QString::fromUtf8(row.row) == QStringLiteral("D 行")
                    ? QStringLiteral("du")
                    : QString::fromLatin1(row.romaji[index]);
            connect(button, &QPushButton::clicked, this, [this, id] {
                selectKana(id);
            });
            grid->addWidget(button, rowIndex, index + 1);
        }
        ++rowIndex;
    }
    const bool currentGroupMatches = selected_
        && (group_->currentIndex() == 0 || selected_->group == group_->currentText());
    const bool currentRowMatches = selected_
        && (row_->currentIndex() == 0 || selected_->row == row_->currentText());
    if (!currentGroupMatches || !currentRowMatches) {
        for (const KanaEntry &entry : entries_) {
            const bool matchingGroup = group_->currentIndex() == 0 || entry.group == group_->currentText();
            const bool matchingRow = row_->currentIndex() == 0 || entry.row == row_->currentText();
            if (matchingGroup && matchingRow) {
                selectKana(entry.audioId, false);
                break;
            }
        }
    } else {
        selectKana(selected_->audioId, false);
    }
    romaji_->setVisible(showRomaji_->isChecked());
}

void MainWindow::selectKana(const QString &id, bool playAudio)
{
    for (const KanaEntry &entry : entries_) {
        if (entry.audioId == id) {
            selected_ = &entry;
            break;
        }
    }
    if (!selected_)
        return;
    selectedCharacter_->setText(currentCharacter(*selected_));
    romaji_->setText(selected_->romaji);
    pronunciation_->setText(noteFor(id));
    if (!playAudio)
        return;
    const QString cacheDirectory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (!QDir().mkpath(cacheDirectory)) {
        pronunciation_->setText(QStringLiteral("无法创建录音缓存目录。"));
        return;
    }
    const QString audioPath = cacheDirectory + QStringLiteral("/") + id + QStringLiteral(".ogg");
    if (!QFileInfo::exists(audioPath)) {
        QFile resource(QStringLiteral(":/audio/") + id + QStringLiteral(".ogg"));
        if (!resource.open(QIODevice::ReadOnly)) {
            pronunciation_->setText(QStringLiteral("找不到本地发音录音。"));
            return;
        }
        const QByteArray bytes = resource.readAll();
        QSaveFile cached(audioPath);
        if (!cached.open(QIODevice::WriteOnly) || cached.write(bytes) != bytes.size()) {
            cached.cancelWriting();
            pronunciation_->setText(QStringLiteral("无法准备本地发音录音。"));
            return;
        }
        if (!cached.commit()) {
            pronunciation_->setText(QStringLiteral("无法保存本地发音录音：%1").arg(cached.errorString()));
            return;
        }
    }
    player_->setMedia(QUrl::fromLocalFile(audioPath));
    player_->play();
}

void MainWindow::annotateText()
{
    const QString text = input_->toPlainText();
    if (text.trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("歌词注音"), QStringLiteral("请先输入日语文本。"));
        return;
    }
    try {
        annotated_ = annotate(text);
        annotatedInput_ = text;
        result_->setSegments(annotated_);
        result_->updateGeometry();
        resultScroll_->widget()->adjustSize();
        exportButton_->setEnabled(true);
        int unknown = 0;
        for (const Segment &segment : annotated_)
            unknown += segment.unknown ? 1 : 0;
        resultStatus_->setText(unknown == 0
                                   ? QStringLiteral("注音完成")
                                   : QStringLiteral("%1 个词未找到读音").arg(unknown));
    } catch (const std::exception &error) {
        QMessageBox::warning(this, QStringLiteral("注音失败"), QString::fromUtf8(error.what()));
    }
}

void MainWindow::exportPdf()
{
    if (annotated_.isEmpty() || input_->toPlainText() != annotatedInput_)
        return;
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出注音 PDF"), QStringLiteral("lyrics.pdf"), QStringLiteral("PDF 文件 (*.pdf)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!exportAnnotatedPdf(path, annotated_, &error)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), error);
        return;
    }
    resultStatus_->setText(QStringLiteral("已导出：%1").arg(QFileInfo(path).fileName()));
}
