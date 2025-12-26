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

    height: 30
    leftPadding: 8
    rightPadding: 8

    background: Item {
        implicitHeight: 30
        implicitWidth: 220

        Rectangle {
            id: track
            x: slider.leftPadding
            width: slider.availableWidth
            height: 7
            radius: 4
            y: slider.topPadding + Math.round((slider.availableHeight - height) / 2)
            color: slider.inactiveColor
        }

        // Blue filled portion overlaying the grey track.
        Rectangle {
            id: fill
            x: track.x
            y: track.y
            width: Math.max(0, track.width * slider.visualPosition)
            height: track.height
            radius: track.radius
            color: slider.enabled ? slider.accentColor : "#6A6F78"
            opacity: 0.95
        }
    }

    handle: Rectangle {
        // Visible anchor/ball to show the current position.
        width: 14
        height: 14
        radius: 7
        color: slider.enabled ? slider.accentColor : "#6A6F78"
        border.width: 1
        border.color: "#D7DCE3"

        // Explicitly position the thumb so it matches the track/fill (prevents drift).
        x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
        y: slider.topPadding + (slider.availableHeight - height) / 2
    }
}


