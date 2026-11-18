import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import SingJapanese 1.0

ApplicationWindow {
    id: window
    objectName: "mainWindow"
    visible: true
    width: Math.min(1360, Screen.desktopAvailableWidth)
    height: Math.min(900, Screen.desktopAvailableHeight)
    minimumWidth: Math.min(1120, Screen.desktopAvailableWidth)
    minimumHeight: Math.min(800, Screen.desktopAvailableHeight)
    title: "唱日语"
    color: Theme.window
    palette.window: Theme.window
    palette.windowText: Theme.text
    palette.base: Theme.surface
    palette.alternateBase: Theme.button
    palette.text: Theme.text
    palette.button: Theme.button
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.window
    palette.mid: Theme.border
    palette.dark: Theme.border
    palette.light: Theme.surface
    font.family: "Noto Sans CJK JP"
    font.pixelSize: Math.round(16 * uiScale)
    readonly property real uiScale: Math.max(1, Math.min(1.3, Math.min(width / 1120, height / 800)))
    property int page: 0
    onPageChanged: backend.stopAudio()

    Shortcut { sequence: "Ctrl+Return"; enabled: window.page === 1; onActivated: backend.annotateText() }
    Shortcut { sequence: "Ctrl+S"; enabled: window.page === 1 && backend.canExport; onActivated: backend.choosePdf() }
    Shortcut { sequence: StandardKey.Print; enabled: window.page === 1 && backend.canExport; onActivated: backend.printDocument() }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(Math.round(80 * window.uiScale), headerRow.implicitHeight + 20)
            color: Theme.header
            RowLayout {
                id: headerRow
                anchors.fill: parent
                anchors.leftMargin: window.width < 700 ? 16 : 28
                anchors.rightMargin: window.width < 700 ? 16 : 28
                spacing: Math.round(12 * window.uiScale)
                Rectangle {
                    Layout.preferredWidth: Math.round(46 * window.uiScale)
                    Layout.preferredHeight: Layout.preferredWidth
                    radius: 13; color: Theme.primary
                    Text { anchors.centerIn: parent; text: "あ"; color: Theme.primaryText; font.pixelSize: Math.round(28 * window.uiScale) }
                }
                Column {
                    spacing: 3
                    Text { text: "唱日语"; font.pixelSize: Math.round(20 * window.uiScale); font.weight: Font.DemiBold; color: Theme.heading }
                    Text {
                        visible: window.width >= 700
                        text: "SING JAPANESE"; font.pixelSize: Math.round(14 * window.uiScale); color: Theme.secondary
                    }
                }
                Item { Layout.fillWidth: true }
                RowLayout {
                    spacing: 6
                    ActionButton {
                        objectName: "themeToggleButton"
                        compact: true
                        text: backend.darkMode ? "浅色模式" : "深色模式"
                        onClicked: backend.darkMode = !backend.darkMode
                    }
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
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? Math.max(errorText.implicitHeight, dismiss.implicitHeight) + 24 : 0
            visible: backend.error.length > 0
            color: Theme.errorSurface
            Text {
                id: errorText
                anchors.left: parent.left; anchors.right: dismiss.left
                anchors.margins: 16; anchors.verticalCenter: parent.verticalCenter
                text: backend.error; color: Theme.errorText; font.pixelSize: Math.round(16 * window.uiScale); wrapMode: Text.Wrap
                Accessible.role: Accessible.AlertMessage
            }
            ActionButton {
                id: dismiss
                anchors.right: parent.right; anchors.rightMargin: 16; anchors.verticalCenter: parent.verticalCenter
                text: "关闭"
                onClicked: backend.clearError()
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            currentIndex: window.page
            KanaPage { uiScale: window.uiScale }
            LyricsPage { uiScale: window.uiScale }
        }

    }
}
