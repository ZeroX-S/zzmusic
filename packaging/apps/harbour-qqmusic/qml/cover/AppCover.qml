import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components" as Components

CoverBackground {
    id: cover

    property var controller
    property var playback: controller ? controller.playback : null
    property var track: playback ? playback.currentTrackValue : ({})
    property bool hasTrack: !!(track && track.mid)
    property bool canToggle: !!(playback && (playback.playing
                                             ? playback.canPause
                                             : playback.canPlay))
    property bool useLightText: true
    property bool artworkContrastResolved: false
    readonly property bool textContrastAdaptive: true
    readonly property bool coverActionTintAdaptive: true
    readonly property int visibleCoverActionCount: 2
    readonly property string coverActionColorHex:
        useLightText ? "#fffafafa" : "#ff151515"
    readonly property color coverPrimaryTextColor:
        coverArtwork.status === Image.Ready
        ? (useLightText ? "#fffafafa" : "#ff151515")
        : Theme.primaryColor
    readonly property color coverSecondaryTextColor:
        coverArtwork.status === Image.Ready
        ? (useLightText ? "#d9ffffff" : "#d9000000")
        : Theme.secondaryColor
    readonly property color coverTextShadowColor:
        useLightText ? "#b0000000" : "#b0ffffff"
    readonly property real progressFraction: playback && playback.durationMs > 0
                                             ? Math.max(0, Math.min(
                                                 1, playback.positionMs
                                                 / playback.durationMs)) : 0
    readonly property real actionReserveHeight: hasTrack
                                                ? Theme.itemSizeLarge : 0
    readonly property bool contentBoundsValid: coverMetadata.y >= 0
                                               && coverMetadata.y
                                                  + coverMetadata.height
                                                  <= cover.height
    readonly property bool metadataClearsActions:
        coverMetadata.y + coverMetadata.height <= coverProgressTrack.y
        && coverProgressTrack.y + coverProgressTrack.height
           <= coverActionSpace.y
    readonly property int titleLineLimit: 2

    Components.CachedArtwork {
        id: coverArtwork
        objectName: "coverArtwork"
        visible: cover.hasTrack
                 && track.artworkUrl.toString().length > 0
        anchors.fill: parent
        controller: cover.controller
        remoteSource: visible ? track.artworkUrl : ""
        fillMode: Image.PreserveAspectCrop
        sourceSize.width: Math.max(1, Math.round(width))
        sourceSize.height: Math.max(1, Math.round(height))
        onStatusChanged: cover.requestArtworkContrast()
    }

    Rectangle {
        anchors.fill: parent
        visible: coverArtwork.status !== Image.Ready
        color: Theme.rgba(Theme.highlightBackgroundColor, 0.2)

        Image {
            anchors.centerIn: parent
            width: Theme.iconSizeLarge
            height: width
            source: "image://theme/icon-m-music"
            opacity: Theme.opacityLow
        }

        BusyIndicator {
            anchors.centerIn: parent
            running: coverArtwork.visible
                     && coverArtwork.status !== Image.Ready
                     && coverArtwork.status !== Image.Error
            visible: running
            size: BusyIndicatorSize.Small
        }
    }

    Column {
        id: coverMetadata
        objectName: "coverMetadata"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: coverProgressTrack.top
        anchors.leftMargin: Theme.paddingMedium
        anchors.rightMargin: Theme.paddingMedium
        anchors.bottomMargin: Theme.paddingMedium
        spacing: Theme.paddingSmall

        Label {
            objectName: "coverTrackTitle"
            width: parent.width
            text: cover.hasTrack ? track.title : qsTr("ZZ Music")
            color: cover.coverPrimaryTextColor
            style: Text.Raised
            styleColor: cover.coverTextShadowColor
            font.pixelSize: Theme.fontSizeMedium
            font.weight: Font.DemiBold
            truncationMode: TruncationMode.Fade
            maximumLineCount: cover.titleLineLimit
            wrapMode: Text.Wrap
        }

        Label {
            objectName: "coverTrackArtist"
            visible: cover.hasTrack
            width: parent.width
            text: track.artist || ""
            color: cover.coverSecondaryTextColor
            style: Text.Raised
            styleColor: cover.coverTextShadowColor
            font.pixelSize: Theme.fontSizeSmall
            truncationMode: TruncationMode.Fade
        }
    }

    Rectangle {
        id: coverProgressTrack
        objectName: "coverProgressTrack"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: coverActionSpace.top
        height: Math.max(2, Theme.paddingSmall / 3)
        color: Theme.rgba(Theme.secondaryColor, 0.25)

        Rectangle {
            objectName: "coverPlaybackProgress"
            width: parent.width * cover.progressFraction
            height: parent.height
            color: Theme.highlightColor
        }
    }

    Item {
        id: coverActionSpace
        objectName: "coverActionSpace"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: cover.actionReserveHeight
    }

    CoverActionList {
        id: primaryCoverActions
        objectName: "primaryCoverActions"
        enabled: cover.hasTrack
        iconBackground: false

        CoverAction {
            objectName: "coverPlayPauseAction"
            iconSource: cover.canToggle
                        ? (playback.playing
                           ? cover.coverActionIcon("icon-cover-pause")
                           : cover.coverActionIcon("icon-cover-play"))
                        : ""
            onTriggered: {
                if (!cover.canToggle)
                    return
                if (playback.playing)
                    playback.pause()
                else
                    playback.play()
            }
        }

        CoverAction {
            objectName: "coverNextAction"
            iconSource: playback && playback.canGoNext
                        ? cover.coverActionIcon("icon-cover-next-song") : ""
            onTriggered: cover.nextTrack()
        }
    }

    Timer {
        id: artworkContrastTimer
        interval: 0
        repeat: false
        onTriggered: cover.requestArtworkContrast()
    }

    Connections {
        target: cover.controller
        ignoreUnknownSignals: true

        onArtworkForegroundAvailable: {
            if (cover.hasTrack
                    && String(remoteUrl) === String(cover.track.artworkUrl)) {
                cover.useLightText = lightText
                cover.artworkContrastResolved = true
            }
        }
    }

    onTrackChanged: {
        resetArtworkContrast()
    }

    function coverActionIcon(name) {
        var source = "image://theme/" + name
        return artworkContrastResolved
                ? source + "?" + coverActionColorHex : source
    }

    function nextTrack() {
        if (playback && playback.canGoNext)
            playback.next()
    }

    function resetArtworkContrast() {
        useLightText = true
        artworkContrastResolved = false
        artworkContrastTimer.restart()
    }

    function requestArtworkContrast() {
        if (!hasTrack || coverArtwork.status !== Image.Ready
                || !controller
                || typeof controller.requestArtworkForeground !== "function")
            return
        controller.requestArtworkForeground(track.artworkUrl)
    }

}
