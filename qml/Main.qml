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
        // Use realized geometry (childrenRect) instead of implicitHeight to avoid under-measuring
        // before delegates are fully instantiated.
        + listColumn.childrenRect.height
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
                color: theme.textMuted
                font.pixelSize: 12
                text: appController && appController.allDevices ? "All devices" : "Default device"
            }
        }

        // Use Flickable directly (avoids ScrollView contentItem restrictions + hides scrollbars).
        Flickable {
            id: listFlick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick

            contentWidth: width
            contentHeight: listColumn.implicitHeight

            Column {
                id: listColumn
                width: listFlick.width
                spacing: 10

                Repeater {
                    model: deviceModel
                    delegate: DeviceCell {
                        width: listColumn.width
                        deviceObject: model.deviceObject
                    }
                }

                Item { width: 1; height: 4 }
            }
        }
    }
}


