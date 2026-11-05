import QtQuick 2.15
import QtQuick.Controls 2.15

ComboBox {
    id: control
    implicitWidth: 126
    implicitHeight: 38
    leftPadding: 13
    rightPadding: 30
    font.pixelSize: 12
    hoverEnabled: true
    contentItem: Text {
        text: control.displayText
        font: control.font
        color: "#46564c"
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Text {
        x: control.width - 24
        y: (control.height - height) / 2
        text: "⌄"
        color: "#6c7d70"
        font.pixelSize: 16
    }
    background: Rectangle {
        radius: 8
        color: control.hovered ? "#f0f3ec" : "#fafbf7"
        border.color: control.activeFocus ? "#618c77" : "#dce2d8"
    }
    delegate: ItemDelegate {
        width: control.width
        text: modelData
        font: control.font
        highlighted: control.highlightedIndex === index
    }
    popup: Popup {
        y: control.height + 5
        width: control.width
        padding: 4
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 320)
        background: Rectangle { radius: 9; color: "#ffffff"; border.color: "#dce2d8" }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }
}
