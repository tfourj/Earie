import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../styles" as Styles

Item {
    id: root
    property var deviceObject
    property string title: deviceObject ? deviceObject.name : ""
    property bool isInput: deviceObject ? deviceObject.isInput : false
    property bool isDefault: deviceObject ? deviceObject.isDefault : false
    property var sessionsModel: deviceObject ? deviceObject.sessionsModel : null
    property bool showInputApplications: !root.isInput || !appController || appController.showInputApplications

    Styles.Theme { id: theme }

    width: parent ? parent.width : 380

    implicitHeight: header.height + masterRow.height + (sessionsList.visible ? sessionsList.implicitHeight : 0) + theme.cellPad * 2 + 10

    Rectangle {
        anchors.fill: parent
        radius: theme.cellRadius
        color: theme.cellBg
        border.color: isDefault ? theme.accent : theme.divider
        border.width: isDefault ? 1 : 1
    }

    Column {
        anchors.fill: parent
        anchors.margins: theme.cellPad
        spacing: 10

        RowLayout {
            id: header
            width: parent.width
            spacing: 8

            Text {
                Layout.fillWidth: true
                elide: Text.ElideRight
                color: theme.text
                font.pixelSize: 14
                text: root.title
            }

            Rectangle {
                visible: root.isDefault
                radius: 8
                color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.18)
                border.color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.55)
                border.width: 1
                Layout.preferredHeight: 18
                Layout.preferredWidth: 54

                Text {
                    anchors.centerIn: parent
                    color: theme.accent
                    font.pixelSize: 11
                    text: "Default"
                }
            }

            Rectangle {
                visible: root.isInput
                radius: 8
                color: Qt.rgba(1, 1, 1, 0.08)
                border.color: Qt.rgba(1, 1, 1, 0.12)
                border.width: 1
                Layout.preferredHeight: 18
                Layout.preferredWidth: 42

                Text {
                    anchors.centerIn: parent
                    color: theme.textMuted
                    font.pixelSize: 11
                    text: "Input"
                }
            }
        }

        DeviceMasterRow {
            id: masterRow
            width: parent.width
            deviceObject: root.deviceObject
        }

        Column {
            id: sessionsList
            visible: root.showInputApplications
            width: parent.width
            spacing: 4

            Repeater {
                model: root.sessionsModel
                delegate: SessionRow {
                    width: sessionsList.width
                    sessionObject: model.sessionObject
                }
            }
        }
    }
}


