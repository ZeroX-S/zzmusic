import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: root

    property var playback
    readonly property real minimumTouchTarget: Theme.itemSizeSmall
    signal queueRequested()

    height: Theme.itemSizeLarge

    Row {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.horizontalPageMargin
        height: Theme.itemSizeLarge
        spacing: 0

        Item {
            width: parent.width / 5
            height: parent.height

            IconButton {
                objectName: "playerRepeatButton"
                anchors.centerIn: parent
                width: root.minimumTouchTarget
                height: root.minimumTouchTarget
                enabled: root.playback && root.playback.queue.length > 0
                icon.source: root.playback && root.playback.repeatMode === 3
                             ? "image://theme/icon-m-shuffle"
                             : root.playback && root.playback.repeatMode === 2
                               ? "image://theme/icon-m-repeat-single"
                               : "image://theme/icon-m-repeat"
                opacity: root.playback && root.playback.repeatMode !== 0
                         ? 1.0 : Theme.opacityLow
                onClicked: root.playback.repeatMode =
                           (root.playback.repeatMode + 1) % 4
            }
        }

        Item {
            width: parent.width / 5
            height: parent.height

            IconButton {
                objectName: "playerPreviousButton"
                anchors.centerIn: parent
                width: root.minimumTouchTarget
                height: root.minimumTouchTarget
                enabled: root.playback && root.playback.canGoPrevious
                icon.source: "image://theme/icon-m-previous"
                onClicked: root.playback.previous()
            }
        }

        Item {
            width: parent.width / 5
            height: parent.height

            Rectangle {
                anchors.centerIn: parent
                width: Theme.itemSizeMedium
                height: width
                radius: width / 2
                color: Theme.rgba(Theme.highlightBackgroundColor, 0.34)
            }

            IconButton {
                objectName: "playerToggleButton"
                anchors.centerIn: parent
                width: Theme.itemSizeMedium
                height: Theme.itemSizeMedium
                enabled: root.playback && (root.playback.playing
                                           ? root.playback.canPause
                                           : root.playback.canPlay)
                icon.source: root.playback && root.playback.playing
                             ? "image://theme/icon-m-pause"
                             : "image://theme/icon-m-play"
                onClicked: {
                    if (root.playback.playing)
                        root.playback.pause()
                    else
                        root.playback.play()
                }
            }
        }

        Item {
            width: parent.width / 5
            height: parent.height

            IconButton {
                objectName: "playerNextButton"
                anchors.centerIn: parent
                width: root.minimumTouchTarget
                height: root.minimumTouchTarget
                enabled: root.playback && root.playback.canGoNext
                icon.source: "image://theme/icon-m-next"
                onClicked: root.playback.next()
            }
        }

        Item {
            width: parent.width / 5
            height: parent.height

            IconButton {
                objectName: "playerQueueButton"
                anchors.centerIn: parent
                width: root.minimumTouchTarget
                height: root.minimumTouchTarget
                enabled: root.playback && root.playback.queue.length > 0
                icon.source: "image://theme/icon-m-menu"
                onClicked: root.queueRequested()
            }
        }
    }
}
