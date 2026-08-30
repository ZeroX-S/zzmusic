import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components" as Components

Page {
    id: page

    property var controller
    property bool headlessTest: false
    property bool embedded: false
    property bool autoRefresh: true
    readonly property var toplists: page.controller
                                   ? page.controller.toplists : []
    readonly property bool hasToplistContent: toplists.count > 0
    readonly property int toplistSectionCount: hasToplistContent ? 1 : 0
    readonly property int primaryTabIndex: 2
    signal recommendedRequested()
    signal discoverRequested()
    signal toplistRequested(string toplistId)

    allowedOrientations: Orientation.All

    SilicaFlickable {
        id: flickable
        objectName: "topChartsContentFlickable"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: compactPlayer.visible ? compactPlayer.top : parent.bottom
        contentHeight: contentColumn.height + Theme.paddingLarge
        clip: true

        PullDownMenu {
            objectName: "topChartsPullDownMenu"
            highlightColor: active
                            ? Theme.highlightBackgroundColor
                            : "transparent"

            MenuItem {
                objectName: "topChartsRefreshMenuItem"
                text: qsTr("Refresh")
                onClicked: page.refreshContent()
            }
        }

        VerticalScrollDecorator {}

        Column {
            id: contentColumn
            width: parent.width
            spacing: Theme.paddingMedium

            Item {
                objectName: "topChartsCompactHeader"
                width: parent.width
                height: visible ? Theme.itemSizeMedium : 0
                visible: !page.embedded

                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: Theme.horizontalPageMargin
                    anchors.rightMargin: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 0

                    Label {
                        width: parent.width
                        text: qsTr("ZZ Music")
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeLarge
                        font.weight: Font.DemiBold
                        truncationMode: TruncationMode.Fade
                    }
                    Label {
                        width: parent.width
                        text: qsTr("Updated charts from QQ Music")
                        color: Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                        truncationMode: TruncationMode.Fade
                    }
                }
            }

            Components.PrimaryTabs {
                objectName: "topChartsPrimaryTabs"
                width: parent.width
                currentIndex: page.primaryTabIndex
                objectNamePrefix: "topCharts"
                recommendedText: qsTr("Recommended")
                discoverText: qsTr("Discover")
                topChartsText: qsTr("Top charts")
                visible: !page.embedded
                height: visible ? Theme.itemSizeMedium : 0
                onRecommendedClicked: page.recommendedRequested()
                onDiscoverClicked: page.discoverRequested()
            }

            Components.SectionTitle {
                objectName: "topChartsSection"
                width: parent.width
                visible: page.hasToplistContent
                text: qsTr("Top charts")
                detail: page.hasToplistContent
                        ? qsTr("%1 charts").arg(toplists.count) : ""
            }

            Components.PlaylistGrid {
                id: toplistGrid
                objectName: "topChartsGrid"
                controller: page.controller
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                columns: page.width > page.height ? 3 : 2
                model: page.toplists
                viewportContentY: flickable.contentY
                viewportHeight: flickable.height
                visible: count > 0
                height: count > 0 ? Math.ceil(count / columns) * cellHeight : 0
                onPlaylistClicked: page.openToplist(playlistId)
            }
        }

        Components.AsyncStateView {
            id: topChartsStateView
            objectName: "topChartsStateView"
            anchors.fill: parent
            state: page.controller ? page.controller.discoverState : 2
            hasContent: page.hasToplistContent
                        && topChartsStateView.state !== 4
            emptyText: qsTr("No charts are available")
            errorText: qsTr("Unable to load top charts")
            onRetryRequested: page.refreshContent()
        }
    }

    Components.CompactPlayer {
        id: compactPlayer
        objectName: "topChartsCompactPlayer"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        controller: page.controller
        playback: page.controller ? page.controller.playback : null
        visible: !page.embedded && compactPlayer.hasTrack
        onPlayerRequested: pageStack.push(
            Qt.resolvedUrl("PlayerPage.qml"),
            { "controller": page.controller })
    }

    Component.onCompleted: {
        console.info("zzmusic_top_charts component=qml event=page_open")
        if (page.autoRefresh)
            page.refreshContent()
    }

    function refreshContent() {
        if (page.controller
                && typeof page.controller.refreshDiscover === "function")
            page.controller.refreshDiscover()
    }

    function openToplist(toplistId) {
        toplistRequested(toplistId)
        if (!headlessTest) {
            pageStack.push(Qt.resolvedUrl("PlaylistPage.qml"),
                           { "controller": page.controller,
                             "playlistId": toplistId,
                             "isToplist": true })
        }
    }
}
