#pragma once

#include "annotator.h"
#include "annotationview.h"

#include <QObject>
#include <QMediaPlayer>
#include <QVariantList>
#include <QStringList>
#include <QTemporaryDir>

class Backend final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool katakana READ katakana WRITE setKatakana NOTIFY kanaChanged)
    Q_PROPERTY(int groupIndex READ groupIndex WRITE setGroupIndex NOTIFY kanaChanged)
    Q_PROPERTY(int rowIndex READ rowIndex WRITE setRowIndex NOTIFY kanaChanged)
    Q_PROPERTY(QStringList rowNames READ rowNames NOTIFY kanaChanged)
    Q_PROPERTY(QVariantList kanaRows READ kanaRows NOTIFY kanaChanged)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY kanaChanged)
    Q_PROPERTY(QString input READ input WRITE setInput NOTIFY lyricsChanged)
    Q_PROPERTY(QVariantList segments READ segments NOTIFY resultChanged)
    Q_PROPERTY(bool canExport READ canExport NOTIFY lyricsChanged)
    Q_PROPERTY(QString status READ status NOTIFY lyricsChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(bool romaji READ romaji WRITE setRomaji NOTIFY annotationOptionsChanged)
    Q_PROPERTY(bool fullRomaji READ fullRomaji WRITE setFullRomaji NOTIFY annotationOptionsChanged)
    Q_PROPERTY(qreal previewWidth READ previewWidth WRITE setPreviewWidth NOTIFY previewWidthChanged)
    Q_PROPERTY(qreal previewFontScale READ previewFontScale WRITE setPreviewFontScale NOTIFY annotationOptionsChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    bool katakana() const { return katakana_; }
    int groupIndex() const { return groupIndex_; }
    int rowIndex() const { return rowIndex_; }
    QStringList rowNames() const;
    QVariantList kanaRows() const;
    QVariantMap selected() const;
    QString input() const { return input_; }
    QVariantList segments() const;
    bool canExport() const { return !annotated_.isEmpty() && input_ == annotatedInput_; }
    QString status() const { return status_; }
    QString error() const { return error_; }
    bool playing() const { return player_.state() == QMediaPlayer::PlayingState; }
    bool darkMode() const { return darkMode_; }
    void setDarkMode(bool value);
    bool romaji() const { return annotationOptions_.romaji; }
    bool fullRomaji() const { return annotationOptions_.fullRomaji; }
    qreal previewWidth() const { return previewWidth_; }
    qreal previewFontScale() const { return annotationOptions_.fontScale; }
    void setRomaji(bool value);
    void setFullRomaji(bool value);
    void setPreviewWidth(qreal value);
    void setPreviewFontScale(qreal value);
    void setKatakana(bool value);
    void setGroupIndex(int value);
    void setRowIndex(int value);
    void setInput(const QString &value);
    Q_INVOKABLE void selectKana(const QString &id);
    Q_INVOKABLE void replay();
    Q_INVOKABLE void stopAudio();
    Q_INVOKABLE void annotateText();
    Q_INVOKABLE void choosePdf();
    Q_INVOKABLE bool savePdf(const QString &path);
    Q_INVOKABLE void chooseDocx();
    Q_INVOKABLE bool saveDocx(const QString &path);
    Q_INVOKABLE bool previewPdf();
    Q_INVOKABLE bool previewDocx();
    Q_INVOKABLE bool copyForWord();
    Q_INVOKABLE void printDocument();
    Q_INVOKABLE void clearError();

signals:
    void kanaChanged();
    void lyricsChanged();
    void resultChanged();
    void errorChanged();
    void playingChanged();
    void darkModeChanged();
    void annotationOptionsChanged();
    void previewWidthChanged();

private:
    void refreshSelection();
    void fail(const QString &message);
    bool previewDocument(bool docx);
    void chooseDocument(bool docx);
    bool katakana_ = false;
    bool darkMode_ = true;
    int groupIndex_ = 0;
    int rowIndex_ = 0;
    QString selectedId_ = QStringLiteral("a");
    QString input_;
    QString annotatedInput_;
    QString status_;
    QString error_;
    QVector<Segment> annotated_;
    QMediaPlayer player_;
    AnnotationOptions annotationOptions_;
    qreal previewWidth_ = annotationPrintableRect().width();
    QTemporaryDir previewDirectory_;
};
