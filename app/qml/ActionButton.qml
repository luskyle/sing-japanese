import QtQuick 2.15
import QtQuick.Controls 2.15
import SingJapanese 1.0

Button {
    id: control
    property bool primary: false
    property bool selected: false
    property bool compact: false
    readonly property real uiScale: ApplicationWindow.window ? ApplicationWindow.window.uiScale : 1
    implicitHeight: Math.max(Math.round((compact ? 34 : 46) * uiScale), implicitContentHeight + verticalPadding * 2)
    implicitWidth: Math.max(Math.round((compact ? 78 : 100) * uiScale), implicitContentWidth + horizontalPadding * 2)
    verticalPadding: Math.round((compact ? 5 : 10) * uiScale)
    horizontalPadding: Math.round((compact ? 10 : 16) * uiScale)
    hoverEnabled: true
    font.pixelSize: Math.round((compact ? 14 : 16) * uiScale)
    font.weight: Font.Medium
    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? Theme.disabledText : control.primary ? Theme.primaryText : control.selected ? Theme.selectedText : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 9
        color: !control.enabled ? Theme.disabledSurface : control.primary ? (control.down ? Theme.primaryPressed : control.hovered ? Theme.primaryHover : Theme.primary)
               : control.selected ? Theme.selected : control.hovered ? Theme.hover : Theme.button
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : control.primary ? "transparent" : Theme.border
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
