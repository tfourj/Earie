import QtQuick

QtObject {
    readonly property color panelBg: "#1D1F22"
    readonly property color cellBg: "#26282C"
    readonly property color cellHover: "#2C2F34"
    readonly property color divider: "#34373D"
    readonly property color text: "#E6E6E6"
    readonly property color textMuted: "#A9ADB4"
    readonly property color accent: "#3A96FF" // Windows-ish blue
    readonly property color trackInactive: "#3A3D43"
    readonly property color trackActive: "#3A96FF"

    readonly property int radius: 14
    readonly property int cellRadius: 12
    readonly property int cellPad: 12

    readonly property string iconFont: "Segoe MDL2 Assets"

    // MDL2 glyphs (best-effort; they render on Win11 by default).
    readonly property string glyphSpeaker: "\uE767"
    readonly property string glyphMute: "\uE74F"
    readonly property string glyphChevron: "\uE70D"
}


