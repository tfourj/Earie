import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "styles" as Styles
import "components"

Item {
    id: root
    width: 420
    height: 520

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
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                color: theme.text
                font.pixelSize: 14
                text: "Volume Mixer"
            }

            Text {
                color: theme.textMuted
                font.pixelSize: 12
                text: appController && appController.allDevices ? "All devices" : "Default device"
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            contentItem: Column {
                width: parent.width
                spacing: 10

                Repeater {
                    model: deviceModel
                    delegate: DeviceCell {
                        width: parent.width
                        deviceObject: model.deviceObject
                    }
                }

                Item { width: 1; height: 4 }
            }
        }
    }
}


