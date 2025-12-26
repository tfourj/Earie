import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../styles" as Styles

Item {
    id: root
    property var deviceObject

    Styles.Theme { id: theme }

    height: 34

    RowLayout {
        anchors.fill: parent
        spacing: 10

        ToolButton {
            id: muteBtn
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            padding: 0

            font.family: theme.iconFont
            font.pixelSize: 16
            text: deviceObject && deviceObject.muted ? theme.glyphMute : theme.glyphSpeaker

            background: Rectangle {
                radius: 8
                color: muteBtn.hovered ? theme.cellHover : "transparent"
            }

            onClicked: if (deviceObject) deviceObject.toggleMute()
        }

        Styles.SliderStyle {
            id: slider
            Layout.fillWidth: true
            // Avoid jitter: while dragging, slider owns its own value, but we still
            // send live volume updates for audible feedback.
            onMoved: if (deviceObject) deviceObject.setVolume(value)

            Component.onCompleted: {
                if (deviceObject) slider.value = deviceObject.volume
            }

            Connections {
                target: deviceObject
                function onChanged() {
                    if (!slider.pressed && deviceObject) {
                        slider.value = deviceObject.volume
                    }
                }
            }
        }

        Text {
            id: pct
            Layout.preferredWidth: 46
            horizontalAlignment: Text.AlignRight
            color: theme.textMuted
            font.pixelSize: 13
            text: {
                return Math.round(slider.value * 100) + "%"
            }
        }
    }
}


