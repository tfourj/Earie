import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../styles" as Styles

Item {
    id: root
    property var sessionObject
    readonly property real peak01: sessionObject ? sessionObject.peak : 0

    Styles.Theme { id: theme }

    height: 34
    opacity: sessionObject && sessionObject.active === false ? 0.82 : 1.0

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

        // Wrap slider + activity meter so the meter DOES NOT participate in RowLayout sizing
        // (prevents slider width jitter when audio starts/stops).
        Item {
            id: sliderWrap
            Layout.fillWidth: true
            Layout.preferredHeight: slider.implicitHeight

            Styles.SliderStyle {
                id: slider
                anchors.fill: parent
                onMoved: if (sessionObject) sessionObject.setVolume(value)

                Component.onCompleted: {
                    if (sessionObject) slider.value = sessionObject.volume
                }

                Connections {
                    target: sessionObject
                    function onChanged() {
                        if (!slider.pressed && sessionObject) {
                            slider.value = sessionObject.volume
                        }
                    }
                }
            }

            // EarTrumpet-like activity meter (thin white line) OVERLAYING the slider track.
            // Track geometry matches `qml/styles/SliderStyle.qml` (track height 7px).
            Rectangle {
                id: peakLine
                readonly property real trackH: 7
                // Same thickness as the blue indicator/track, but translucent.
                readonly property real lineH: trackH
                readonly property real trackY: slider.topPadding + Math.round((slider.availableHeight - trackH) / 2)

                x: slider.leftPadding
                y: trackY
                // Clip to the current volume anchor so lowering volume doesn't "jump" the meter past the handle.
                width: Math.max(0, slider.availableWidth * Math.min(peak01, slider.visualPosition))
                height: lineH
                radius: 4
                color: "#FFFFFF"
                opacity: 0.22
                visible: peak01 > 0.005
                z: 20

                Behavior on width {
                    NumberAnimation { duration: 70; easing.type: Easing.OutQuad }
                }
            }
        }

        Text {
            Layout.preferredWidth: 46
            horizontalAlignment: Text.AlignRight
            color: theme.textMuted
            font.pixelSize: 13
            text: {
                return Math.round(slider.value * 100) + "%"
            }
        }
    }

    ToolTip.visible: mouse.containsMouse && sessionObject
    ToolTip.delay: 450
    ToolTip.text: sessionObject
                  ? (sessionObject.displayName + "\n" + sessionObject.exePath + "\nPID: " + sessionObject.pid)
                  : ""
}


