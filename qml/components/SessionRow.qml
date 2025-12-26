import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../styles" as Styles

Item {
    id: root
    property var sessionObject

    Styles.Theme { id: theme }

    height: 34

    Rectangle {
        anchors.fill: parent
        radius: 10
            color: mouse.containsMouse ? theme.cellHover : "transparent"
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }

    RowLayout {
        anchors.fill: parent
        spacing: 10

        Item {
            Layout.preferredWidth: 26
            Layout.preferredHeight: 26

            Image {
                anchors.centerIn: parent
                width: 20
                height: 20
                smooth: true
                source: sessionObject && sessionObject.iconKey ? ("image://appicon/" + encodeURIComponent(sessionObject.iconKey)) : ""
            }
        }

        ToolButton {
            id: muteBtn
            Layout.preferredWidth: 22
            Layout.preferredHeight: 22
            padding: 0

            font.family: theme.iconFont
            font.pixelSize: 14
            text: sessionObject && sessionObject.muted ? theme.glyphMute : theme.glyphSpeaker

            background: Rectangle {
                radius: 8
                color: muteBtn.hovered ? "#2E3136" : "transparent"
            }
            onClicked: if (sessionObject) sessionObject.toggleMute()
        }

        Styles.SliderStyle {
            id: slider
            Layout.fillWidth: true
            value: sessionObject ? sessionObject.volume : 0
            onMoved: if (sessionObject) sessionObject.setVolume(value)
        }

        Text {
            Layout.preferredWidth: 46
            horizontalAlignment: Text.AlignRight
            color: theme.textMuted
            font.pixelSize: 13
            text: {
                const v = sessionObject ? sessionObject.volume : 0
                return Math.round(v * 100) + "%"
            }
        }
    }

    ToolTip.visible: mouse.containsMouse && sessionObject
    ToolTip.delay: 450
    ToolTip.text: sessionObject
                  ? (sessionObject.displayName + "\n" + sessionObject.exePath + "\nPID: " + sessionObject.pid)
                  : ""
}


