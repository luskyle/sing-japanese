import QtQuick 2.15
import QtQuick.Controls 2.15
import SingJapanese 1.0

ComboBox {
    id: control
    readonly property real uiScale: ApplicationWindow.window ? ApplicationWindow.window.uiScale : 1
    implicitWidth: Math.max(Math.round(150 * uiScale), label.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(Math.round(46 * uiScale), label.implicitHeight + 20)
    leftPadding: Math.round(14 * uiScale)
    rightPadding: Math.round(34 * uiScale)
    font.pixelSize: Math.round(16 * uiScale)
    hoverEnabled: true
    contentItem: Text {
        id: label
        text: control.displayText
        font: control.font
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Text {
        x: control.width - Math.round(26 * control.uiScale)
        y: (control.height - height) / 2
        text: "⌄"
        color: Theme.secondary
        font.pixelSize: Math.round(18 * control.uiScale)
    }
    background: Rectangle {
        radius: 8
        color: control.hovered ? Theme.hover : Theme.surface
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
    delegate: ItemDelegate {
        width: control.width
        text: modelData
        font: control.font
        highlighted: control.highlightedIndex === index
        palette.text: Theme.text
        palette.highlightedText: Theme.selectedText
        background: Rectangle {
            color: parent.highlighted ? Theme.selected : parent.hovered ? Theme.hover : Theme.surface
        }
    }
    popup: Popup {
        y: control.height + 5
        width: control.width
        padding: 4
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 320)
        background: Rectangle { radius: 9; color: Theme.surface; border.color: Theme.border }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }
}
