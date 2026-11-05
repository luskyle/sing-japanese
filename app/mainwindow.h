#pragma once

#include "annotator.h"

#include <QMainWindow>

class AnnotationView;
class QComboBox;
class QCheckBox;
class QLabel;
class QMediaPlayer;
class QPlainTextEdit;
class QRadioButton;
class QScrollArea;
class QPushButton;
class QTabWidget;
class QButtonGroup;

struct KanaEntry {
    QString audioId;
    QString hiragana;
    QString katakana;
    QString romaji;
    QString row;
    QString group;
};

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void rebuildKanaGrid();
    void selectKana(const QString &id, bool playAudio = true);
    void annotateText();
    void exportPdf();

private:
    QVector<KanaEntry> entries_;
    const KanaEntry *selected_ = nullptr;
    QTabWidget *tabs_ = nullptr;
    QComboBox *group_ = nullptr;
    QComboBox *row_ = nullptr;
    QCheckBox *showRomaji_ = nullptr;
    QRadioButton *hiragana_ = nullptr;
    QRadioButton *katakana_ = nullptr;
    QLabel *selectedCharacter_ = nullptr;
    QLabel *romaji_ = nullptr;
    QLabel *pronunciation_ = nullptr;
    QPlainTextEdit *input_ = nullptr;
    AnnotationView *result_ = nullptr;
    QScrollArea *resultScroll_ = nullptr;
    QLabel *resultStatus_ = nullptr;
    QPushButton *exportButton_ = nullptr;
    QMediaPlayer *player_ = nullptr;
    QVector<Segment> annotated_;
    QString annotatedInput_;

    QWidget *createKanaPage();
    QWidget *createLyricsPage();
    QString currentCharacter(const KanaEntry &entry) const;
};
