import QtQuick
import QtQuick.Controls

import "." as Styles

Slider {
    id: slider

    Styles.Theme { id: theme }

    property color accentColor: theme.accent
    property color inactiveColor: theme.trackInactive

    from: 0
    to: 1
    stepSize: 0.001

    height: 26

    background: Item {
        implicitHeight: 26
        implicitWidth: 220

        Rectangle {
            id: track
            x: 0
            width: parent.width
            height: 4
            radius: 2
            y: Math.round((parent.height - height) / 2) - 2
            color: slider.inactiveColor
        }

        // Thin accent line under the track (EarTrumpet-like).
        Rectangle {
            id: accentLine
            x: 0
            y: track.y + track.height + 4
            width: Math.max(2, track.width * slider.visualPosition)
            height: 2
            radius: 1
            color: slider.enabled ? slider.accentColor : "#6A6F78"
            opacity: 0.95
        }
    }

    handle: Rectangle {
        width: 10
        height: 10
        radius: 5
        color: "transparent" // EarTrumpet hides the thumb visually; keep hitbox via Slider internals.
        border.color: "transparent"
    }
}


