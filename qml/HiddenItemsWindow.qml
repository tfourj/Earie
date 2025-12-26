import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "styles" as Styles

Item {
    id: root
    width: 420
    implicitHeight: 520

    readonly property int contentHeightHint: Math.ceil(
        12 * 2
        + headerRow.implicitHeight
        + 10
        + scrollArea.contentHeight
        + 6
    )

    onContentHeightHintChanged: if (appController) appController.requestHiddenItemsRelayout()

    Styles.Theme { id: theme }

    property var hiddenDevices: []
    property var globalProcesses: []
    property var perDeviceProcesses: []
    property var perDeviceExpandedMap: ({})
    property bool devicesExpanded: true
    property bool globalExpanded: true
    property bool perDeviceExpanded: true

    function refreshModels() {
        if (!appController)
            return
        hiddenDevices = appController.hiddenDevicesSnapshot() || []
        globalProcesses = appController.hiddenProcessesGlobalSnapshot() || []
        perDeviceProcesses = appController.hiddenProcessesPerDeviceSnapshot() || []
        var nextMap = {}
        for (var i = 0; i < perDeviceProcesses.length; ++i) {
            var id = perDeviceProcesses[i].deviceId
            if (id && perDeviceExpandedMap[id] === true)
                nextMap[id] = true
        }
        perDeviceExpandedMap = nextMap
        appController.requestHiddenItemsRelayout()
    }

    function isPerDeviceExpanded(id) {
        return perDeviceExpandedMap[id] === true
    }

    function setPerDeviceExpanded(id, expanded) {
        var nextMap = {}
        for (var key in perDeviceExpandedMap)
            nextMap[key] = perDeviceExpandedMap[key]
        if (id)
            nextMap[id] = expanded === true
        perDeviceExpandedMap = nextMap
    }

    Component.onCompleted: refreshModels()
    onVisibleChanged: if (visible) refreshModels()

    Connections {
        target: appController
        function onHiddenItemsChanged() { refreshModels() }
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
            id: headerRow
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                color: theme.text
                font.pixelSize: 14
                text: "Hidden items"
            }

            ToolButton {
                id: closeBtn
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                padding: 0
                text: "X"
                font.pixelSize: 12

                background: Rectangle {
                    radius: 8
                    color: closeBtn.hovered ? theme.cellHover : "transparent"
                }

                onClicked: if (appController) appController.hideHiddenItemsWindow()
            }
        }

        Flickable {
            id: scrollArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: width
            contentHeight: contentColumn.implicitHeight

            Column {
                id: contentColumn
                width: scrollArea.width
                spacing: 12

                Column {
                    width: parent.width
                    spacing: 6

                    Item {
                        width: parent.width
                        height: 30

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: devicesExpanded = !devicesExpanded
                        }

                        Text {
                            text: theme.glyphChevron
                            font.family: theme.iconFont
                            font.pixelSize: 12
                            color: theme.textMuted
                            anchors.left: parent.left
                            anchors.leftMargin: 2
                            anchors.verticalCenter: parent.verticalCenter
                            rotation: devicesExpanded ? 90 : 0
                        }

                        Text {
                            color: theme.text
                            font.pixelSize: 12
                            text: "Hidden devices"
                            anchors.left: parent.left
                            anchors.leftMargin: 20
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Text {
                        visible: devicesExpanded && hiddenDevices.length === 0
                        color: theme.textMuted
                        font.pixelSize: 11
                        text: "(No devices)"
                    }

                    Repeater {
                        model: devicesExpanded ? hiddenDevices : []
                        delegate: Rectangle {
                            width: parent.width
                            height: 34
                            radius: theme.cellRadius
                            color: deviceMouse.containsMouse ? theme.cellHover : theme.cellBg
                            border.color: Qt.rgba(1, 1, 1, 0.05)
                            border.width: 1

                            MouseArea {
                                id: deviceMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: if (appController) appController.setDeviceHidden(modelData.deviceId, !modelData.hidden)
                            }

                            Rectangle {
                                width: 14
                                height: 14
                                radius: 3
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                border.width: 1
                                border.color: modelData.hidden ? Qt.rgba(0.23, 0.59, 1.0, 0.95)
                                                     : Qt.rgba(1, 1, 1, 0.22)
                                color: modelData.hidden ? Qt.rgba(0.23, 0.59, 1.0, 0.75)
                                              : "transparent"

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 6
                                    height: 6
                                    radius: 1
                                    color: "white"
                                        visible: modelData.hidden
                                }
                            }

                            Text {
                                text: modelData.connected ? modelData.name : "[disconnected] " + modelData.name
                                anchors.left: parent.left
                                anchors.leftMargin: 30
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                color: theme.text
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 6

                    Item {
                        width: parent.width
                        height: 30

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                globalExpanded = !globalExpanded
                                perDeviceExpanded = !perDeviceExpanded
                            }
                        }

                        Text {
                            text: theme.glyphChevron
                            font.family: theme.iconFont
                            font.pixelSize: 12
                            color: theme.textMuted
                            anchors.left: parent.left
                            anchors.leftMargin: 2
                            anchors.verticalCenter: parent.verticalCenter
                            rotation: (globalExpanded || perDeviceExpanded) ? 90 : 0
                        }

                        Text {
                            color: theme.text
                            font.pixelSize: 12
                            text: "Hidden processes"
                            anchors.left: parent.left
                            anchors.leftMargin: 20
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 6
                        visible: globalExpanded

                        Text {
                            color: theme.textMuted
                            font.pixelSize: 11
                            text: "Global"
                        }

                        Text {
                            visible: globalProcesses.length === 0
                            color: theme.textMuted
                            font.pixelSize: 11
                            text: "(No processes)"
                        }

                    Repeater {
                            model: globalProcesses
                            delegate: Rectangle {
                                width: parent.width
                                height: 34
                                radius: theme.cellRadius
                                color: globalMouse.containsMouse ? theme.cellHover : theme.cellBg
                                border.color: Qt.rgba(1, 1, 1, 0.05)
                                border.width: 1

                                MouseArea {
                                    id: globalMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: if (appController) appController.setProcessHiddenGlobal(modelData.exePath, !modelData.hidden)
                                }

                                Rectangle {
                                    width: 14
                                    height: 14
                                    radius: 3
                                    anchors.left: parent.left
                                    anchors.leftMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    border.width: 1
                                    border.color: modelData.hidden ? Qt.rgba(0.23, 0.59, 1.0, 0.95)
                                                         : Qt.rgba(1, 1, 1, 0.22)
                                    color: modelData.hidden ? Qt.rgba(0.23, 0.59, 1.0, 0.75)
                                                  : "transparent"

                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 6
                                        height: 6
                                        radius: 1
                                        color: "white"
                                        visible: modelData.hidden
                                    }
                                }

                                Text {
                                    text: modelData.name
                                    anchors.left: parent.left
                                    anchors.leftMargin: 30
                                    anchors.right: parent.right
                                    anchors.rightMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: theme.text
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 6
                        visible: perDeviceExpanded

                        Text {
                            color: theme.textMuted
                            font.pixelSize: 11
                            text: "Per device"
                        }

                        Text {
                            visible: perDeviceProcesses.length === 0
                            color: theme.textMuted
                            font.pixelSize: 11
                            text: "(No devices)"
                        }

                        Repeater {
                            model: perDeviceProcesses
                            delegate: Column {
                                width: parent.width
                                spacing: 6
                                readonly property bool expanded: root.isPerDeviceExpanded(deviceId)
                                property string deviceId: modelData.deviceId

                                Item {
                                    width: parent.width
                                    height: 26

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.setPerDeviceExpanded(deviceId, !root.isPerDeviceExpanded(deviceId))
                                    }

                                    Text {
                                        text: theme.glyphChevron
                                        font.family: theme.iconFont
                                        font.pixelSize: 11
                                        color: theme.textMuted
                                        anchors.left: parent.left
                                        anchors.leftMargin: 2
                                        anchors.verticalCenter: parent.verticalCenter
                                        rotation: expanded ? 90 : 0
                                    }

                                    Text {
                                        color: theme.textMuted
                                        font.pixelSize: 11
                                        text: modelData.connected ? modelData.name : "[disconnected] " + modelData.name
                                        anchors.left: parent.left
                                        anchors.leftMargin: 18
                                        anchors.verticalCenter: parent.verticalCenter
                                        elide: Text.ElideRight
                                    }
                                }

                                Text {
                                    visible: expanded && (!modelData.processes || modelData.processes.length === 0)
                                    color: theme.textMuted
                                    font.pixelSize: 11
                                    text: "(No processes)"
                                }

                                Repeater {
                                    model: expanded ? (modelData.processes || []) : []
                                    delegate: Rectangle {
                                        width: parent.width
                                        height: 34
                                        radius: theme.cellRadius
                                        color: perDevMouse.containsMouse ? theme.cellHover : theme.cellBg
                                        border.color: Qt.rgba(1, 1, 1, 0.05)
                                        border.width: 1

                                        MouseArea {
                                            id: perDevMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: if (appController) appController.setProcessHiddenForDevice(deviceId, modelData.exePath, !modelData.hidden)
                                        }

                                        Rectangle {
                                            width: 14
                                            height: 14
                                            radius: 3
                                            anchors.left: parent.left
                                            anchors.leftMargin: 10
                                            anchors.verticalCenter: parent.verticalCenter
                                            border.width: 1
                                            border.color: modelData.hidden ? Qt.rgba(0.23, 0.59, 1.0, 0.95)
                                                                          : Qt.rgba(1, 1, 1, 0.22)
                                            color: modelData.hidden ? Qt.rgba(0.23, 0.59, 1.0, 0.75)
                                                                    : "transparent"

                                            Rectangle {
                                                anchors.centerIn: parent
                                                width: 6
                                                height: 6
                                                radius: 1
                                                color: "white"
                                                visible: modelData.hidden
                                            }
                                        }

                                        Text {
                                            text: modelData.name
                                            anchors.left: parent.left
                                            anchors.leftMargin: 30
                                            anchors.right: parent.right
                                            anchors.rightMargin: 10
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: theme.text
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
