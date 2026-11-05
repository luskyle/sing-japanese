#pragma once

#include "annotator.h"

#include <QObject>
#include <QMediaPlayer>
#include <QVariantList>
#include <QStringList>

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
    Q_INVOKABLE void clearError();

signals:
    void kanaChanged();
    void lyricsChanged();
    void resultChanged();
    void errorChanged();
    void playingChanged();

private:
    void refreshSelection();
    void fail(const QString &message);
    bool katakana_ = false;
    int groupIndex_ = 0;
    int rowIndex_ = 0;
    QString selectedId_ = QStringLiteral("a");
    QString input_;
    QString annotatedInput_;
    QString status_;
    QString error_;
    QVector<Segment> annotated_;
    QMediaPlayer player_;
};
