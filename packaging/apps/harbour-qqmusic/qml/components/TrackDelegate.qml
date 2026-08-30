import QtQuick 2.0
import Sailfish.Silica 1.0

ListItem {
    id: root

    property var controller
    property string trackMid: ""
    property string title: ""
    property string artist: ""
    property string album: ""
    property url artwork: ""
    property int durationMs: 0
    property bool playable: true
    property url sourceUrl: ""
    property bool current: false

    signal playRequested()
    signal playNextRequested()
    signal addToQueueRequested()
    signal viewSourceRequested()

    contentHeight: Math.max(Theme.itemSizeMedium,
                            textColumn.height + 2 * Theme.paddingSmall)
    enabled: playable
    onClicked: playRequested()

    Rectangle {
        anchors.fill: parent
        visible: root.current
        color: Theme.rgba(Theme.highlightBackgroundColor,
                          root.highlighted ? 0.35 : 0.2)
    }

    CachedArtwork {
        id: art
        anchors.left: parent.left
        anchors.leftMargin: Theme.horizontalPageMargin
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.itemSizeSmall
        height: width
        controller: root.controller
        remoteSource: root.artwork
        fillMode: Image.PreserveAspectCrop
    }

    Column {
        id: textColumn
        anchors.left: art.right
        anchors.right: durationLabel.left
        anchors.leftMargin: Theme.paddingMedium
        anchors.rightMargin: Theme.paddingSmall
        anchors.verticalCenter: parent.verticalCenter

        Label {
            width: parent.width
            text: root.title
            color: root.highlighted || root.current
                   ? Theme.highlightColor : Theme.primaryColor
            font.pixelSize: Theme.fontSizeSmall
            font.weight: root.current ? Font.DemiBold : Font.Normal
            truncationMode: TruncationMode.Fade
        }

        Label {
            width: parent.width
            text: root.artist + (root.album ? " · " + root.album : "")
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeExtraSmall
            truncationMode: TruncationMode.Fade
        }
    }

    Label {
        id: durationLabel
        anchors.right: parent.right
        anchors.rightMargin: Theme.horizontalPageMargin
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.itemSizeMedium
        horizontalAlignment: Text.AlignRight
        text: root.durationMs > 0
              ? Math.floor(root.durationMs / 60000) + ":"
                + ("0" + Math.floor((root.durationMs / 1000) % 60)).slice(-2)
              : ""
        color: Theme.secondaryColor
        font.pixelSize: Theme.fontSizeExtraSmall
    }

    menu: ContextMenu {
        MenuItem {
            text: qsTr("Play next")
            onClicked: root.playNextRequested()
        }
        MenuItem {
            text: qsTr("Add to queue")
            onClicked: root.addToQueueRequested()
        }
        MenuItem {
            visible: root.sourceUrl.toString().length > 0
            text: qsTr("View source")
            onClicked: root.viewSourceRequested()
        }
    }

}
