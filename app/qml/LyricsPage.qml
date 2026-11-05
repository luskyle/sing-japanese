import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import SingJapanese 1.0

Item {
    id: page
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 28; spacing: 18
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 5
                Text { text: "把歌词，读得更清楚"; font.pixelSize: 25; font.weight: Font.DemiBold; color: "#243d32" }
                Text { text: "本地词典注音，保留原文与换行。"; color: "#7b867b"; font.pixelSize: 12 }
            }
            Item { Layout.fillWidth: true }
            Text { text: "02 / FURIGANA"; color: "#80937e"; font.pixelSize: 11; font.letterSpacing: 1.5 }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 18
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                radius: 16; color: "#ffffff"; border.color: "#e1e6dc"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 16
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "原文"; font.pixelSize: 14; font.weight: Font.DemiBold; color: "#364b3c" }
                        Item { Layout.fillWidth: true }
                        Text { text: Array.from(editor.text).length + " / 20,000"; color: "#9aa392"; font.pixelSize: 10 }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#edf0e9" }
                    ScrollView {
                        id: inputScroll
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true
                        TextArea {
                            id: editor
                            objectName: "lyricsInput"
                            text: backend.input
                            onTextChanged: backend.input = text
                            placeholderText: "在这里粘贴日语歌词…\n\n注音结果会显示在右侧。"
                            placeholderTextColor: "#b1b8aa"
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            color: "#3d493e"; selectionColor: "#d6e5d1"; selectedTextColor: "#243d32"
                            font.family: "Noto Sans CJK JP"; font.pixelSize: 16
                            padding: 0
                            background: Item {}
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        ActionButton {
                            objectName: "annotateButton"
                            primary: true; text: "自动注音"
                            onClicked: backend.annotateText()
                        }
                        Item { Layout.fillWidth: true }
                        Text { text: "Ctrl + Enter"; color: "#a1aa97"; font.pixelSize: 10 }
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                radius: 16; color: "#ffffff"; border.color: "#e1e6dc"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 16
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "注音稿"; font.pixelSize: 14; font.weight: Font.DemiBold; color: "#364b3c" }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            implicitWidth: 60; implicitHeight: 24; radius: 6; color: "#eef2e8"
                            Text { anchors.centerIn: parent; text: "A4 / PDF"; color: "#81916f"; font.pixelSize: 9 }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#edf0e9" }
                    Item {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        Column {
                            anchors.centerIn: parent; spacing: 14
                            visible: backend.segments.length === 0
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "歌"; color: "#d6dfcc"; font.pixelSize: 58 }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "让每个汉字，都有读音"; color: "#9da890"; font.pixelSize: 12 }
                        }
                        Flickable {
                            id: output
                            objectName: "annotationScroll"
                            anchors.fill: parent; clip: true
                            visible: backend.segments.length > 0
                            contentHeight: preview.height
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}
                            AnnotationView {
                                id: preview
                                objectName: "annotationPreview"
                                width: output.width - 10
                                height: implicitHeight
                                segments: backend.segments
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: backend.status; color: "#8d9a7f"; font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                        ActionButton {
                            objectName: "exportButton"
                            text: "导出 PDF"
                            enabled: backend.canExport
                            onClicked: backend.choosePdf()
                        }
                    }
                }
            }
        }
        Text {
            Layout.fillWidth: true
            text: "常规词典读音可能不同于歌手的特殊唱法；文本仅保留在本次会话。"
            wrapMode: Text.Wrap; color: "#939e87"; font.pixelSize: 10
        }
    }
}
