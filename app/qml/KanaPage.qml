import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: page
    objectName: "kanaPage"
    property real uiScale: 1
    property bool showRomaji: true
    readonly property int margin: height < 650 ? 8 : 16
    readonly property int gap: Math.round((height < 650 ? 8 : 12) * uiScale)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: page.margin
        spacing: page.gap

        Flow {
            Layout.fillWidth: true
            spacing: Math.round(8 * page.uiScale)
            ActionButton {
                objectName: "hiraganaButton"
                text: "平假名"; selected: !backend.katakana
                onClicked: backend.katakana = false
            }
            ActionButton {
                objectName: "katakanaButton"
                text: "片假名"; selected: backend.katakana
                onClicked: backend.katakana = true
            }
            CheckBox {
                objectName: "romajiToggle"
                text: "罗马音"; checked: page.showRomaji
                onToggled: page.showRomaji = checked
                font.pixelSize: Math.round(16 * page.uiScale)
                palette.highlight: "#2d6152"
            }
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
        }
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.minimumHeight: 0
            radius: 16; color: "#ffffff"; border.color: "#e1e6dc"
            Item {
                id: chart
                objectName: "kanaChart"
                anchors.fill: parent
                anchors.margins: Math.round(8 * page.uiScale)
                readonly property var rows: backend.kanaRows
                readonly property real cellGap: Math.round(4 * page.uiScale)
                readonly property real blockGap: Math.round(16 * page.uiScale)
                readonly property real headingHeight: Math.round(24 * page.uiScale)
                readonly property int columnCount: Math.max(1, Math.min(
                    Math.floor((width + blockGap) / (200 * page.uiScale + blockGap)),
                    Math.ceil(rows.length / Math.max(1,
                        Math.floor((height - headingHeight) / (64 * page.uiScale + cellGap))))))
                readonly property int rowsPerBlock: Math.max(1, Math.ceil(rows.length / columnCount))
                readonly property real rowHeight: Math.max(1, Math.min(82 * page.uiScale,
                    (height - headingHeight - rowsPerBlock * cellGap) / rowsPerBlock))
                Row {
                    spacing: chart.blockGap
                    Repeater {
                        model: chart.columnCount
                        Column {
                            id: block
                            readonly property int blockIndex: index
                            readonly property real labelWidth: Math.round(42 * page.uiScale)
                            readonly property real cellWidth: Math.max(1,
                                (width - labelWidth - chart.cellGap * 5) / 5)
                            width: (chart.width - chart.blockGap * (chart.columnCount - 1)) / chart.columnCount
                            spacing: chart.cellGap
                            Row {
                                spacing: chart.cellGap
                                height: chart.headingHeight
                                Item { width: block.labelWidth; height: 1 }
                                Repeater {
                                    model: ["a", "i", "u", "e", "o"]
                                    Text {
                                        objectName: "vowel_" + block.blockIndex + "_" + modelData
                                        width: block.cellWidth
                                        horizontalAlignment: Text.AlignHCenter
                                        text: modelData; color: "#65766a"
                                        font.pixelSize: Math.round(15 * page.uiScale)
                                    }
                                }
                            }
                            Repeater {
                                model: chart.rows.slice(block.blockIndex * chart.rowsPerBlock,
                                    (block.blockIndex + 1) * chart.rowsPerBlock)
                                Row {
                                    id: kanaRow
                                    property var rowData: modelData
                                    spacing: chart.cellGap
                                    height: chart.rowHeight
                                    Text {
                                        width: block.labelWidth; height: parent.height
                                        text: kanaRow.rowData.name; color: "#65766a"
                                        font.pixelSize: Math.round(14 * page.uiScale)
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    Repeater {
                                        model: kanaRow.rowData.cells
                                        Item {
                                            width: block.cellWidth; height: kanaRow.height
                                            ActionButton {
                                                id: kanaButton
                                                objectName: modelData.id ? "kana_" + modelData.id : ""
                                                anchors.fill: parent
                                                visible: !!modelData.id
                                                verticalPadding: 1; horizontalPadding: 2
                                                property bool isCurrent: modelData.id === backend.selected.id
                                                text: modelData.character || ""
                                                Accessible.name: text
                                                onClicked: backend.selectKana(modelData.id)
                                                background: Rectangle {
                                                    radius: 8
                                                    color: kanaButton.isCurrent ? "#e5eee4" : kanaButton.hovered ? "#f0f4ed" : "#f8f9f5"
                                                    border.color: kanaButton.activeFocus ? "#2d6152" : kanaButton.isCurrent ? "#91ae8c" : "#edf0e8"
                                                    Behavior on color { ColorAnimation { duration: 120 } }
                                                }
                                                contentItem: Item {
                                                    Text {
                                                        width: parent.width
                                                        height: parent.height - (roman.visible ? roman.height : 0)
                                                        text: modelData.character || ""
                                                        font.pixelSize: Math.round(31 * page.uiScale)
                                                        minimumPixelSize: 18
                                                        fontSizeMode: Text.Fit
                                                        color: kanaButton.isCurrent ? "#2d6152" : "#34453b"
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    Text {
                                                        id: roman
                                                        anchors.bottom: parent.bottom
                                                        width: parent.width; visible: page.showRomaji
                                                        text: modelData.romaji || ""; color: "#65766a"
                                                        font.pixelSize: Math.round(14 * page.uiScale)
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
        }
        Rectangle {
            objectName: "pronunciationDetails"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(64 * page.uiScale, selectedCharacter.implicitHeight + 20,
                replay.implicitHeight + 20, detailNote.implicitHeight + 20)
            radius: 12; color: "#e9eee2"; border.color: "#dce4d3"
            RowLayout {
                id: detailRow
                anchors.fill: parent
                anchors.margins: 10
                spacing: page.gap
                Text {
                    id: selectedCharacter
                    text: backend.selected.character
                    font.pixelSize: Math.round((page.height < 650 ? 32 : 40) * page.uiScale); color: "#2e5340"
                }
                Text {
                    objectName: "selectedRomaji"
                    visible: page.showRomaji
                    text: backend.selected.romaji; color: "#526e57"
                    font.pixelSize: Math.round(20 * page.uiScale)
                }
                Text {
                    text: (backend.katakana ? "平假名  " : "片假名  ") + backend.selected.counterpart
                    color: "#5e735f"; font.pixelSize: Math.round(16 * page.uiScale)
                }
                ActionButton {
                    id: replay
                    objectName: "replayButton"
                    primary: true
                    text: backend.playing ? "正在播放…" : "播放原音"
                    onClicked: backend.replay()
                }
                Text {
                    id: detailNote
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: backend.selected.note
                    wrapMode: Text.Wrap; color: "#51684f"
                    font.pixelSize: Math.round(14 * page.uiScale)
                }
            }
        }
    }
}
