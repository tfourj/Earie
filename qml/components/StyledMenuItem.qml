import QtQuick
import QtQuick.Controls

import "../styles" as Styles

MenuItem {
    id: root

    implicitHeight: 30
    leftPadding: 10
    rightPadding: 10
    topPadding: 0
    bottomPadding: 0

    Styles.Theme { id: theme }

    contentItem: Text {
        text: root.text
        color: root.enabled ? theme.text : theme.textMuted
        font.pixelSize: 12
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: 8
        color: root.down ? Qt.rgba(1, 1, 1, 0.06)
                         : (root.hovered ? theme.cellHover : "transparent")
    }
}
