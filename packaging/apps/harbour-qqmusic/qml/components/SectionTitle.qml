import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: root

    property string text: ""
    property string detail: ""
    readonly property bool compactPresentation: true

    height: visible ? Theme.itemSizeMedium : 0

    Label {
        anchors.left: parent.left
        anchors.leftMargin: Theme.horizontalPageMargin
        anchors.right: detailLabel.visible ? detailLabel.left : parent.right
        anchors.rightMargin: Theme.paddingMedium
        anchors.verticalCenter: parent.verticalCenter
        text: root.text
        color: Theme.highlightColor
        font.pixelSize: Theme.fontSizeMedium
        font.weight: Font.DemiBold
        truncationMode: TruncationMode.Fade
    }

    Label {
        id: detailLabel
        anchors.right: parent.right
        anchors.rightMargin: Theme.horizontalPageMargin
        anchors.verticalCenter: parent.verticalCenter
        visible: root.detail.length > 0
        text: root.detail
        color: Theme.secondaryColor
        font.pixelSize: Theme.fontSizeExtraSmall
        truncationMode: TruncationMode.Fade
    }

    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: Theme.horizontalPageMargin
        anchors.right: parent.right
        anchors.rightMargin: Theme.horizontalPageMargin
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.rgba(Theme.secondaryColor, 0.18)
    }
}
