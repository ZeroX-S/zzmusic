import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components" as Components

Page {
    id: page

    property var controller
    property string playlistId: ""
    property bool isToplist: false
    property bool headlessTest: false
    property bool transitionContentReady: headlessTest
    readonly property bool hasSafeSource: controller
                                          && controller.playlistSourceUrl
                                          && controller.playlistSourceUrl.toString().length > 0
    readonly property string toplistPeriodText: isToplist && controller
                                                 ? controller.playlistPeriod : ""
    readonly property bool toplistPeriodVisible: toplistPeriodText.length > 0
    readonly property bool compactHeader: true

    allowedOrientations: Orientation.All

    Timer {
        id: transitionContentTimer
        interval: 24
        repeat: false
        onTriggered: {
            page.transitionContentReady = true
            console.info("zzmusic_performance component=playlist event=content_ready")
        }
    }

    SilicaListView {
        id: trackList
        objectName: "playlistTrackList"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: compactPlayer.visible ? compactPlayer.top : parent.bottom
        model: page.transitionContentReady && page.controller
               ? page.controller.playlistTracks : null
        clip: true
        VerticalScrollDecorator {}

        PullDownMenu {
            MenuItem {
                enabled: trackList.count > 0
                text: qsTr("Play all")
                onClicked: page.controller.playPlaylistAt(0)
            }
            MenuItem {
                visible: page.hasSafeSource
                text: qsTr("View source")
                onClicked: Qt.openUrlExternally(
                               page.controller.playlistSourceUrl)
            }
            MenuItem {
                text: qsTr("Refresh")
                onClicked: page.requestContent()
            }
        }

        header: Item {
            width: trackList.width
            height: Theme.itemSizeMedium + Theme.itemSizeHuge
                    + 2 * Theme.paddingLarge

            Item {
                id: playlistPageHeader
                objectName: "playlistCompactHeader"
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: Theme.itemSizeMedium

                Label {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.horizontalPageMargin
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    text: page.isToplist ? qsTr("Chart") : qsTr("Playlist")
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeLarge
                    font.weight: Font.DemiBold
                    truncationMode: TruncationMode.Fade
                }
            }

            Components.CachedArtwork {
                id: playlistArtwork
                objectName: "playlistDetailArtwork"
                anchors.top: playlistPageHeader.bottom
                anchors.topMargin: Theme.paddingLarge
                anchors.left: parent.left
                anchors.leftMargin: Theme.horizontalPageMargin
                width: Theme.itemSizeHuge
                height: width
                controller: page.controller
                active: page.transitionContentReady
                remoteSource: page.controller
                              ? page.controller.playlistArtwork : ""
                fillMode: Image.PreserveAspectCrop
                sourceSize.width: Math.max(1, Math.round(width))
                sourceSize.height: Math.max(1, Math.round(height))

                Rectangle {
                    anchors.fill: parent
                    visible: playlistArtwork.status !== Image.Ready
                    color: Theme.rgba(Theme.highlightBackgroundColor, 0.2)

                    Image {
                        anchors.centerIn: parent
                        source: "image://theme/icon-m-music"
                        opacity: Theme.opacityLow
                    }

                    BusyIndicator {
                        objectName: "playlistDetailArtworkBusy"
                        anchors.centerIn: parent
                        running: page.controller
                                 && page.controller.playlistArtwork
                                 && playlistArtwork.status !== Image.Ready
                                 && playlistArtwork.status !== Image.Error
                        visible: running
                        size: BusyIndicatorSize.Small
                    }
                }
            }

            Column {
                id: headerColumn
                anchors.left: playlistArtwork.right
                anchors.right: parent.right
                anchors.leftMargin: Theme.paddingLarge
                anchors.rightMargin: Theme.horizontalPageMargin
                anchors.verticalCenter: playlistArtwork.verticalCenter
                spacing: Theme.paddingSmall

                Label {
                    width: parent.width
                    text: page.controller && page.controller.playlistTitle
                          ? page.controller.playlistTitle
                          : (page.isToplist ? qsTr("Chart") : qsTr("Playlist"))
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeMedium
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    width: parent.width
                    text: page.controller
                          ? qsTr("%1 tracks · %2")
                            .arg(page.controller.playlistTrackCount)
                            .arg(page.controller.playlistAuthor)
                          : ""
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    id: playlistPeriodLabel
                    objectName: "playlistPeriodLabel"
                    width: parent.width
                    visible: page.toplistPeriodVisible
                    text: page.toplistPeriodText
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    truncationMode: TruncationMode.Fade
                }
                Button {
                    objectName: "playlistPlayAllButton"
                    enabled: trackList.count > 0
                    text: qsTr("Play all")
                    onClicked: page.controller.playPlaylistAt(0)
                }
            }
        }

        delegate: Components.TrackDelegate {
            controller: page.controller
            width: trackList.width
            trackMid: model.trackMid
            title: model.title
            artist: model.artist
            album: model.album
            artwork: model.artwork
            durationMs: model.durationMs
            playable: model.playable
            sourceUrl: model.sourceUrl
            current: page.controller && page.controller.playback
                     && page.controller.playback.currentTrackValue.mid
                        === model.trackMid
            onPlayRequested: page.controller.playPlaylistAt(index)
            onPlayNextRequested: page.controller.playNextPlaylistTrack(index)
            onAddToQueueRequested: page.controller.appendPlaylistTrack(index)
            onViewSourceRequested: Qt.openUrlExternally(sourceUrl)
        }

    }

    Components.AsyncStateView {
        anchors.fill: parent
        state: page.controller ? page.controller.playlistState : 2
        hasContent: trackList.count > 0
        emptyText: qsTr("This playlist has no playable tracks")
        errorText: qsTr("Unable to load this playlist")
        onRetryRequested: page.requestContent()
    }

    Components.CompactPlayer {
        id: compactPlayer
        objectName: "playlistCompactPlayer"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        controller: page.controller
        playback: page.controller ? page.controller.playback : null
        onPlayerRequested: pageStack.push(
            Qt.resolvedUrl("PlayerPage.qml"),
            { "controller": page.controller })
    }

    Component.onCompleted: {
        page.activateDeferredContent()
        if (controller && playlistId.length > 0)
            requestContent()
    }

    onStatusChanged: page.activateDeferredContent()

    function activateDeferredContent() {
        if (headlessTest) {
            transitionContentReady = true
            return
        }
        if (status === PageStatus.Active && !transitionContentReady
                && !transitionContentTimer.running)
            transitionContentTimer.start()
    }

    function requestContent() {
        if (!controller || playlistId.length === 0)
            return
        if (isToplist)
            controller.openToplist(playlistId)
        else
            controller.openPlaylist(playlistId)
    }

}
