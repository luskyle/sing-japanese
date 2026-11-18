import QtQuick 2.15
import QtQuick.Controls 2.15
import SingJapanese 1.0

CheckBox {
    readonly property real uiScale: ApplicationWindow.window ? ApplicationWindow.window.uiScale : 1
    font.pixelSize: Math.round(16 * uiScale)
    palette.window: Theme.window
    palette.windowText: Theme.text
    palette.text: Theme.text
    palette.buttonText: Theme.text
    palette.base: Theme.surface
    palette.button: Theme.surface
    palette.light: Theme.hover
    palette.mid: Theme.border
    palette.dark: Theme.border
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.window
}
