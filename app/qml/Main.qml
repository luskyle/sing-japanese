import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: window
    objectName: "mainWindow"
    visible: true
    width: 1120
    height: 800
    minimumWidth: 800
    minimumHeight: 620
    title: "假名小调"
    color: "#f4f3ec"
    font.family: "Noto Sans CJK JP"
    font.pixelSize: 13
    property int page: 0
    onPageChanged: backend.stopAudio()

    Shortcut { sequence: "Ctrl+Return"; enabled: window.page === 1; onActivated: backend.annotateText() }
    Shortcut { sequence: "Ctrl+S"; enabled: window.page === 1 && backend.canExport; onActivated: backend.choosePdf() }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 82
            color: "#fcfcf8"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                spacing: 14
                Rectangle {
                    width: 42; height: 42; radius: 13; color: "#2d6152"
                    Text { anchors.centerIn: parent; text: "あ"; color: "#ffffff"; font.pixelSize: 26 }
                }
                Column {
                    spacing: 3
                    Text { text: "假名小调"; font.pixelSize: 18; font.weight: Font.DemiBold; color: "#243d32" }
                    Text { text: "S I N G   J A P A N E S E"; font.pixelSize: 9; color: "#839087" }
                }
                Item { Layout.fillWidth: true }
                RowLayout {
                    spacing: 6
                    ActionButton {
                        objectName: "kanaTab"
                        text: "假名学习"; selected: window.page === 0
                        onClicked: window.page = 0
                    }
                    ActionButton {
                        objectName: "lyricsTab"
                        text: "歌词注音"; selected: window.page === 1
                        onClicked: window.page = 1
                    }
                }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#e1e4da" }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? errorText.implicitHeight + 28 : 0
            visible: backend.error.length > 0
            color: "#fff1ea"
            Text {
                id: errorText
                anchors.left: parent.left; anchors.right: dismiss.left
                anchors.margins: 24; anchors.verticalCenter: parent.verticalCenter
                text: backend.error; color: "#9b4935"; font.pixelSize: 12; wrapMode: Text.Wrap
                Accessible.role: Accessible.AlertMessage
            }
            ActionButton {
                id: dismiss
                anchors.right: parent.right; anchors.rightMargin: 16; anchors.verticalCenter: parent.verticalCenter
                text: "关闭"; implicitHeight: 30; implicitWidth: 60
                onClicked: backend.clearError()
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: window.page
            KanaPage {}
            LyricsPage {}
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 28; Layout.rightMargin: 28; Layout.bottomMargin: 15
            spacing: 6
            Rectangle { width: 5; height: 5; radius: 3; color: "#618c77" }
            Text { text: "本地运行 · 无需联网"; color: "#7c887f"; font.pixelSize: 10 }
            Item { Layout.fillWidth: true }
            Text { text: "让文字与声音，慢慢熟悉。"; color: "#91998e"; font.pixelSize: 10 }
        }
    }
}
