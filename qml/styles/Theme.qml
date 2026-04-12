import QtQuick

QtObject {
    readonly property color panelBg: "#1D1F22"
    readonly property color cellBg: "#26282C"
    readonly property color cellHover: "#2C2F34"
    readonly property color divider: "#34373D"
    readonly property color text: "#E6E6E6"
    readonly property color textMuted: "#A9ADB4"
    readonly property color accent: "#3A96FF"
    readonly property color trackInactive: "#3A3D43"
    readonly property color trackActive: "#3A96FF"

    readonly property int radius: 14
    readonly property int cellRadius: 12
    readonly property int cellPad: 12

    readonly property string iconFont: "Segoe MDL2 Assets"

    readonly property string glyphSpeaker: "\uE767"
    readonly property string glyphMute: "\uE74F"
    readonly property string glyphChevron: "\uE70D"
    readonly property string glyphSettings: "\uE713"
    readonly property string glyphEye: "\uE890"

    function withAlpha(colorValue, alpha) {
        return Qt.rgba(colorValue.r, colorValue.g, colorValue.b, alpha)
    }

    function blend(baseColor, tintColor, amount) {
        const t = Math.max(0, Math.min(1, amount))
        return Qt.rgba(
            baseColor.r + (tintColor.r - baseColor.r) * t,
            baseColor.g + (tintColor.g - baseColor.g) * t,
            baseColor.b + (tintColor.b - baseColor.b) * t,
            baseColor.a + (tintColor.a - baseColor.a) * t
        )
    }

    function presetColor(key) {
        switch (key) {
        case "rose": return "#D86A84"
        case "coral": return "#D9825B"
        case "amber": return "#C89A32"
        case "lime": return "#7FA443"
        case "mint": return "#46A182"
        case "teal": return "#3D95A5"
        case "blue": return "#4B84D9"
        case "indigo": return "#6C72D8"
        case "plum": return "#9A6ACB"
        case "slate": return "#6E8195"
        default: return accent
        }
    }

    function paletteModel() {
        return [
            { key: "", label: "Default", color: accent },
            { key: "rose", label: "Rose", color: presetColor("rose") },
            { key: "coral", label: "Coral", color: presetColor("coral") },
            { key: "amber", label: "Amber", color: presetColor("amber") },
            { key: "lime", label: "Lime", color: presetColor("lime") },
            { key: "mint", label: "Mint", color: presetColor("mint") },
            { key: "teal", label: "Teal", color: presetColor("teal") },
            { key: "blue", label: "Blue", color: presetColor("blue") },
            { key: "indigo", label: "Indigo", color: presetColor("indigo") },
            { key: "plum", label: "Plum", color: presetColor("plum") },
            { key: "slate", label: "Slate", color: presetColor("slate") }
        ]
    }

    function deviceAccent(colorKey) {
        return colorKey ? presetColor(colorKey) : accent
    }

    function deviceCardBg(colorKey) {
        return colorKey ? blend(cellBg, presetColor(colorKey), 0.20) : cellBg
    }

    function deviceCardHover(colorKey) {
        return colorKey ? blend(cellHover, presetColor(colorKey), 0.24) : cellHover
    }

    function deviceBorder(colorKey, isDefault) {
        if (!colorKey)
            return isDefault ? accent : divider
        return isDefault
            ? blend(accent, presetColor(colorKey), 0.55)
            : blend(divider, presetColor(colorKey), 0.42)
    }

    function deviceBadgeBg(colorKey, emphasized) {
        if (!colorKey)
            return emphasized
                ? Qt.rgba(accent.r, accent.g, accent.b, 0.18)
                : Qt.rgba(1, 1, 1, 0.08)
        return withAlpha(blend(cellBg, presetColor(colorKey), emphasized ? 0.48 : 0.30), emphasized ? 0.95 : 0.90)
    }

    function deviceBadgeBorder(colorKey, emphasized) {
        if (!colorKey)
            return emphasized
                ? Qt.rgba(accent.r, accent.g, accent.b, 0.55)
                : Qt.rgba(1, 1, 1, 0.12)
        return withAlpha(blend(divider, presetColor(colorKey), emphasized ? 0.72 : 0.52), emphasized ? 0.95 : 0.70)
    }

    function deviceBadgeText(colorKey, emphasized) {
        if (!colorKey)
            return emphasized ? accent : textMuted
        return emphasized ? text : blend(textMuted, presetColor(colorKey), 0.35)
    }
}
