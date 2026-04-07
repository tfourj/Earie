import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "styles" as Styles

Item {
    id: root
    width: 760
    implicitHeight: 660

    property string currentTab: "general"
    property var hiddenDevices: []
    property var globalProcesses: []
    property var perDeviceProcesses: []
    property var perDeviceExpandedMap: ({})

    readonly property int contentHeightHint: Math.ceil(
        18 * 2
        + headerRow.implicitHeight
        + 14
        + bodyRow.implicitHeight
        + 10
    )

    function refreshHiddenModels() {
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

    function openTab(tab) {
        currentTab = tab
    }

    Component.onCompleted: refreshHiddenModels()
    onCurrentTabChanged: if (appController) appController.requestSettingsRelayout()
    onContentHeightHintChanged: if (appController) appController.requestSettingsRelayout()

    Connections {
        target: appController
        function onHiddenItemsChanged() { refreshHiddenModels() }
    }

    Styles.Theme { id: theme }

    component NavButton: Rectangle {
        required property string tabKey
        required property string label

        radius: 12
        color: root.currentTab === tabKey ? Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.16) : "transparent"
        border.width: 1
        border.color: root.currentTab === tabKey ? Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.5) : Qt.rgba(1, 1, 1, 0.06)
        implicitHeight: 40

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.right: parent.right
            anchors.rightMargin: 14
            color: root.currentTab === parent.tabKey ? theme.text : theme.textMuted
            font.pixelSize: 13
            font.bold: root.currentTab === parent.tabKey
            text: parent.label
            elide: Text.ElideRight
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.openTab(parent.tabKey)
        }
    }

    component SectionCard: Rectangle {
        default property alias contentData: contentColumn.data

        width: parent ? parent.width : 480
        radius: 14
        color: Qt.rgba(0x26 / 255, 0x28 / 255, 0x2C / 255, 0.94)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.06)
        implicitHeight: contentColumn.implicitHeight + 24

        Column {
            id: contentColumn
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10
        }
    }

    component SectionHeading: Text {
        color: theme.text
        font.pixelSize: 18
        font.bold: true
    }

    component SectionBody: Text {
        color: theme.textMuted
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }

    component ToggleRow: Rectangle {
        id: toggleRow
        required property string label
        required property string description
        required property bool checked
        signal toggled()

        radius: 12
        color: toggleMouse.containsMouse ? theme.cellHover : theme.cellBg
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.05)
        implicitHeight: 66

        MouseArea {
            id: toggleMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.toggled()
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    color: theme.text
                    font.pixelSize: 13
                    text: toggleRow.label
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    color: theme.textMuted
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    text: toggleRow.description
                }
            }

            Switch {
                checked: toggleRow.checked
                onToggled: toggleRow.toggled()
            }
        }
    }

    component ChoiceChip: Rectangle {
        required property string label
        required property bool selected
        signal clicked()

        radius: 11
        color: selected ? Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.18) : theme.cellBg
        border.width: 1
        border.color: selected ? Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.52) : Qt.rgba(1, 1, 1, 0.08)
        implicitHeight: 34
        implicitWidth: chipText.implicitWidth + 28

        Text {
            id: chipText
            anchors.centerIn: parent
            color: parent.selected ? theme.text : theme.textMuted
            font.pixelSize: 12
            font.bold: parent.selected
            text: parent.label
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }

    component ActionButton: Rectangle {
        required property string label
        signal clicked()

        radius: 11
        color: actionMouse.containsMouse ? theme.cellHover : theme.cellBg
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.08)
        implicitHeight: 36
        implicitWidth: actionText.implicitWidth + 28

        Text {
            id: actionText
            anchors.centerIn: parent
            color: theme.text
            font.pixelSize: 12
            text: parent.label
        }

        MouseArea {
            id: actionMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 18
        color: Qt.rgba(0, 0, 0, 0)
        border.color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        clip: true

        Rectangle {
            anchors.fill: parent
            radius: 18
            color: Qt.rgba(0x1D / 255, 0x1F / 255, 0x22 / 255, 0.86)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        RowLayout {
            id: headerRow
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    color: theme.text
                    font.pixelSize: 22
                    font.bold: true
                    text: "Settings"
                }

                Text {
                    color: theme.textMuted
                    font.pixelSize: 12
                    text: "Everything app-owned in one place."
                }
            }

            ToolButton {
                id: closeBtn
                Layout.preferredWidth: 44
                Layout.preferredHeight: 32
                padding: 0
                font.family: theme.iconFont
                text: "\uE711"
                font.pixelSize: 10

                background: Rectangle {
                    radius: 8
                    color: closeBtn.hovered ? "#C42B1C" : "transparent"
                }

                onClicked: if (appController) appController.hideSettingsWindow()
            }
        }

        RowLayout {
            id: bodyRow
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            Rectangle {
                Layout.preferredWidth: 180
                Layout.fillHeight: true
                radius: 16
                color: Qt.rgba(1, 1, 1, 0.03)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.05)

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    NavButton { width: parent.width; tabKey: "general"; label: "General" }
                    NavButton { width: parent.width; tabKey: "devices"; label: "Devices" }
                    NavButton { width: parent.width; tabKey: "hidden"; label: "Hidden" }
                    NavButton { width: parent.width; tabKey: "about"; label: "About" }
                }
            }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: width
                contentHeight: contentColumn.implicitHeight

                Column {
                    id: contentColumn
                    width: parent.width
                    spacing: 14

                    Column {
                        visible: root.currentTab === "general"
                        width: parent.width
                        spacing: 14

                        SectionHeading { text: "General" }
                        SectionBody { width: parent.width; text: "Startup, diagnostics, and interaction settings." }

                        SectionCard {
                            ToggleRow {
                                width: parent.width
                                label: "Start with Windows"
                                description: "Launch Earie automatically when you sign in."
                                checked: appController && appController.startWithWindows
                                onToggled: if (appController) appController.startWithWindows = !appController.startWithWindows
                            }
                            ToggleRow {
                                width: parent.width
                                label: "Debug mode"
                                description: "Write app logs next to the executable for troubleshooting."
                                checked: appController && appController.debugMode
                                onToggled: if (appController) appController.debugMode = !appController.debugMode
                            }
                            ToggleRow {
                                width: parent.width
                                label: "Show hover process status"
                                description: "Show extra process state details while hovering session rows."
                                checked: appController && appController.showProcessStatusOnHover
                                onToggled: if (appController) appController.showProcessStatusOnHover = !appController.showProcessStatusOnHover
                            }
                            ToggleRow {
                                width: parent.width
                                label: "Scroll wheel changes volume on hover"
                                description: "Use the mouse wheel over sliders to adjust volume in fixed 2% steps."
                                checked: appController && appController.scrollWheelVolumeOnHover
                                onToggled: if (appController) appController.scrollWheelVolumeOnHover = !appController.scrollWheelVolumeOnHover
                            }
                        }

                        SectionCard {
                            Text {
                                color: theme.text
                                font.pixelSize: 13
                                text: "Folders"
                            }

                            Row {
                                spacing: 10
                                ActionButton {
                                    label: "Open config folder"
                                    onClicked: if (appController) appController.openConfigFolder()
                                }
                                ActionButton {
                                    label: "Open application folder"
                                    onClicked: if (appController) appController.openAppFolder()
                                }
                            }
                        }
                    }

                    Column {
                        visible: root.currentTab === "devices"
                        width: parent.width
                        spacing: 14

                        SectionHeading { text: "Devices" }
                        SectionBody { width: parent.width; text: "Control what devices are shown and how the tray icon looks." }

                        SectionCard {
                            Text {
                                color: theme.text
                                font.pixelSize: 13
                                text: "Visibility"
                            }

                            Row {
                                spacing: 10
                                ChoiceChip {
                                    label: "Default device"
                                    selected: appController && !appController.allDevices
                                    onClicked: if (appController) appController.allDevices = false
                                }
                                ChoiceChip {
                                    label: "All devices"
                                    selected: appController && appController.allDevices
                                    onClicked: if (appController) appController.allDevices = true
                                }
                            }

                            ToggleRow {
                                width: parent.width
                                label: "Show system sessions"
                                description: "Include Windows system audio sessions in the mixer."
                                checked: appController && appController.showSystemSessions
                                onToggled: if (appController) appController.showSystemSessions = !appController.showSystemSessions
                            }
                            ToggleRow {
                                width: parent.width
                                label: "Show input devices"
                                description: "Include microphones and other capture endpoints in the mixer."
                                checked: appController && appController.showInputDevices
                                onToggled: if (appController) appController.showInputDevices = !appController.showInputDevices
                            }
                        }

                        SectionCard {
                            Text {
                                color: theme.text
                                font.pixelSize: 13
                                text: "Tray icon"
                            }

                            Row {
                                spacing: 10
                                ChoiceChip {
                                    label: "Native (.ico)"
                                    selected: appController && appController.useNativeTrayIcon
                                    onClicked: if (appController) appController.useNativeTrayIcon = true
                                }
                                ChoiceChip {
                                    label: "Earie White"
                                    selected: appController && !appController.useNativeTrayIcon && appController.trayIconMode === 0
                                    onClicked: if (appController) { appController.useNativeTrayIcon = false; appController.trayIconMode = 0 }
                                }
                                ChoiceChip {
                                    label: "Earie Black"
                                    selected: appController && !appController.useNativeTrayIcon && appController.trayIconMode === 1
                                    onClicked: if (appController) { appController.useNativeTrayIcon = false; appController.trayIconMode = 1 }
                                }
                            }
                        }
                    }

                    Column {
                        visible: root.currentTab === "hidden"
                        width: parent.width
                        spacing: 14

                        SectionHeading { text: "Hidden" }
                        SectionBody { width: parent.width; text: "Manage hidden devices and process rules without leaving settings." }

                        SectionCard {
                            Text {
                                color: theme.text
                                font.pixelSize: 13
                                text: "Hidden devices"
                            }

                            Text {
                                visible: hiddenDevices.length === 0
                                color: theme.textMuted
                                font.pixelSize: 11
                                text: "(No devices)"
                            }

                            Repeater {
                                model: hiddenDevices
                                delegate: Rectangle {
                                    width: parent.width
                                    height: 36
                                    radius: 12
                                    color: deviceMouse.containsMouse ? theme.cellHover : theme.cellBg
                                    border.width: 1
                                    border.color: Qt.rgba(1, 1, 1, 0.05)

                                    MouseArea {
                                        id: deviceMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: if (appController) appController.setDeviceHidden(modelData.deviceId, !modelData.hidden)
                                    }

                                    Row {
                                        anchors.fill: parent
                                        anchors.margins: 12
                                        spacing: 10

                                        Rectangle {
                                            width: 14
                                            height: 14
                                            radius: 3
                                            anchors.verticalCenter: parent.verticalCenter
                                            border.width: 1
                                            border.color: modelData.hidden ? Qt.rgba(0.23, 0.59, 1.0, 0.95) : Qt.rgba(1, 1, 1, 0.22)
                                            color: modelData.hidden ? Qt.rgba(0.23, 0.59, 1.0, 0.75) : "transparent"

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
                                            width: parent.width - 32
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: theme.text
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                            text: modelData.connected ? modelData.name : "[disconnected] " + modelData.name
                                        }
                                    }
                                }
                            }
                        }

                        SectionCard {
                            Text {
                                color: theme.text
                                font.pixelSize: 13
                                text: "Hidden processes"
                            }

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
                                    height: 36
                                    radius: 12
                                    color: globalMouse.containsMouse ? theme.cellHover : theme.cellBg
                                    border.width: 1
                                    border.color: Qt.rgba(1, 1, 1, 0.05)

                                    MouseArea {
                                        id: globalMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: if (appController) appController.setProcessHiddenGlobal(modelData.exePath, !modelData.hidden)
                                    }

                                    Row {
                                        anchors.fill: parent
                                        anchors.margins: 12
                                        spacing: 10

                                        Rectangle {
                                            width: 14
                                            height: 14
                                            radius: 3
                                            anchors.verticalCenter: parent.verticalCenter
                                            border.width: 1
                                            border.color: modelData.hidden ? Qt.rgba(0.23, 0.59, 1.0, 0.95) : Qt.rgba(1, 1, 1, 0.22)
                                            color: modelData.hidden ? Qt.rgba(0.23, 0.59, 1.0, 0.75) : "transparent"

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
                                            width: parent.width - 32
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: theme.text
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                            text: modelData.name
                                        }
                                    }
                                }
                            }

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
                                    required property var modelData
                                    readonly property string deviceId: modelData.deviceId
                                    readonly property bool expanded: root.isPerDeviceExpanded(deviceId)

                                    Rectangle {
                                        width: parent.width
                                        height: 38
                                        radius: 12
                                        color: headerMouse.containsMouse ? theme.cellHover : theme.cellBg
                                        border.width: 1
                                        border.color: Qt.rgba(1, 1, 1, 0.05)

                                        MouseArea {
                                            id: headerMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.setPerDeviceExpanded(deviceId, !expanded)
                                        }

                                        Row {
                                            anchors.fill: parent
                                            anchors.margins: 12
                                            spacing: 8

                                            Text {
                                                anchors.verticalCenter: parent.verticalCenter
                                                color: theme.textMuted
                                                font.family: theme.iconFont
                                                font.pixelSize: 11
                                                text: expanded ? "\uE70E" : "\uE70D"
                                            }

                                            Text {
                                                width: parent.width - 24
                                                anchors.verticalCenter: parent.verticalCenter
                                                color: theme.text
                                                font.pixelSize: 12
                                                elide: Text.ElideRight
                                                text: modelData.connected ? modelData.name : "[disconnected] " + modelData.name
                                            }
                                        }
                                    }

                                    Column {
                                        visible: expanded
                                        width: parent.width
                                        spacing: 4

                                        Text {
                                            visible: !modelData.processes || modelData.processes.length === 0
                                            color: theme.textMuted
                                            font.pixelSize: 11
                                            text: "(No processes)"
                                        }

                                        Repeater {
                                            model: modelData.processes || []
                                            delegate: Rectangle {
                                                width: parent.width
                                                height: 34
                                                radius: 10
                                                color: processMouse.containsMouse ? theme.cellHover : Qt.rgba(1, 1, 1, 0.03)
                                                border.width: 1
                                                border.color: Qt.rgba(1, 1, 1, 0.04)

                                                MouseArea {
                                                    id: processMouse
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: if (appController) appController.setProcessHiddenForDevice(deviceId, modelData.exePath, !modelData.hidden)
                                                }

                                                Row {
                                                    anchors.fill: parent
                                                    anchors.margins: 12
                                                    spacing: 10

                                                    Rectangle {
                                                        width: 14
                                                        height: 14
                                                        radius: 3
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        border.width: 1
                                                        border.color: modelData.hidden ? Qt.rgba(0.23, 0.59, 1.0, 0.95) : Qt.rgba(1, 1, 1, 0.22)
                                                        color: modelData.hidden ? Qt.rgba(0.23, 0.59, 1.0, 0.75) : "transparent"

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
                                                        width: parent.width - 32
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        color: theme.text
                                                        font.pixelSize: 12
                                                        elide: Text.ElideRight
                                                        text: modelData.name
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Column {
                        visible: root.currentTab === "about"
                        width: parent.width
                        spacing: 14

                        SectionHeading { text: "About" }
                        SectionBody { width: parent.width; text: "App info and quick links." }

                        SectionCard {
                            Row {
                                spacing: 14

                                Rectangle {
                                    width: 72
                                    height: 72
                                    radius: 18
                                    color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.12)
                                    border.width: 1
                                    border.color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.3)

                                    Image {
                                        anchors.centerIn: parent
                                        width: 40
                                        height: 40
                                        source: "qrc:/assets/earie_white.svg"
                                        fillMode: Image.PreserveAspectFit
                                    }
                                }

                                Column {
                                    spacing: 4

                                    Text {
                                        color: theme.text
                                        font.pixelSize: 20
                                        font.bold: true
                                        text: "Earie"
                                    }

                                    Text {
                                        color: theme.textMuted
                                        font.pixelSize: 12
                                        text: "Version " + appVersion
                                    }

                                    Text {
                                        color: theme.textMuted
                                        font.pixelSize: 12
                                        text: "Per-app volume mixer for Windows."
                                    }
                                }
                            }
                        }

                        SectionCard {
                            Text {
                                color: theme.text
                                font.pixelSize: 13
                                text: "Credits"
                            }

                            Text {
                                width: parent.width
                                color: theme.textMuted
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                                text: "Made by tfourj. This settings window replaces the old popup settings, hidden-items window, and native About dialog."
                            }
                        }
                    }
                }
            }
        }
    }
}
