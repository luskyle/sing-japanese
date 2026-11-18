pragma Singleton
import QtQuick 2.15

QtObject {
    readonly property bool dark: backend.darkMode
    readonly property color window: dark ? "#151b19" : "#f4f3ec"
    readonly property color header: dark ? "#1b2420" : "#fcfcf8"
    readonly property color surface: dark ? "#1f2a25" : "#ffffff"
    readonly property color button: dark ? "#27352e" : "#f5f6f1"
    readonly property color hover: dark ? "#34473b" : "#e9ede6"
    readonly property color selected: dark ? "#304c40" : "#e4eee6"
    readonly property color selectedText: dark ? "#b5e0c3" : "#245d4f"
    readonly property color text: dark ? "#e4eee6" : "#34453b"
    readonly property color heading: dark ? "#edf5ef" : "#243d32"
    readonly property color secondary: dark ? "#b6c8bc" : "#5e6d61"
    readonly property color border: dark ? "#43574a" : "#e1e6dc"
    readonly property color divider: dark ? "#35473c" : "#edf0e9"
    readonly property color accent: dark ? "#8fc4a5" : "#2d6152"
    readonly property color primary: dark ? "#397b65" : "#2d6152"
    readonly property color primaryHover: dark ? "#3d806a" : "#3d7462"
    readonly property color primaryPressed: dark ? "#306b58" : "#204b40"
    readonly property color primaryText: "#ffffff"
    readonly property color disabledSurface: dark ? "#27312c" : "#edf0ec"
    readonly property color disabledText: dark ? "#8b9b90" : "#a5aaa6"
    readonly property color details: dark ? "#25382e" : "#e9eee2"
    readonly property color cell: dark ? "#243229" : "#f8f9f5"
    readonly property color cellHover: dark ? "#32483a" : "#f0f4ed"
    readonly property color cellSelected: dark ? "#365641" : "#e5eee4"
    readonly property color selectedBorder: dark ? "#83b594" : "#91ae8c"
    readonly property color paperWell: dark ? "#111713" : "#edf0ea"
    readonly property color errorSurface: dark ? "#422a25" : "#fff1ea"
    readonly property color errorText: dark ? "#ffc4b2" : "#9b4935"
}
