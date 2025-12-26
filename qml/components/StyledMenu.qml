import QtQuick
import QtQuick.Controls

import "../styles" as Styles

Menu {
    id: root

    padding: 6
    implicitWidth: 190
    closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

    onAboutToShow: if (appController) appController.popupOpened()
    onAboutToHide: if (appController) appController.popupClosed()

    Connections {
        target: appController
        function onCloseAllPopupsRequested() { root.close() }
    }

    Styles.Theme { id: theme }

    background: Rectangle {
        radius: 10
        color: Qt.rgba(0x20 / 255, 0x22 / 255, 0x26 / 255, 0.95)
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
    }
}
