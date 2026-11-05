import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: page
    property bool showRomaji: true
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 18
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 5
                Text { text: "从一个音开始"; font.pixelSize: 25; font.weight: Font.DemiBold; color: "#243d32" }
                Text { text: "点击假名听原音，认识字形，也记住声音。"; color: "#7b867b"; font.pixelSize: 12 }
            }
            Item { Layout.fillWidth: true }
            Text { text: "01 / KANA"; color: "#80937e"; font.pixelSize: 11; font.letterSpacing: 1.5 }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 20
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                radius: 16; color: "#ffffff"; border.color: "#e1e6dc"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 18; spacing: 14
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        ActionButton {
                            objectName: "hiraganaButton"
                            text: "平假名"; selected: !backend.katakana; implicitWidth: 86
                            onClicked: backend.katakana = false
                        }
                        ActionButton {
                            objectName: "katakanaButton"
                            text: "片假名"; selected: backend.katakana; implicitWidth: 86
                            onClicked: backend.katakana = true
                        }
                        Item { Layout.fillWidth: true }
                        CheckBox {
                            objectName: "romajiToggle"
                            text: "罗马音"; checked: page.showRomaji
                            onToggled: page.showRomaji = checked
                            font.pixelSize: 11
                            palette.highlight: "#2d6152"
                        }
                    }
                    RowLayout {
                        spacing: 8
                        ChoiceBox {
                            objectName: "groupFilter"
                            model: ["全部 · 71", "清音 · 46", "浊音 · 20", "半浊音 · 5"]
                            currentIndex: backend.groupIndex
                            onActivated: backend.groupIndex = currentIndex
                        }
                        ChoiceBox {
                            objectName: "rowFilter"
                            model: backend.rowNames; currentIndex: backend.rowIndex
                            onActivated: backend.rowIndex = currentIndex
                        }
                        Item { Layout.fillWidth: true }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#edf0e9" }
                    Flickable {
                        id: chart
                        objectName: "kanaChart"
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true; contentHeight: chartRows.height
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        Column {
                            id: chartRows
                            width: chart.width - 12; spacing: 7
                            Row {
                                width: parent.width; height: 24
                                Item { width: 38; height: 1 }
                                Repeater {
                                    model: ["a", "i", "u", "e", "o"]
                                    Text {
                                        width: (chartRows.width - 38) / 5; horizontalAlignment: Text.AlignHCenter
                                        text: modelData; color: "#96a18f"; font.pixelSize: 11
                                    }
                                }
                            }
                            Repeater {
                                model: backend.kanaRows
                                Row {
                                    id: kanaRow
                                    property var rowData: modelData
                                    spacing: 5
                                    width: chartRows.width; height: page.showRomaji ? 66 : 52
                                    Text {
                                        width: 33; height: parent.height
                                        text: kanaRow.rowData.name; color: "#929d8c"; font.pixelSize: 9
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    Repeater {
                                        model: kanaRow.rowData.cells
                                        Item {
                                            width: (chartRows.width - 38 - 25) / 5
                                            height: kanaRow.height
                                            ActionButton {
                                                id: kanaButton
                                                objectName: modelData.id ? "kana_" + modelData.id : ""
                                                anchors.fill: parent
                                                visible: !!modelData.id
                                                property bool isCurrent: modelData.id === backend.selected.id
                                                text: modelData.character || ""
                                                Accessible.name: text
                                                onClicked: backend.selectKana(modelData.id)
                                                background: Rectangle {
                                                    radius: 10
                                                    color: kanaButton.isCurrent ? "#e5eee4" : kanaButton.hovered ? "#f0f4ed" : "#f8f9f5"
                                                    border.color: kanaButton.activeFocus ? "#2d6152" : kanaButton.isCurrent ? "#91ae8c" : "#edf0e8"
                                                    Behavior on color { ColorAnimation { duration: 120 } }
                                                }
                                                contentItem: Column {
                                                    spacing: 1
                                                    Text {
                                                        width: parent.width; text: modelData.character || ""
                                                        font.pixelSize: 26; color: kanaButton.isCurrent ? "#2d6152" : "#34453b"
                                                        horizontalAlignment: Text.AlignHCenter
                                                    }
                                                    Text {
                                                        width: parent.width; visible: page.showRomaji
                                                        text: modelData.romaji || ""; color: "#8a9685"; font.pixelSize: 9
                                                        horizontalAlignment: Text.AlignHCenter
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Rectangle {
                Layout.preferredWidth: page.width < 950 ? 212 : 262
                Layout.fillHeight: true
                radius: 16; color: "#e9eee2"; border.color: "#dce4d3"
                Flickable {
                    id: detailScroll
                    objectName: "pronunciationScroll"
                    anchors.fill: parent; anchors.margins: 23
                    contentHeight: detailContents.height
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    Column {
                        id: detailContents
                        width: detailScroll.width
                        spacing: 12
                        Text { text: "发音手帖"; color: "#678066"; font.pixelSize: 11; font.letterSpacing: 2 }
                        Item { width: 1; height: 16 }
                        Text {
                            width: parent.width; horizontalAlignment: Text.AlignHCenter
                            text: backend.selected.character
                            font.pixelSize: page.width < 950 ? 84 : 108; color: "#2e5340"
                        }
                        Text {
                            objectName: "selectedRomaji"
                            visible: page.showRomaji
                            width: parent.width; horizontalAlignment: Text.AlignHCenter
                            text: backend.selected.romaji; color: "#678066"; font.pixelSize: 18
                        }
                        Text {
                            width: parent.width; horizontalAlignment: Text.AlignHCenter
                            text: (backend.katakana ? "平假名  " : "片假名  ") + backend.selected.counterpart
                            color: "#7f8e75"; font.pixelSize: 12
                        }
                        Item { width: 1; height: 10 }
                        ActionButton {
                            objectName: "replayButton"
                            width: parent.width; primary: true
                            text: backend.playing ? "正在播放…" : "播放原音"
                            onClicked: backend.replay()
                        }
                        Text {
                            text: "真人录音 · 离线播放"
                            width: parent.width; horizontalAlignment: Text.AlignHCenter
                            color: "#8c9981"; font.pixelSize: 10
                        }
                        Rectangle { width: parent.width; height: 1; color: "#d4dfca" }
                        Text {
                            width: parent.width
                            text: backend.selected.note
                            wrapMode: Text.Wrap; color: "#6a7c61"; font.pixelSize: 12; lineHeight: 1.55
                        }
                    }
                }
            }
        }
    }
}
