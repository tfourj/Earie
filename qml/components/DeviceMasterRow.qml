import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../styles" as Styles

Item {
    id: root
    property var deviceObject
    readonly property real peak01: deviceObject ? deviceObject.peak : 0
    property bool _wheelAdjusting: false

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

        // Wrap slider + activity meter so the meter doesn't participate in RowLayout sizing.
        Item {
            id: sliderWrap
            Layout.fillWidth: true
            Layout.preferredHeight: slider.implicitHeight

            Styles.SliderStyle {
                id: slider
                anchors.fill: parent
                accentColor: (deviceObject && deviceObject.muted) ? "#6A6F78" : theme.accent
                inactiveColor: (deviceObject && deviceObject.muted) ? "#3A3D44" : theme.trackInactive
                // Avoid jitter: while dragging, slider owns its own value, but we still
                // send live volume updates for audible feedback.
                onMoved: if (deviceObject) deviceObject.setVolume(value)

                Component.onCompleted: {
                    if (deviceObject) slider.value = deviceObject.volume
                }

                Connections {
                    target: deviceObject
                    function onChanged() {
                        if (!slider.pressed && !root._wheelAdjusting && deviceObject) {
                            slider.value = deviceObject.volume
                        }
                    }
                }
            }

            Timer {
                id: wheelSyncHold
                interval: 120
                repeat: false
                onTriggered: root._wheelAdjusting = false
            }

            // Optional: adjust device volume with mouse wheel when hovering the slider.
            WheelHandler {
                enabled: appController && appController.scrollWheelVolumeOnHover
                target: null
                onWheel: function(ev) {
                    if (!enabled)
                        return
                    if (!deviceObject || slider.pressed)
                        return

                    var steps = 0
                    if (ev.angleDelta && ev.angleDelta.y) {
                        var raw = ev.angleDelta.y / 120
                        steps = raw > 0 ? Math.ceil(raw) : Math.floor(raw)
                    } else if (ev.pixelDelta && ev.pixelDelta.y) {
                        steps = ev.pixelDelta.y > 0 ? 1 : -1
                    }

                    if (steps === 0)
                        return

                    var next = slider.value + steps * 0.02
                    next = Math.max(0, Math.min(1, next))
                    slider.value = next
                    deviceObject.setVolume(next)
                    root._wheelAdjusting = true
                    wheelSyncHold.restart()
                    ev.accepted = true
                }
            }

            // Device activity meter (max of its sessions), EarTrumpet-like.
            Rectangle {
                id: peakLine
                readonly property real trackH: 7
                readonly property real lineH: trackH
                readonly property real trackY: slider.topPadding + Math.round((slider.availableHeight - trackH) / 2)

                x: slider.leftPadding
                y: trackY
                width: Math.max(0, slider.availableWidth * Math.min(peak01, slider.visualPosition))
                height: lineH
                radius: 4
                color: "#FFFFFF"
                opacity: 0.32
                visible: peak01 > 0.005
                z: 20

                Behavior on width {
                    NumberAnimation { duration: 70; easing.type: Easing.OutQuad }
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


