import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components" as Components

Page {
    id: page

    property var controller
    property bool headlessTest: false
    property var playback: controller ? controller.playback : null
    property var track: playback ? playback.currentTrackValue : ({})
    property string lastArtworkState: ""
    property int requestedPortraitSlide: 0
    property bool queuePanelRequested: false
    property bool transitionContentReady: headlessTest
    readonly property bool hasTrack: !!(track && track.mid)
    readonly property string trackAlbumText: hasTrack && track.album
                                                ? track.album : ""
    readonly property bool artworkDecodeAsynchronous: true
    readonly property bool landscapeLayoutActive: width > height
    readonly property bool usesScaleTransform: false
    readonly property real minimumTouchTarget: Theme.itemSizeSmall
    readonly property bool compactHeader: true
    readonly property real portraitMetadataExtent: Math.max(
                                                        Theme.itemSizeLarge,
                                                        metadataColumn.height
                                                        + 2 * Theme.paddingSmall)
    readonly property real portraitArtworkExtent: Math.max(
                                                       0, Math.min(
                                                           width
                                                           - 2 * Theme.horizontalPageMargin,
                                                           height
                                                           - portraitHeader.height
                                                           - portraitMetadataExtent
                                                           - progressFrame.height
                                                           - portraitControls.height
                                                           - portraitAudioDetailsFrame.height
                                                           - 5 * Theme.paddingSmall))
    readonly property real portraitStackExtent: portraitArtworkExtent
                                                 + portraitMetadataExtent
                                                 + progressFrame.height
                                                 + portraitControls.height
                                                 + portraitAudioDetailsFrame.height
                                                 + 4 * Theme.paddingSmall
    readonly property real portraitStackTop: portraitHeader.height
                                             + Math.max(
                                                 Theme.paddingSmall,
                                                 (height
                                                  - portraitHeader.height
                                                  - portraitStackExtent) / 2)
    readonly property real portraitArtworkToControlsGap:
        portraitControls.y - (slideshowFrame.y + slideshowFrame.height)
    readonly property bool portraitStackCompact:
        portraitArtworkToControlsGap <= portraitMetadataExtent
                                        + progressFrame.height
                                        + 3 * Theme.paddingSmall
    readonly property string playbackPositionText: formatPosition(
                                                       playback
                                                       ? playback.positionMs : 0)
    readonly property string playbackDurationText: formatPosition(
                                                       playback
                                                       ? playback.durationMs : 0)
    readonly property string playbackTimeText: playbackPositionText + " / "
                                               + playbackDurationText
    readonly property string audioBitrateText: formatBitrate(
                                                   playback
                                                   ? playback.audioBitrateKbps
                                                   : 0,
                                                   playback
                                                   ? playback.audioLossless
                                                   : false)
    readonly property int portraitSlideIndex: slideshowLoader.item
                                               && slideshowLoader.item.currentIndex
                                                  !== undefined
                                               ? slideshowLoader.item.currentIndex
                                               : requestedPortraitSlide
    readonly property bool layoutBoundsValid: landscapeLayoutActive
                                              ? landscapeControls.y
                                                + landscapeControls.height
                                                + Theme.paddingSmall
                                                === landscapeAudioDetailsFrame.y
                                                && landscapeAudioDetailsFrame.y
                                                   + landscapeAudioDetailsFrame.height
                                                   <= landscapeControls.parent.height
                                                && landscapeSliderFrame.y
                                                   + landscapeSliderFrame.height
                                                   <= landscapeControls.y
                                              : portraitHeader.height
                                                === Theme.itemSizeMedium
                                                && slideshowFrame.height >= 0
                                                && slideshowFrame.y
                                                   + slideshowFrame.height
                                                   <= metadataFrame.y
                                                && metadataColumn.y >= 0
                                                && metadataColumn.y
                                                   + metadataColumn.height
                                                   <= metadataFrame.height
                                                && progressFrame.y
                                                   + progressFrame.height
                                                   === portraitControls.y
                                                && portraitControls.y
                                                   + portraitControls.height
                                                   + Theme.paddingSmall
                                                   === portraitAudioDetailsFrame.y
                                                && portraitAudioDetailsFrame.y
                                                   + portraitAudioDetailsFrame.height
                                                   <= portraitLayout.height
                                                && portraitStackCompact

    allowedOrientations: Orientation.All

    Timer {
        id: transitionContentTimer
        interval: 24
        repeat: false
        onTriggered: {
            page.transitionContentReady = true
            console.info("zzmusic_performance component=player event=content_ready")
        }
    }

    Item {
        id: portraitLayout
        anchors.fill: parent
        visible: !page.landscapeLayoutActive

        Item {
            id: portraitHeader
            objectName: "playerCompactHeader"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.itemSizeMedium

            Label {
                anchors.left: parent.left
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.right: playerViewTabs.left
                anchors.rightMargin: Theme.paddingMedium
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Now playing")
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeLarge
                font.weight: Font.DemiBold
                truncationMode: TruncationMode.Fade
            }

            Row {
                id: playerViewTabs
                objectName: "playerViewTabs"
                anchors.right: parent.right
                anchors.rightMargin: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                height: Theme.itemSizeSmall

                Repeater {
                    model: [qsTr("Cover"), qsTr("Lyrics")]

                    BackgroundItem {
                        objectName: index === 0
                                    ? "playerCoverTab" : "playerLyricsTab"
                        width: Theme.itemSizeMedium
                        height: Theme.itemSizeSmall
                        onClicked: page.selectPortraitSlide(index)

                        Label {
                            anchors.centerIn: parent
                            text: modelData
                            color: index === page.portraitSlideIndex
                                   ? Theme.highlightColor
                                   : Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            font.weight: index === page.portraitSlideIndex
                                         ? Font.DemiBold : Font.Normal
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: Math.max(2, Theme.paddingSmall / 3)
                            visible: index === page.portraitSlideIndex
                            color: Theme.highlightColor
                        }
                    }
                }
            }
        }

        Item {
            id: slideshowFrame
            objectName: "playerPortraitMediaFrame"
            anchors.top: parent.top
            anchors.topMargin: page.portraitStackTop
            anchors.left: parent.left
            anchors.leftMargin: Theme.horizontalPageMargin
            anchors.right: parent.right
            anchors.rightMargin: Theme.horizontalPageMargin
            height: page.portraitArtworkExtent
            clip: true

            Loader {
                id: slideshowLoader
                anchors.fill: parent
                active: !page.landscapeLayoutActive
                        && page.transitionContentReady
                asynchronous: !page.headlessTest
                sourceComponent: page.headlessTest
                                 ? headlessSlideshow
                                 : productionSlideshow
            }
        }

        Row {
            objectName: "playerSlideIndicator"
            anchors.horizontalCenter: slideshowFrame.horizontalCenter
            anchors.top: slideshowFrame.bottom
            anchors.topMargin: Theme.paddingSmall
            spacing: Theme.paddingSmall
            visible: !page.headlessTest && page.hasTrack
            z: 2

            Repeater {
                model: 2
                Rectangle {
                    width: index === page.portraitSlideIndex
                           ? Theme.paddingLarge : Theme.paddingMedium
                    height: Math.max(3, Theme.paddingSmall / 2)
                    color: index === page.portraitSlideIndex
                           ? Theme.highlightColor
                           : Theme.rgba(Theme.secondaryColor, 0.45)
                }
            }
        }

        Component {
            id: productionSlideshow
            SlideshowView {
                id: productionView
                itemWidth: width
                itemHeight: height
                clip: true
                model: 2
                currentIndex: page.requestedPortraitSlide

                onCurrentIndexChanged: {
                    if (page.requestedPortraitSlide !== currentIndex)
                        page.requestedPortraitSlide = currentIndex
                }

                delegate: Item {
                    width: productionView.itemWidth
                    height: productionView.itemHeight

                    Loader {
                        anchors.fill: parent
                        anchors.margins: Theme.paddingMedium
                        active: page.transitionContentReady
                                && index === productionView.currentIndex
                        asynchronous: true
                        sourceComponent: index === 0
                                         ? artworkComponent : lyricComponent
                    }
                }
            }
        }

        Component {
            id: headlessSlideshow
            Item {
                Loader {
                    anchors.fill: parent
                    anchors.margins: Theme.paddingMedium
                    sourceComponent: artworkComponent
                }
            }
        }

        Item {
            id: metadataFrame
            objectName: "playerPortraitMetadataFrame"
            anchors.top: slideshowFrame.bottom
            anchors.topMargin: Theme.paddingSmall
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.horizontalPageMargin
            anchors.rightMargin: Theme.horizontalPageMargin
            height: page.portraitMetadataExtent

            Column {
                id: metadataColumn
                width: parent.width
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.paddingSmall

                Label {
                    width: parent.width
                    text: page.hasTrack ? page.track.title
                                        : qsTr("Nothing playing")
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeMedium
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    width: parent.width
                    text: page.hasTrack ? page.track.artist : ""
                    color: Theme.primaryColor
                    font.pixelSize: Theme.fontSizeSmall
                    horizontalAlignment: Text.AlignHCenter
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    width: parent.width
                    visible: page.trackAlbumText.length > 0
                    text: page.trackAlbumText
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    horizontalAlignment: Text.AlignHCenter
                    truncationMode: TruncationMode.Fade
                }
            }
        }

        Item {
            id: progressFrame
            objectName: "playerPortraitProgressFrame"
            anchors.top: metadataFrame.bottom
            anchors.topMargin: Theme.paddingSmall
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.horizontalPageMargin
            anchors.rightMargin: Theme.horizontalPageMargin
            height: Theme.itemSizeMedium
            clip: true

            Loader {
                anchors.fill: parent
                active: page.headlessTest || page.transitionContentReady
                sourceComponent: page.headlessTest
                                 ? headlessSlider : playbackSlider
            }
        }

        Components.TransportControls {
            id: portraitControls
            objectName: "playerPortraitTransportControls"
            anchors.top: progressFrame.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            playback: page.playback
            onQueueRequested: page.showQueue()
        }

        Item {
            id: portraitAudioDetailsFrame
            objectName: "playerPortraitAudioDetailsFrame"
            anchors.top: portraitControls.bottom
            anchors.topMargin: Theme.paddingSmall
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.horizontalPageMargin
            anchors.rightMargin: Theme.horizontalPageMargin
            height: Theme.itemSizeSmall

            Loader {
                anchors.fill: parent
                active: !page.landscapeLayoutActive
                sourceComponent: audioDetailsBar
            }
        }
    }

    Item {
        id: landscapeLayout
        anchors.fill: parent
        visible: page.landscapeLayoutActive

        Row {
            anchors.fill: parent
            anchors.margins: Theme.paddingLarge
            spacing: Theme.paddingLarge

            Item {
                width: parent.width * 0.42
                height: parent.height

                Loader {
                    anchors.fill: parent
                    active: page.landscapeLayoutActive
                            && page.transitionContentReady
                    asynchronous: !page.headlessTest
                    sourceComponent: artworkComponent
                }
            }

            Item {
                width: parent.width - parent.spacing - parent.width * 0.42
                height: parent.height

                Column {
                    id: landscapeMetadata
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: Theme.paddingSmall

                    Label {
                        width: parent.width
                        text: page.hasTrack ? page.track.title
                                         : qsTr("Nothing playing")
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeMedium
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        truncationMode: TruncationMode.Fade
                    }
                    Label {
                        width: parent.width
                        text: page.hasTrack ? page.track.artist : ""
                        color: Theme.primaryColor
                        font.pixelSize: Theme.fontSizeSmall
                        truncationMode: TruncationMode.Fade
                    }
                    Label {
                        width: parent.width
                        visible: page.trackAlbumText.length > 0
                        text: page.trackAlbumText
                        color: Theme.secondaryColor
                        opacity: Theme.opacityHigh
                        font.pixelSize: Theme.fontSizeExtraSmall
                        truncationMode: TruncationMode.Fade
                    }
                }

                Loader {
                    anchors.top: landscapeMetadata.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: landscapeSliderFrame.top
                    active: page.landscapeLayoutActive
                            && page.transitionContentReady
                    asynchronous: !page.headlessTest
                    sourceComponent: lyricComponent
                }

                Item {
                    id: landscapeSliderFrame
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: landscapeControls.top
                    height: Theme.itemSizeMedium

                    Loader {
                        anchors.fill: parent
                        active: page.landscapeLayoutActive
                                && (page.headlessTest
                                    || page.transitionContentReady)
                        sourceComponent: page.headlessTest
                                         ? headlessSlider : playbackSlider
                    }
                }

                Components.TransportControls {
                    id: landscapeControls
                    objectName: "playerLandscapeTransportControls"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: landscapeAudioDetailsFrame.top
                    anchors.bottomMargin: Theme.paddingSmall
                    playback: page.playback
                    onQueueRequested: page.showQueue()
                }

                Item {
                    id: landscapeAudioDetailsFrame
                    objectName: "playerLandscapeAudioDetailsFrame"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: Theme.itemSizeSmall

                    Loader {
                        anchors.fill: parent
                        active: page.landscapeLayoutActive
                        sourceComponent: audioDetailsBar
                    }
                }
            }
        }
    }

    Loader {
        id: queuePanelLoader
        active: !page.headlessTest && page.queuePanelRequested
        sourceComponent: Component {
            Components.QueuePanel {
                parent: page
                width: page.width
                height: Math.min(page.height * 0.72,
                                 Theme.itemSizeHuge * 5)
                controller: page.controller
            }
        }
    }

    Component {
        id: playbackSlider
        Item {
            Row {
                id: playbackTimeRow
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: Theme.fontSizeExtraSmall + Theme.paddingSmall

                Label {
                    objectName: "playerPositionLabel"
                    width: parent.width / 2
                    text: page.playbackPositionText
                    color: Theme.primaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                }
                Label {
                    objectName: "playerDurationLabel"
                    width: parent.width / 2
                    text: page.playbackDurationText
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    horizontalAlignment: Text.AlignRight
                }
            }

            Slider {
                objectName: "playerProgressSlider"
                anchors.top: playbackTimeRow.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                minimumValue: 0
                maximumValue: page.playback
                              ? Math.max(1, page.playback.durationMs) : 1
                value: page.playback ? page.playback.positionMs : 0
                enabled: page.playback && page.playback.durationMs > 0
                label: ""
                valueText: ""
                onReleased: {
                    if (page.playback)
                        page.playback.seek(value)
                }
            }
        }
    }

    Component {
        id: headlessSlider
        Item {}
    }

    Component {
        id: audioDetailsBar

        Item {
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.rgba(Theme.secondaryColor, 0.22)
            }

            Row {
                anchors.centerIn: parent
                height: parent.height
                spacing: Theme.paddingMedium

                Label {
                    objectName: "playerAudioBitrateLabel"
                    height: parent.height
                    text: qsTr("Bitrate")
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    objectName: "playerAudioBitrateValue"
                    height: parent.height
                    text: page.audioBitrateText
                    color: page.playback && page.playback.audioLossless
                           ? Theme.highlightColor : Theme.primaryColor
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: Font.DemiBold
                    verticalAlignment: Text.AlignVCenter
                    truncationMode: TruncationMode.Fade
                }
            }
        }
    }

    Component {
        id: lyricComponent
        Components.LyricView {
            lyrics: page.controller ? page.controller.lyrics : null
            playback: page.playback
            state: page.controller ? page.controller.lyricState : 2
        }
    }

    Component {
        id: artworkComponent

        Item {
            id: artworkRoot

            property url remoteUrl: page.track && page.track.artworkUrl
                                    ? page.track.artworkUrl : ""
            readonly property string loadState: remoteUrl.toString().length === 0
                                                ? "empty"
                                      : artworkImage.resolvedSource.toString().length === 0
                                                || artworkImage.status === Image.Loading
                                                || artworkImage.status === Image.Null
                                                ? "loading"
                                      : artworkImage.status === Image.Ready
                                                ? "ready" : "error"

            Rectangle {
                anchors.fill: parent
                color: Theme.rgba(Theme.highlightBackgroundColor, 0.16)

                Image {
                    anchors.centerIn: parent
                    width: Theme.iconSizeLarge
                    height: width
                    source: "image://theme/icon-m-music"
                    opacity: Theme.opacityLow
                }
            }

            Components.CachedArtwork {
                id: artworkImage
                objectName: "playerArtworkImage"
                anchors.fill: parent
                controller: page.controller
                remoteSource: artworkRoot.remoteUrl
                fillMode: Image.PreserveAspectCrop
                asynchronous: page.artworkDecodeAsynchronous
                sourceSize.width: Math.max(1, Math.round(width))
                sourceSize.height: Math.max(1, Math.round(height))
            }

            BusyIndicator {
                objectName: "playerArtworkBusyIndicator"
                anchors.centerIn: parent
                running: artworkRoot.loadState === "loading"
                visible: running
                size: BusyIndicatorSize.Medium
            }

            onLoadStateChanged: page.reportArtworkState(loadState)
            Component.onCompleted: page.reportArtworkState(loadState)
        }
    }

    function formatPosition(milliseconds) {
        var seconds = Math.max(0, Math.floor(milliseconds / 1000))
        return Math.floor(seconds / 60) + ":"
               + ("0" + (seconds % 60)).slice(-2)
    }

    function formatBitrate(bitrateKbps, lossless) {
        if (bitrateKbps && bitrateKbps > 0)
            return bitrateKbps + " kbps"
                   + (lossless ? " · " + qsTr("Lossless") : "")
        return lossless ? qsTr("Lossless") : "—"
    }

    function reportArtworkState(state) {
        if (state === lastArtworkState)
            return
        lastArtworkState = state
        console.info("qqmusic_player component=qml event=artwork_state state="
                     + state)
    }

    function reportLyricState() {
        var state = controller ? controller.lyricState : -1
        var count = controller && controller.lyrics
                  ? controller.lyrics.count : 0
        console.info("qqmusic_player component=qml event=lyrics_state state="
                     + state + " count=" + count)
    }

    function showQueue() {
        if (!queuePanelRequested) {
            queuePanelRequested = true
            Qt.callLater(showQueue)
        } else if (queuePanelLoader.item) {
            queuePanelLoader.item.show()
        }
    }

    function selectPortraitSlide(index) {
        requestedPortraitSlide = Math.max(0, Math.min(1, index))
        if (slideshowLoader.item
                && slideshowLoader.item.currentIndex !== undefined)
            slideshowLoader.item.currentIndex = requestedPortraitSlide
    }

    Connections {
        target: page.controller
        onLyricStateChanged: page.reportLyricState()
    }

    Component.onCompleted: {
        console.info("qqmusic_player component=qml event=page_open layout="
                     + (landscapeLayoutActive ? "landscape" : "portrait")
                     + " has_track=" + Boolean(track && track.mid))
        page.activateDeferredContent()
        reportLyricState()
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
}
