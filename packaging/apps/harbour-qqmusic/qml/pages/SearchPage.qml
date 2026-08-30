import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components" as Components

Page {
    id: page

    property var controller
    property string query: ""
    property bool headlessTest: false
    property bool transitionContentReady: headlessTest
    property int selectedKind: modeSelector.currentIndex
    property bool paginationRequested: false
    readonly property int debounceInterval: 350

    allowedOrientations: Orientation.All

    Timer {
        id: transitionContentTimer
        interval: 24
        repeat: false
        onTriggered: {
            page.transitionContentReady = true
            console.info("zzmusic_performance component=search event=content_ready")
        }
    }

    Timer {
        id: debounce
        interval: page.debounceInterval
        repeat: false
        onTriggered: {
            if (page.controller)
                page.controller.search(page.query)
        }
    }

    Column {
        id: searchHeader
        width: parent.width

        Item {
            objectName: "searchCompactHeader"
            width: parent.width
            height: Theme.itemSizeMedium

            Label {
                anchors.left: parent.left
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.right: parent.right
                anchors.rightMargin: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Search")
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeLarge
                font.weight: Font.DemiBold
                truncationMode: TruncationMode.Fade
            }
        }

        Loader {
            id: searchFieldLoader
            width: parent.width
            height: Theme.itemSizeMedium
            active: page.headlessTest || page.transitionContentReady
            asynchronous: !page.headlessTest
            sourceComponent: page.headlessTest
                             ? headlessSearchComponent : searchComponent
        }

        Item {
            id: modeSelector
            width: parent.width
            height: Theme.itemSizeMedium
            property int currentIndex: page.controller
                                       ? page.controller.searchMode : 0

            Row {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.rightMargin: Theme.horizontalPageMargin
                height: parent.height

                Repeater {
                    model: [qsTr("Tracks"), qsTr("Playlists")]
                    BackgroundItem {
                        width: parent.width / 2
                        height: Theme.itemSizeMedium
                        onClicked: modeSelector.currentIndex = index
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: Math.max(2, Theme.paddingSmall / 3)
                            visible: index === modeSelector.currentIndex
                            color: Theme.highlightColor
                        }
                        Label {
                            anchors.centerIn: parent
                            text: modelData
                            color: index === modeSelector.currentIndex
                                   ? Theme.highlightColor : Theme.primaryColor
                            font.pixelSize: Theme.fontSizeSmall
                            font.weight: index === modeSelector.currentIndex
                                         ? Font.DemiBold : Font.Normal
                        }
                    }
                }
            }

            onCurrentIndexChanged: {
                if (!page.controller)
                    return
                page.controller.searchMode = currentIndex
                if (page.query.trim().length > 0)
                    debounce.restart()
            }
        }
    }

    SilicaListView {
        id: resultList
        objectName: "searchTrackResults"
        anchors.top: searchHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: compactPlayer.visible ? compactPlayer.top : parent.bottom
        visible: modeSelector.currentIndex === 0
        model: page.transitionContentReady && page.controller
               ? page.controller.searchTracks : null
        clip: true
        VerticalScrollDecorator {}

        delegate: Components.TrackDelegate {
            controller: page.controller
            width: resultList.width
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
            onPlayRequested: page.controller.playSearchTrack(index)
            onPlayNextRequested: page.controller.playNextSearchTrack(index)
            onAddToQueueRequested: page.controller.appendSearchTrack(index)
            onViewSourceRequested: Qt.openUrlExternally(sourceUrl)
        }

        onAtYEndChanged: {
            if (atYEnd && count > 0 && !page.paginationRequested) {
                page.paginationRequested = true
                page.controller.loadMoreSearch()
            }
        }
        onCountChanged: page.paginationRequested = false
    }

    SilicaListView {
        id: playlistResults
        anchors.fill: resultList
        visible: modeSelector.currentIndex === 1
        model: page.transitionContentReady && page.controller
               ? page.controller.searchPlaylists : null
        clip: true
        VerticalScrollDecorator {}

        delegate: ListItem {
            width: playlistResults.width
            contentHeight: Theme.itemSizeLarge
            onClicked: pageStack.push(
                Qt.resolvedUrl("PlaylistPage.qml"),
                { "controller": page.controller,
                  "playlistId": model.playlistId })

            Components.CachedArtwork {
                id: resultArtwork
                anchors.left: parent.left
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.itemSizeMedium
                height: width
                controller: page.controller
                remoteSource: model.artwork
                fillMode: Image.PreserveAspectCrop
                sourceSize.width: Math.max(1, Math.round(width))
                sourceSize.height: Math.max(1, Math.round(height))
            }
            Rectangle {
                anchors.fill: resultArtwork
                visible: resultArtwork.status !== Image.Ready
                color: Theme.rgba(Theme.highlightBackgroundColor, 0.18)

                Image {
                    anchors.centerIn: parent
                    width: Theme.iconSizeSmall
                    height: width
                    source: "image://theme/icon-m-music"
                    opacity: Theme.opacityLow
                }
            }
            Column {
                anchors.left: resultArtwork.right
                anchors.right: parent.right
                anchors.leftMargin: Theme.paddingMedium
                anchors.rightMargin: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                Label {
                    width: parent.width
                    text: model.title
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    width: parent.width
                    text: page.playlistResultMeta(model.trackCount,
                                                  model.author)
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeSmall
                    truncationMode: TruncationMode.Fade
                }
            }
        }

        onAtYEndChanged: {
            if (atYEnd && count > 0 && !page.paginationRequested) {
                page.paginationRequested = true
                page.controller.loadMoreSearch()
            }
        }
        onCountChanged: page.paginationRequested = false
    }

    Components.AsyncStateView {
        anchors.top: searchHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: compactPlayer.visible ? compactPlayer.top : parent.bottom
        state: page.controller ? page.controller.searchState : 2
        hasContent: resultList.count > 0 || playlistResults.count > 0
        emptyText: page.query.trim().length === 0
                   ? qsTr("Enter a search term")
                   : qsTr("No matching results")
        errorText: qsTr("Search failed")
        onRetryRequested: page.controller.search(page.query)
    }

    Components.CompactPlayer {
        id: compactPlayer
        objectName: "searchCompactPlayer"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        controller: page.controller
        playback: page.controller ? page.controller.playback : null
        onPlayerRequested: pageStack.push(
            Qt.resolvedUrl("PlayerPage.qml"),
            { "controller": page.controller })
    }

    Component {
        id: searchComponent
        SearchField {
            width: page.width
            text: page.query
            placeholderText: qsTr("Tracks or playlists")
            EnterKey.enabled: text.trim().length > 0
            EnterKey.onClicked: {
                debounce.stop()
                page.controller.search(text)
            }
            onTextChanged: {
                if (page.query !== text)
                    page.query = text
                debounce.restart()
            }
        }
    }

    Component {
        id: headlessSearchComponent
        Item {
            width: page.width
            height: Theme.itemSizeMedium
        }
    }

    function playlistResultMeta(trackCount, author) {
        var authorText = author || qsTr("QQ Music")
        return trackCount > 0
               ? qsTr("%1 tracks").arg(trackCount) + " · " + authorText
               : authorText
    }

    Component.onCompleted: page.activateDeferredContent()
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
