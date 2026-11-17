import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import SingJapanese 1.0

Item {
    id: page
    property real uiScale: 1
    readonly property bool narrow: width < 760
    readonly property int margin: height < 650 ? 16 : 24
    readonly property int gap: Math.round(12 * uiScale)
    property bool actualPaperSize: false
    onActualPaperSizeChanged: {
        inputScroll.contentX = 0
        output.contentX = 0
    }

    AnnotationView {
        id: preview
        objectName: "annotationPreview"
        visible: false
        width: printableWidth
        // Pages paint this layout separately; the hidden source must not allocate a document-sized surface.
        height: 1
        segments: backend.segments
        romaji: backend.romaji
        fullRomaji: backend.fullRomaji
        fontScale: backend.previewFontScale
    }

    Binding {
        target: backend
        property: "previewWidth"
        value: preview.printableWidth
    }

    Item {
        objectName: "lyricsWorkspace"
        anchors.fill: parent

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: page.margin
            spacing: Math.round(8 * page.uiScale)

            Flow {
                Layout.fillWidth: true
                spacing: Math.round(8 * page.uiScale)
                CheckBox {
                    objectName: "romajiOption"
                    text: "汉字加注罗马音"
                    checked: backend.romaji
                    onToggled: backend.romaji = checked
                    font.pixelSize: Math.round(16 * page.uiScale)
                    palette.highlight: "#2d6152"
                }
                CheckBox {
                    objectName: "fullRomajiOption"
                    text: "全部内容加注罗马音"
                    checked: backend.fullRomaji
                    onToggled: backend.fullRomaji = checked
                    font.pixelSize: Math.round(16 * page.uiScale)
                    palette.highlight: "#2d6152"
                }
                ActionButton {
                    objectName: "paperZoomButton"
                    compact: true
                    text: page.actualPaperSize ? "适合宽度" : "放大到 100%"
                    onClicked: page.actualPaperSize = !page.actualPaperSize
                }
            }
            GridLayout {
                id: panels
                objectName: "lyricsPanels"
                columns: page.narrow ? 1 : 2
                Layout.fillWidth: true; Layout.fillHeight: true
                Layout.minimumHeight: 0
                columnSpacing: page.gap; rowSpacing: page.gap
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Layout.minimumWidth: 0
                    Layout.minimumHeight: 0
                    Layout.preferredHeight: Math.round(400 * page.uiScale)
                    radius: 16; color: "#ffffff"; border.color: "#e1e6dc"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Math.round(16 * page.uiScale)
                        spacing: page.gap
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "原文"; font.pixelSize: Math.round(18 * page.uiScale)
                                font.weight: Font.DemiBold; color: "#364b3c"
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: Array.from(editor.text).length + " / 20,000"
                                color: "#65766a"; font.pixelSize: Math.round(14 * page.uiScale)
                            }
                        }
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#edf0e9" }
                        Flickable {
                            id: inputScroll
                            objectName: "originalPaperScroll"
                            Layout.fillWidth: true; Layout.fillHeight: true
                            Layout.minimumHeight: 0
                            clip: true
                            readonly property real paperZoom: page.actualPaperSize ? 1
                                : Math.min(1, Math.max(0.1, (width - 16) / preview.paperWidth))
                            contentWidth: Math.max(width, originalPaper.width * paperZoom + 4)
                            contentHeight: originalPaper.height * paperZoom + 4
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

                            Rectangle {
                                x: inputScroll.contentX; y: inputScroll.contentY
                                width: inputScroll.width; height: inputScroll.height
                                color: "#edf0ea"; z: -1
                            }

                            function ensureCursorVisible() {
                                if (!editor.activeFocus)
                                    return
                                const cursor = editor.cursorRectangle
                                const left = originalPaper.x + (editor.x + cursor.x) * paperZoom
                                const top = originalPaper.y + (editor.y + cursor.y) * paperZoom
                                const right = left + Math.max(2, cursor.width * paperZoom)
                                const bottom = top + cursor.height * paperZoom
                                if (left < contentX)
                                    contentX = Math.max(0, left - 12)
                                else if (right > contentX + width)
                                    contentX = Math.min(contentWidth - width, right - width + 12)
                                if (top < contentY)
                                    contentY = Math.max(0, top - 12)
                                else if (bottom > contentY + height)
                                    contentY = Math.min(contentHeight - height, bottom - height + 12)
                            }

                            Rectangle {
                                id: originalPaper
                                objectName: "originalPaper"
                                x: Math.max(2, (inputScroll.width - width * scale) / 2)
                                y: 2
                                width: preview.paperWidth
                                height: Math.max(preview.paperHeight,
                                    editor.y + editor.height + preview.paperMarginTop + preview.contentPadding)
                                scale: inputScroll.paperZoom
                                transformOrigin: Item.TopLeft
                                color: "#ffffff"; border.color: "#d2dad2"
                                TextArea {
                                    id: editor
                                    objectName: "lyricsInput"
                                    x: preview.paperMarginLeft + preview.contentPadding
                                    y: preview.paperMarginTop + preview.contentPadding
                                    width: preview.printableWidth - 2 * preview.contentPadding
                                    height: Math.max(preview.paperHeight - 2 * y, contentHeight)
                                    text: backend.input
                                    onTextChanged: backend.input = text
                                    onCursorRectangleChanged: inputScroll.ensureCursorVisible()
                                    onActiveFocusChanged: if (activeFocus) inputScroll.ensureCursorVisible()
                                    placeholderText: "在这里粘贴日语歌词…\n\n注音结果会显示在右侧。"
                                    placeholderTextColor: "#839184"
                                    wrapMode: TextEdit.Wrap
                                    selectByMouse: true
                                    color: "#3d493e"; selectionColor: "#d6e5d1"; selectedTextColor: "#243d32"
                                    font.family: "Noto Sans CJK JP"; font.pixelSize: 16
                                    padding: 0
                                    background: Item {}
                                }
                            }
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Layout.minimumWidth: 0
                    Layout.minimumHeight: 0
                    Layout.preferredHeight: Math.round(400 * page.uiScale)
                    radius: 16; color: "#ffffff"; border.color: "#e1e6dc"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Math.round(16 * page.uiScale)
                        spacing: page.gap
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "注音稿"; font.pixelSize: Math.round(18 * page.uiScale)
                                font.weight: Font.DemiBold; color: "#364b3c"
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#edf0e9" }
                        Item {
                            Layout.fillWidth: true; Layout.fillHeight: true
                            Layout.minimumHeight: 0
                            Rectangle { anchors.fill: parent; color: "#edf0ea" }
                            ListView {
                                id: output
                                objectName: "annotationScroll"
                                anchors.fill: parent; clip: true
                                readonly property real paperZoom: page.actualPaperSize ? 1
                                    : Math.min(1, Math.max(0.1, (width - 16) / preview.paperWidth))
                                contentWidth: Math.max(width, preview.paperWidth * paperZoom + 4)
                                model: preview.pageCount
                                spacing: 12
                                boundsBehavior: Flickable.StopAtBounds
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                                delegate: Item {
                                    width: Math.max(output.width, preview.paperWidth * output.paperZoom + 4)
                                    height: preview.paperHeight * output.paperZoom
                                    AnnotationPageView {
                                        objectName: "annotationPaper_" + index
                                        x: Math.max(2, (output.width - width * scale) / 2)
                                        width: preview.paperWidth
                                        height: preview.paperHeight
                                        scale: output.paperZoom
                                        transformOrigin: Item.TopLeft
                                        document: preview
                                        pageIndex: index
                                        Text {
                                            x: preview.paperMarginLeft + preview.contentPadding
                                            y: preview.paperMarginTop + preview.contentPadding
                                            visible: index === 0 && backend.segments.length === 0
                                            text: "注音结果将在这里显示"
                                            font.pixelSize: 16; color: "#839184"
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Flow {
                objectName: "lyricsActions"
                Layout.fillWidth: true
                spacing: Math.round(6 * page.uiScale)
                ActionButton {
                    objectName: "annotateButton"
                    compact: true
                    primary: true; text: "自动注音"
                    onClicked: backend.annotateText()
                }
                ActionButton {
                    objectName: "previewDocxButton"
                    compact: true
                    text: "预览 DOCX"
                    enabled: backend.canExport
                    onClicked: backend.previewDocx()
                }
                ActionButton {
                    objectName: "previewPdfButton"
                    compact: true
                    text: "预览 PDF"
                    enabled: backend.canExport
                    onClicked: backend.previewPdf()
                }
                ActionButton {
                    objectName: "copyWordButton"
                    compact: true
                    text: "复制到 Word"
                    enabled: backend.canExport
                    onClicked: backend.copyForWord()
                }
                ActionButton {
                    objectName: "exportDocxButton"
                    compact: true
                    text: "导出 DOCX"
                    enabled: backend.canExport
                    onClicked: backend.chooseDocx()
                }
                ActionButton {
                    objectName: "exportButton"
                    compact: true
                    text: "导出 PDF"
                    enabled: backend.canExport
                    onClicked: backend.choosePdf()
                }
            }
        }
    }
}
