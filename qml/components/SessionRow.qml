import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import "../styles" as Styles

Item {
    id: root
    property var sessionObject
    readonly property real peak01: sessionObject ? sessionObject.peak : 0
    property bool _wheelAdjusting: false

    Styles.Theme { id: theme }

    function positionIconTipAtCursor() {
        // Use global cursor position from C++ so it works even when the window isn't capturing mouse moves.
        if (!appController)
            return

        const p = appController.cursorPos()
        const avail = appController.cursorScreenAvailableGeometry()
        const w = iconTip.width
        const h = iconTip.height
        const margin = 10

        const xWanted = Math.round(p.x - w - margin)
        const yWanted = Math.round(p.y - h - margin)

        iconTip.x = Math.max(avail.x, Math.min(avail.x + avail.width - w, xWanted))
        iconTip.y = Math.max(avail.y, Math.min(avail.y + avail.height - h, yWanted))
    }

    height: 34
    opacity: sessionObject && sessionObject.active === false ? 0.82 : 1.0

    // Unified hover state for the entire row (works even when hovering child controls like the slider/icon).
    HoverHandler {
        id: hover
    }

    Rectangle {
        anchors.fill: parent
        // Slight inset so the hover highlight doesn't butt up against the cell edges.
        anchors.margins: 1
        radius: 10
        color: hover.hovered ? theme.cellHover : "transparent"
    }

    RowLayout {
        id: contentRow
        anchors.fill: parent
        // Inner padding so content (esp. the % text) isn't flush with the right edge.
        anchors.leftMargin: 6
        anchors.rightMargin: 10
        spacing: 10
        opacity: 1.0

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

            MouseArea {
                id: iconMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onEntered: {
                    if (appController && appController.showProcessStatusOnHover && sessionObject) {
                        iconHoverTimer.restart()
                    }
                }
                onExited: {
                    iconHoverTimer.stop()
                    iconTip.close()
                }
                onPositionChanged: {
                    // Keep the tooltip near the cursor while it's visible.
                    if (iconTip.visible) {
                        root.positionIconTipAtCursor()
                    }
                }
                onClicked: function(mouse) {
                    if (!sessionObject) return
                    if (mouse.button === Qt.RightButton) {
                        ctxMenu.popup()
                    } else {
                        sessionObject.toggleMute()
                    }
                }
            }

            StyledMenu {
                id: ctxMenu
                StyledMenuItem {
                    text: "Hide globally"
                    onTriggered: {
                        if (appController && sessionObject) {
                            appController.setProcessHiddenGlobal(sessionObject.exePath, true)
                        }
                    }
                }
                StyledMenuItem {
                    text: "Hide on this device"
                    onTriggered: {
                        if (appController && sessionObject) {
                            appController.setProcessHiddenForDevice(sessionObject.deviceId, sessionObject.exePath, true)
                        }
                    }
                }
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
                accentColor: (sessionObject && sessionObject.muted) ? "#6A6F78" : theme.accent
                inactiveColor: (sessionObject && sessionObject.muted) ? "#3A3D44" : theme.trackInactive
                onMoved: if (sessionObject) sessionObject.setVolume(value)

                Component.onCompleted: {
                    if (sessionObject) slider.value = sessionObject.volume
                }

                Connections {
                    target: sessionObject
                    function onChanged() {
                        if (!slider.pressed && !root._wheelAdjusting && sessionObject) {
                            slider.value = sessionObject.volume
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

            // Optional: adjust volume with mouse wheel when hovering the slider.
            WheelHandler {
                id: wheel
                enabled: appController && appController.scrollWheelVolumeOnHover
                target: null
                onWheel: function(ev) {
                    if (!enabled)
                        return
                    if (!sessionObject || slider.pressed)
                        return

                    var steps = 0
                    if (ev.angleDelta && ev.angleDelta.y) {
                        var raw = ev.angleDelta.y / 120
                        steps = raw > 0 ? Math.ceil(raw) : Math.floor(raw)
                    } else if (ev.pixelDelta && ev.pixelDelta.y) {
                        // Trackpads can report small pixel deltas; treat any non-zero as one step.
                        steps = ev.pixelDelta.y > 0 ? 1 : -1
                    }

                    if (steps === 0)
                        return

                    var next = slider.value + steps * 0.02
                    next = Math.max(0, Math.min(1, next))
                    slider.value = next
                    sessionObject.setVolume(next)
                    root._wheelAdjusting = true
                    wheelSyncHold.restart()
                    ev.accepted = true
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
                opacity: 0.32
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

    // Tooltip-only: show process name after a longer hover on the icon.
    // Custom popup so we can match StyledMenu visuals + position at the cursor.
    Timer {
        id: iconHoverTimer
        interval: 2000
        repeat: false
        onTriggered: {
            if (!(appController && appController.showProcessStatusOnHover))
                return
            if (!sessionObject || !iconMouse.containsMouse)
                return
            root.positionIconTipAtCursor()
            iconTip.open()
        }
    }

    Window {
        id: iconTip
        flags: Qt.ToolTip | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        color: "transparent"
        visible: false
        width: tipBg.implicitWidth
        height: tipBg.implicitHeight

        function open() { visible = true }
        function close() { visible = false }

        Rectangle {
            id: tipBg
            anchors.fill: parent
            implicitWidth: Math.min(320, tipText.implicitWidth + 16)
            implicitHeight: tipText.implicitHeight + 16
            radius: 10
            // Same shape as StyledMenu, but lighter / more transparent.
            color: Qt.rgba(0x20 / 255, 0x22 / 255, 0x26 / 255, 0.55)
            border.color: Qt.rgba(1, 1, 1, 0.10)
            border.width: 1

            Text {
                id: tipText
                anchors.fill: parent
                anchors.margins: 8
                text: sessionObject ? sessionObject.displayName : ""
                color: theme.text
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }
    }
}


