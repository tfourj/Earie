import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../styles" as Styles
import "../styles/SliderStyle.qml" as SliderStyle

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

        SliderStyle {
            id: slider
            Layout.fillWidth: true
            value: deviceObject ? deviceObject.volume : 0
            onMoved: if (deviceObject) deviceObject.setVolume(value)
        }

        Text {
            id: pct
            Layout.preferredWidth: 46
            horizontalAlignment: Text.AlignRight
            color: theme.textMuted
            font.pixelSize: 13
            text: {
                const v = deviceObject ? deviceObject.volume : 0
                return Math.round(v * 100) + "%"
            }
        }
    }
}


