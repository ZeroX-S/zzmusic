import QtQuick 2.0
import Sailfish.Silica 1.0

BackgroundItem {
    id: root

    property var controller
    property var playback
    property var track: playback ? playback.currentTrackValue : ({})
    property bool hasTrack: !!(track && track.mid)
    readonly property bool dockedPresentation: true
    readonly property bool progressOnTop: true
    readonly property int visibleTransportControlCount: 4
    readonly property real progressFraction: playback && playback.durationMs > 0
                                             ? Math.max(0, Math.min(
                                                 1, playback.positionMs
                                                 / playback.durationMs)) : 0
    readonly property bool artworkLoading: hasTrack
                                           && track.artworkUrl
                                           && artwork.status !== Image.Ready
                                           && artwork.status !== Image.Error
    readonly property bool layoutBoundsValid: artwork.x >= 0
                                              && textColumn.x >= artwork.x
                                                 + artwork.width
                                              && textColumn.x
                                                 + textColumn.width
                                                 <= transportRow.x
                                              && transportRow.x
                                                 + transportRow.width <= width
    signal playerRequested()

    height: hasTrack ? Theme.itemSizeLarge : 0
    visible: hasTrack
    z: 20
    onClicked: playerRequested()

    Rectangle {
        anchors.fill: parent
        color: Theme.rgba(Theme.overlayBackgroundColor, 0.94)
    }

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.max(2, Theme.paddingSmall / 3)
        color: Theme.rgba(Theme.secondaryColor, 0.22)

        Rectangle {
            objectName: "compactPlayerProgress"
            width: parent.width * root.progressFraction
            height: parent.height
            color: Theme.highlightColor
        }
    }

    CachedArtwork {
        id: artwork
        objectName: "compactPlayerArtwork"
        anchors.left: parent.left
        anchors.leftMargin: Theme.horizontalPageMargin
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.itemSizeMedium - Theme.paddingSmall
        height: width
        controller: root.controller
        remoteSource: root.hasTrack ? root.track.artworkUrl : ""
        fillMode: Image.PreserveAspectCrop
        sourceSize.width: Math.max(1, Math.round(width))
        sourceSize.height: Math.max(1, Math.round(height))
    }

    Rectangle {
        anchors.fill: artwork
        visible: artwork.status !== Image.Ready
        color: Theme.rgba(Theme.highlightBackgroundColor, 0.24)

        Image {
            anchors.centerIn: parent
            width: Theme.iconSizeSmall
            height: width
            source: "image://theme/icon-m-music"
            opacity: Theme.opacityLow
        }

        BusyIndicator {
            anchors.centerIn: parent
            running: root.artworkLoading
            visible: running
            size: BusyIndicatorSize.Small
        }
    }

    Column {
        id: textColumn
        anchors.left: artwork.right
        anchors.right: transportRow.left
        anchors.leftMargin: Theme.paddingMedium
        anchors.rightMargin: Theme.paddingSmall
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0

        Label {
            width: parent.width
            text: root.hasTrack ? root.track.title : ""
            color: root.highlighted ? Theme.highlightColor : Theme.primaryColor
            truncationMode: TruncationMode.Fade
        }
        Label {
            width: parent.width
            text: root.hasTrack
                  ? root.track.artist
                    + (root.track.album ? " · " + root.track.album : "")
                  : ""
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeExtraSmall
            truncationMode: TruncationMode.Fade
        }
    }

    Row {
        id: transportRow
        objectName: "compactPlayerTransportRow"
        anchors.right: parent.right
        anchors.rightMargin: Theme.paddingSmall
        anchors.verticalCenter: parent.verticalCenter
        width: 4 * Theme.itemSizeSmall
        height: Theme.itemSizeMedium
        spacing: 0

        IconButton {
            objectName: "compactPlayerMode"
            width: Theme.itemSizeSmall
            height: parent.height
            enabled: !!(root.playback && root.playback.queue.length > 0)
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

        IconButton {
            objectName: "compactPlayerPrevious"
            width: Theme.itemSizeSmall
            height: parent.height
            enabled: !!(root.playback && root.playback.canGoPrevious)
            icon.source: "image://theme/icon-m-previous"
            onClicked: root.playback.previous()
        }

        IconButton {
            id: toggle
            objectName: "compactPlayerToggle"
            width: Theme.itemSizeSmall
            height: parent.height
            enabled: !!(root.playback && (root.playback.playing
                                         ? root.playback.canPause
                                         : root.playback.canPlay))
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

        IconButton {
            objectName: "compactPlayerNext"
            width: Theme.itemSizeSmall
            height: parent.height
            enabled: !!(root.playback && root.playback.canGoNext)
            icon.source: "image://theme/icon-m-next"
            onClicked: root.playback.next()
        }
    }

}
