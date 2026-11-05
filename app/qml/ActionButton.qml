import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: control
    property bool primary: false
    property bool selected: false
    implicitHeight: 40
    implicitWidth: Math.max(92, implicitContentWidth + 32)
    padding: 10
    horizontalPadding: 16
    hoverEnabled: true
    font.pixelSize: 13
    font.weight: Font.Medium
    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? "#a5aaa6" : control.primary ? "#ffffff" : control.selected ? "#245d4f" : "#505b55"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 9
        color: !control.enabled ? "#edf0ec" : control.primary ? (control.down ? "#204b40" : control.hovered ? "#3d7462" : "#2d6152")
               : control.selected ? "#e4eee6" : control.hovered ? "#e9ede6" : "#f5f6f1"
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? "#618c77" : control.primary ? "transparent" : "#dce2d8"
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
