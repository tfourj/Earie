import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "styles" as Styles
import "components"

Item {
    id: root
    width: 420
    // Height is controlled by C++ (auto-fit to content, clamped to screen).
    // Keep a reasonable implicit baseline for initial show.
    implicitHeight: 420

    // C++ reads this to auto-size the flyout height.
    // Includes: margins (12*2) + header row + spacing + list content + bottom padding.
    readonly property int contentHeightHint: Math.ceil(
        12*2
        + Math.max(topRow.implicitHeight, topRow.height)
        + 10
        + listView.contentHeight
        + 4
    )

    Styles.Theme { id: theme }

    Rectangle {
        anchors.fill: parent
        radius: theme.radius
        color: Qt.rgba(0, 0, 0, 0) // let acrylic show through; QML draws cells/panel.
        border.color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        clip: true

        Rectangle {
            anchors.fill: parent
            radius: theme.radius
            color: Qt.rgba(0x1D / 255, 0x1F / 255, 0x22 / 255, 0.80)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout {
            id: topRow
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                color: theme.text
                font.pixelSize: 14
                text: "Earie"
            }

            Text {
                id: modeText
                color: theme.textMuted
                font.pixelSize: 12
                text: appController && appController.allDevices ? "All devices" : "Default device"
                opacity: modeMouse.containsMouse ? 1.0 : 0.9

                MouseArea {
                    id: modeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: if (appController) appController.allDevices = !appController.allDevices
                }
            }
        }

        // ListView is already a Flickable; we use it to support drag reordering.
        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 10
            boundsBehavior: Flickable.StopAtBounds
            model: deviceModel
            // Smoothly keep equal spacing while reordering.
            moveDisplaced: Transition {
                NumberAnimation { properties: "x,y"; duration: 120; easing.type: Easing.OutCubic }
            }

            // Drag feedback ("ghost") state
            property bool dragging: false
            property string draggingId: ""
            property var draggingDeviceObject: null
            property real dragGhostY: 0
            property int dragTargetIndex: -1

            function clearDragState() {
                dragging = false
                draggingId = ""
                draggingDeviceObject = null
                dragTargetIndex = -1
            }

            function indexNearY(y) {
                // ListView.indexAt returns -1 when over spacing/gaps; probe around the gap.
                const x = 10
                let idx = listView.indexAt(x, y)
                if (idx >= 0) return idx
                idx = listView.indexAt(x, y - listView.spacing / 2)
                if (idx >= 0) return idx
                idx = listView.indexAt(x, y + listView.spacing / 2)
                if (idx >= 0) return idx
                return -1
            }

            function targetIndexForMidY(midY) {
                if (listView.count <= 0)
                    return -1

                // Clamp to content bounds so dragging from bottom doesn't "wrap".
                const clampedY = Math.max(0, Math.min(midY, Math.max(0, listView.contentHeight - 1)))
                let idx = listView.indexNearY(clampedY)
                if (idx < 0)
                    idx = listView.count - 1

                // If we're past the midpoint of the hit item, insert after it.
                const item = listView.itemAtIndex(idx)
                if (item && clampedY > (item.y + item.height / 2))
                    idx = Math.min(idx + 1, listView.count - 1)

                return idx
            }

            // Floating ghost cell that follows the pointer while the real list stays snapped.
            Item {
                id: dragGhost
                visible: listView.dragging && listView.draggingDeviceObject
                z: 5000
                x: 0
                y: Math.max(0, Math.min(listView.dragGhostY, listView.height - height))
                width: listView.width
                height: ghostCell.implicitHeight

                DeviceCell {
                    id: ghostCell
                    anchors.fill: parent
                    deviceObject: listView.draggingDeviceObject
                    opacity: 0.92
                }
            }

            delegate: Item {
                id: row
                width: listView.width
                height: cell.implicitHeight
                property string deviceId: model.deviceId
                property var deviceObject: model.deviceObject

                // Drag the whole device cell to reorder (snap by moving the model).
                // IMPORTANT: we do NOT move the delegate itself (prevents overlap).
                property int _lastTargetIndex: -1
                property real _dragStartY: 0

                DeviceCell {
                    id: cell
                    anchors.fill: parent
                    deviceObject: model.deviceObject
                    // Hide the original while we show a ghost on top.
                    opacity: (listView.dragging && listView.draggingId === row.deviceId) ? 0.15 : 1.0
                }

                // Subtle placeholder highlight at the current target slot.
                Rectangle {
                    anchors.fill: parent
                    radius: 12
                    color: "transparent"
                    border.width: (listView.dragging
                                   && listView.dragTargetIndex === index
                                   && listView.draggingId !== row.deviceId) ? 1 : 0
                    border.color: Qt.rgba(0.23, 0.59, 1.0, 0.55)
                }

                DragHandler {
                    id: dragHandler
                    target: null
                    xAxis.enabled: false
                    yAxis.enabled: true

                    onActiveChanged: {
                        if (active) {
                            row.z = 1000
                            row._lastTargetIndex = -1
                            row._dragStartY = row.y
                            listView.dragging = true
                            listView.draggingId = row.deviceId
                            listView.draggingDeviceObject = row.deviceObject
                            listView.dragTargetIndex = index
                            listView.dragGhostY = row.y
                        } else {
                            row.z = 0
                            row._lastTargetIndex = -1
                            if (listView.draggingId === row.deviceId) {
                                listView.clearDragState()
                            }
                        }
                    }

                    onTranslationChanged: {
                        // Use the fixed drag start Y (row.y changes as the model reorders).
                        const ghostY = row._dragStartY + dragHandler.translation.y
                        const midY = ghostY + row.height / 2
                        if (listView.draggingId === row.deviceId) {
                            listView.dragGhostY = ghostY
                        }
                        const idx = listView.targetIndexForMidY(midY)
                        if (idx >= 0 && idx !== index && idx !== row._lastTargetIndex) {
                            row._lastTargetIndex = idx
                            if (listView.draggingId === row.deviceId) {
                                listView.dragTargetIndex = idx
                            }
                            if (audioBackend) audioBackend.moveDeviceToIndex(row.deviceId, idx)
                        }
                    }
                }

                Component.onDestruction: {
                    // If the delegate is recycled while dragging, ensure we clear the ghost.
                    if (listView.draggingId === row.deviceId) {
                        listView.clearDragState()
                    }
                }
            }
        }
    }
}


