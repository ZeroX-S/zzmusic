import QtQuick 2.0
import Sailfish.Silica 1.0

DockedPanel {
    id: root

    property var controller
    property var playback: controller ? controller.playback : null
    dock: Dock.Bottom
    modal: true

    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.itemSizeLarge

        Column {
            anchors.left: parent.left
            anchors.leftMargin: Theme.horizontalPageMargin
            anchors.right: parent.right
            anchors.rightMargin: Theme.horizontalPageMargin
            anchors.verticalCenter: parent.verticalCenter

            Label {
                width: parent.width
                text: qsTr("Play queue")
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeMedium
                font.weight: Font.DemiBold
                truncationMode: TruncationMode.Fade
            }
            Label {
                width: parent.width
                text: root.playback && root.playback.currentTrackValue
                      ? root.playback.currentTrackValue.title : ""
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                truncationMode: TruncationMode.Fade
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.rgba(Theme.secondaryColor, 0.18)
        }
    }

    SilicaListView {
        id: queueList
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: clearButton.top
        model: root.playback ? root.playback.queue : []
        clip: true
        VerticalScrollDecorator {}

        delegate: ListItem {
            objectName: "queueTrack_" + modelData.mid
            width: queueList.width
            contentHeight: Theme.itemSizeMedium
            onClicked: {
                root.controller.playQueueIndex(index)
            }

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: Theme.paddingSmall
                visible: index === root.playback.currentIndex
                color: Theme.highlightColor
            }
            Column {
                anchors.left: parent.left
                anchors.right: removeButton.left
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.rightMargin: Theme.paddingSmall
                anchors.verticalCenter: parent.verticalCenter

                Label {
                    width: parent.width
                    text: modelData.title
                    color: index === root.playback.currentIndex
                           ? Theme.highlightColor : Theme.primaryColor
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    width: parent.width
                    text: modelData.artist
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    truncationMode: TruncationMode.Fade
                }
            }
            IconButton {
                id: removeButton
                anchors.right: parent.right
                anchors.rightMargin: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                icon.source: "image://theme/icon-m-delete"
                onClicked: root.playback.remove(index)
            }
        }
    }

    Button {
        id: clearButton
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.paddingLarge
        anchors.horizontalCenter: parent.horizontalCenter
        enabled: root.playback && root.playback.queue.length > 0
        text: qsTr("Clear queue")
        onClicked: clearRemorse.execute(qsTr("Clearing queue"),
                                        function() {
            root.playback.clear()
            root.hide()
        })
    }

    RemorsePopup { id: clearRemorse }
}
