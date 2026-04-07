import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

import "styles" as Styles
import "components"

Item {
    id: root
    width: 420
    implicitHeight: 420

    readonly property int contentHeightHint: Math.ceil(
        12 * 2
        + Math.max(topRow.implicitHeight, topRow.height)
        + 10
        + scrollArea.contentHeight
        + 4
    )
    property bool trayIconExpanded: false

    function reopenSettingsMenu() {
        Qt.callLater(function() {
            if (settingsMenu)
                settingsMenu.open()
        })
    }

    onContentHeightHintChanged: {
        if (appController) appController.requestRelayout()
    }

    Styles.Theme { id: theme }

    component DeviceSectionView: ListView {
        id: sectionList
        required property bool isInputSection

        implicitHeight: contentHeight
        interactive: false
        clip: true
        spacing: 10
        boundsBehavior: Flickable.StopAtBounds
        moveDisplaced: Transition {
            NumberAnimation { properties: "x,y"; duration: 120; easing.type: Easing.OutCubic }
        }

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
            const x = 10
            let idx = sectionList.indexAt(x, y)
            if (idx >= 0) return idx
            idx = sectionList.indexAt(x, y - sectionList.spacing / 2)
            if (idx >= 0) return idx
            idx = sectionList.indexAt(x, y + sectionList.spacing / 2)
            if (idx >= 0) return idx
            return -1
        }

        function targetIndexForMidY(midY) {
            if (sectionList.count <= 0)
                return -1

            const clampedY = Math.max(0, Math.min(midY, Math.max(0, sectionList.contentHeight - 1)))
            let idx = sectionList.indexNearY(clampedY)
            if (idx < 0)
                idx = sectionList.count - 1

            const item = sectionList.itemAtIndex(idx)
            if (item && clampedY > (item.y + item.height / 2))
                idx = Math.min(idx + 1, sectionList.count - 1)

            return idx
        }

        Item {
            id: dragGhost
            visible: sectionList.dragging && sectionList.draggingDeviceObject
            z: 5000
            x: 0
            y: Math.max(0, Math.min(sectionList.dragGhostY, sectionList.height - height))
            width: sectionList.width
            height: ghostCell.implicitHeight

            DeviceCell {
                id: ghostCell
                anchors.fill: parent
                deviceObject: sectionList.draggingDeviceObject
                opacity: 0.92
            }
        }

        delegate: Item {
            id: row
            width: sectionList.width
            height: cell.implicitHeight
            property string deviceId: model.deviceId
            property var deviceObject: model.deviceObject
            property bool isInput: model.isInput
            property int _lastTargetIndex: -1
            property real _dragStartY: 0

            DeviceCell {
                id: cell
                anchors.fill: parent
                deviceObject: model.deviceObject
                opacity: (sectionList.dragging && sectionList.draggingId === row.deviceId) ? 0.15 : 1.0
            }

            Rectangle {
                anchors.fill: parent
                radius: 12
                color: "transparent"
                border.width: (sectionList.dragging
                               && sectionList.dragTargetIndex === index
                               && sectionList.draggingId !== row.deviceId) ? 1 : 0
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
                        sectionList.dragging = true
                        sectionList.draggingId = row.deviceId
                        sectionList.draggingDeviceObject = row.deviceObject
                        sectionList.dragTargetIndex = index
                        sectionList.dragGhostY = row.y
                    } else {
                        row.z = 0
                        row._lastTargetIndex = -1
                        if (sectionList.draggingId === row.deviceId)
                            sectionList.clearDragState()
                    }
                }

                onTranslationChanged: {
                    const ghostY = row._dragStartY + dragHandler.translation.y
                    const midY = ghostY + row.height / 2
                    if (sectionList.draggingId === row.deviceId)
                        sectionList.dragGhostY = ghostY
                    const idx = sectionList.targetIndexForMidY(midY)
                    if (idx >= 0 && idx !== index && idx !== row._lastTargetIndex) {
                        row._lastTargetIndex = idx
                        if (sectionList.draggingId === row.deviceId)
                            sectionList.dragTargetIndex = idx
                        if (audioBackend)
                            audioBackend.moveDeviceToIndexInDirection(row.deviceId, row.isInput, idx)
                    }
                }
            }

            Component.onDestruction: {
                if (sectionList.draggingId === row.deviceId)
                    sectionList.clearDragState()
            }
        }
    }

    Connections {
        target: appController
        function onCloseAllPopupsRequested() {
            if (settingsMenu) settingsMenu.close()
            if (hiddenMenu) hiddenMenu.close()
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: theme.radius
        color: Qt.rgba(0, 0, 0, 0)
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

            Image {
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                Layout.alignment: Qt.AlignVCenter
                source: "qrc:/assets/earie_white.svg"
                sourceSize.width: Math.round(20 * Screen.devicePixelRatio)
                sourceSize.height: Math.round(20 * Screen.devicePixelRatio)
                smooth: true
            }

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                color: theme.text
                font.pixelSize: 14
                text: "Earie"
            }

            ToolButton {
                id: settingsBtn
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                padding: 0
                property bool wasOpenOnPress: false
                font.family: theme.iconFont
                font.pixelSize: 14
                text: theme.glyphSettings

                background: Rectangle {
                    radius: 8
                    color: settingsBtn.hovered ? theme.cellHover : "transparent"
                }

                onPressed: wasOpenOnPress = settingsMenu.visible
                onClicked: {
                    if (wasOpenOnPress) {
                        wasOpenOnPress = false
                        settingsMenu.close()
                        return
                    }
                    settingsMenu.open()
                }

                StyledMenu {
                    id: settingsMenu
                    y: settingsBtn.height

                    StyledMenuItem {
                        text: (appController && appController.debugMode ? "✓ " : "") + "Debug mode"
                        onTriggered: {
                            if (appController) appController.debugMode = !appController.debugMode
                            root.reopenSettingsMenu()
                        }
                    }
                    StyledMenuItem {
                        text: (appController && appController.startWithWindows ? "✓ " : "") + "Start with Windows"
                        onTriggered: {
                            if (appController) appController.startWithWindows = !appController.startWithWindows
                            root.reopenSettingsMenu()
                        }
                    }
                    StyledMenuItem {
                        id: trayIconItem
                        text: (root.trayIconExpanded ? "▼ " : "► ") + "Tray icon options"
                        onTriggered: {
                            root.trayIconExpanded = !root.trayIconExpanded
                            root.reopenSettingsMenu()
                        }
                    }
                    StyledMenuItem {
                        visible: root.trayIconExpanded
                        implicitHeight: root.trayIconExpanded ? 30 : 0
                        height: root.trayIconExpanded ? 30 : 0
                        leftPadding: 26
                        text: (appController && appController.useNativeTrayIcon ? "✓ " : "") + "Native (.ico)"
                        onTriggered: {
                            if (appController) appController.useNativeTrayIcon = true
                            root.reopenSettingsMenu()
                        }
                    }
                    StyledMenuItem {
                        visible: root.trayIconExpanded
                        implicitHeight: root.trayIconExpanded ? 30 : 0
                        height: root.trayIconExpanded ? 30 : 0
                        leftPadding: 26
                        text: (appController && !appController.useNativeTrayIcon && appController.trayIconMode === 0 ? "✓ " : "") + "Earie White"
                        onTriggered: {
                            if (appController) { appController.useNativeTrayIcon = false; appController.trayIconMode = 0 }
                            root.reopenSettingsMenu()
                        }
                    }
                    StyledMenuItem {
                        visible: root.trayIconExpanded
                        implicitHeight: root.trayIconExpanded ? 30 : 0
                        height: root.trayIconExpanded ? 30 : 0
                        leftPadding: 26
                        text: (appController && !appController.useNativeTrayIcon && appController.trayIconMode === 1 ? "✓ " : "") + "Earie Black"
                        onTriggered: {
                            if (appController) { appController.useNativeTrayIcon = false; appController.trayIconMode = 1 }
                            root.reopenSettingsMenu()
                        }
                    }
                    StyledMenuItem {
                        text: "Open config folder"
                        onTriggered: if (appController) appController.openConfigFolder()
                    }
                    StyledMenuItem {
                        text: "Open application folder"
                        onTriggered: if (appController) appController.openAppFolder()
                    }
                    MenuSeparator { }
                    StyledMenuItem {
                        text: (appController && appController.showSystemSessions ? "✓ " : "") + "Show system sessions"
                        onTriggered: {
                            if (appController) appController.showSystemSessions = !appController.showSystemSessions
                            root.reopenSettingsMenu()
                        }
                    }
                    StyledMenuItem {
                        text: (appController && appController.showInputDevices ? "✓ " : "") + "Show input devices"
                        onTriggered: {
                            if (appController) appController.showInputDevices = !appController.showInputDevices
                            root.reopenSettingsMenu()
                        }
                    }
                    StyledMenuItem {
                        text: (appController && appController.showProcessStatusOnHover ? "✓ " : "") + "Show hover process status"
                        onTriggered: {
                            if (appController) appController.showProcessStatusOnHover = !appController.showProcessStatusOnHover
                            root.reopenSettingsMenu()
                        }
                    }
                    StyledMenuItem {
                        text: (appController && appController.scrollWheelVolumeOnHover ? "✓ " : "") + "Scroll wheel changes volume on hover (2%)"
                        onTriggered: {
                            if (appController) appController.scrollWheelVolumeOnHover = !appController.scrollWheelVolumeOnHover
                            root.reopenSettingsMenu()
                        }
                    }
                }
            }

            ToolButton {
                id: hiddenBtn
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                padding: 0
                property bool wasOpenOnPress: false
                font.family: theme.iconFont
                font.pixelSize: 14
                text: theme.glyphEye

                background: Rectangle {
                    radius: 8
                    color: hiddenBtn.hovered ? theme.cellHover : "transparent"
                }

                onPressed: wasOpenOnPress = hiddenMenu.visible
                onClicked: {
                    if (wasOpenOnPress) {
                        wasOpenOnPress = false
                        hiddenMenu.close()
                        return
                    }
                    hiddenMenu.open()
                }

                StyledMenu {
                    id: hiddenMenu
                    y: hiddenBtn.height

                    StyledMenuItem {
                        text: "Hidden devices..."
                        onTriggered: if (appController) appController.showHiddenItemsWindowSection("devices")
                    }
                    StyledMenuItem {
                        text: "Hidden processes..."
                        onTriggered: if (appController) appController.showHiddenItemsWindowSection("processes")
                    }
                }
            }

            ToolButton {
                id: modeBtn
                Layout.preferredHeight: 28
                padding: 6
                font.pixelSize: 11
                text: appController && appController.allDevices ? "All devices" : "Default device"

                background: Rectangle {
                    radius: 8
                    color: modeBtn.hovered ? theme.cellHover : "transparent"
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                }

                onClicked: if (appController) appController.allDevices = !appController.allDevices
            }
        }

        Flickable {
            id: scrollArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: width
            contentHeight: sectionsColumn.implicitHeight

            Column {
                id: sectionsColumn
                width: scrollArea.width
                spacing: 12

                Column {
                    width: parent.width
                    spacing: 8

                    Text {
                        color: theme.textMuted
                        font.pixelSize: 11
                        text: "Playback"
                    }

                    Text {
                        visible: outputList.count === 0
                        color: theme.textMuted
                        font.pixelSize: 11
                        text: "(No playback devices)"
                    }

                    DeviceSectionView {
                        id: outputList
                        width: parent.width
                        isInputSection: false
                        model: outputDeviceModel
                    }
                }

                Column {
                    visible: appController && appController.showInputDevices
                    width: parent.width
                    spacing: 8

                    Text {
                        color: theme.textMuted
                        font.pixelSize: 11
                        text: "Input"
                    }

                    Text {
                        visible: inputList.count === 0
                        color: theme.textMuted
                        font.pixelSize: 11
                        text: "(No input devices)"
                    }

                    DeviceSectionView {
                        id: inputList
                        width: parent.width
                        isInputSection: true
                        model: inputDeviceModel
                    }
                }
            }
        }
    }
}
