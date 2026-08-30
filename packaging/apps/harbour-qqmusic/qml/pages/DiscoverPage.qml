import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components" as Components

Page {
    id: page

    property var controller
    property bool headlessTest: false
    property bool embedded: false
    property bool autoRefresh: true
    property int contentStage: headlessTest ? 2 : 0
    readonly property bool categoryContentReady: contentStage >= 1
    readonly property bool publicPlaylistContentReady: contentStage >= 2
    readonly property var categoryGroups: page.controller
                                          && page.controller.categoryGroups
                                          ? page.controller.categoryGroups : []
    readonly property var publicPlaylists: page.controller
                                           ? page.controller.publicPlaylists : []
    readonly property int categorySectionCount: categoryGroups.length
    readonly property int publicPlaylistSectionCount: publicPlaylists.count > 0 ? 1 : 0
    readonly property int toplistSectionCount: 0
    readonly property bool hasDiscoverContent: page.categoryItemCount() > 0
                                                || publicPlaylists.count > 0
    readonly property int primaryTabIndex: 1
    signal recommendedRequested()
    signal topChartsRequested()
    signal playlistRequested(string playlistId)

    allowedOrientations: Orientation.All

    Timer {
        id: contentStageTimer
        interval: 24
        repeat: false
        onTriggered: {
            if (page.contentStage < 2) {
                ++page.contentStage
                console.info("zzmusic_performance component=discover event=content_stage stage="
                             + page.contentStage)
            }
            if (page.contentStage < 2)
                restart()
        }
    }

    SilicaFlickable {
        id: flickable
        objectName: "discoverContentFlickable"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: compactPlayer.visible ? compactPlayer.top : parent.bottom
        contentHeight: contentColumn.height + Theme.paddingLarge
        clip: true

        PullDownMenu {
            objectName: "discoverPullDownMenu"
            highlightColor: active
                            ? Theme.highlightBackgroundColor
                            : "transparent"

            MenuItem {
                objectName: "discoverRefreshMenuItem"
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
                objectName: "discoverCompactHeader"
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
                        text: qsTr("Explore languages, styles, and more")
                        color: Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                        truncationMode: TruncationMode.Fade
                    }
                }
            }

            Components.PrimaryTabs {
                id: primaryTabs
                objectName: "discoverPrimaryTabs"
                width: parent.width
                currentIndex: page.primaryTabIndex
                objectNamePrefix: "discover"
                recommendedText: qsTr("Recommended")
                discoverText: qsTr("Discover")
                topChartsText: qsTr("Top charts")
                visible: !page.embedded
                height: visible ? Theme.itemSizeMedium : 0
                onRecommendedClicked: page.openRecommended()
                onTopChartsClicked: page.topChartsRequested()
            }

            Repeater {
                id: categoryRepeater
                objectName: "discoverCategoryGroups"
                model: page.categoryContentReady ? page.categoryGroups : []

                delegate: Item {
                    id: categoryGroup
                    objectName: "categoryGroup_" + index
                    width: contentColumn.width
                    height: groupTitle.height + categoryStrip.height
                            + Theme.paddingSmall

                    Label {
                        id: groupTitle
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: Theme.horizontalPageMargin
                        anchors.rightMargin: Theme.horizontalPageMargin
                        height: Theme.itemSizeSmall
                        text: modelData.title || ""
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeMedium
                        font.weight: Font.DemiBold
                        truncationMode: TruncationMode.Fade
                    }

                    SilicaListView {
                        id: categoryStrip
                        objectName: "categoryStrip_" + index
                        anchors.top: groupTitle.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: Theme.horizontalPageMargin
                        anchors.rightMargin: Theme.horizontalPageMargin
                        height: Theme.itemSizeSmall
                        orientation: ListView.Horizontal
                        spacing: Theme.paddingSmall
                        clip: true
                        interactive: contentWidth > width
                        boundsBehavior: Flickable.StopAtBounds
                        model: modelData.items || []

                        delegate: BackgroundItem {
                            id: categoryItem
                            objectName: "categoryItem_"
                                     + String(modelData.categoryId)
                            width: Math.max(Theme.itemSizeMedium,
                                            categoryLabel.implicitWidth
                                            + 2 * Theme.paddingMedium)
                            height: categoryStrip.height
                            onClicked: page.selectCategory(
                                           String(modelData.categoryId))

                            Rectangle {
                                anchors.fill: parent
                                radius: height / 2
                                color: categoryItem.highlighted
                                       ? Theme.rgba(Theme.highlightColor, 0.24)
                                       : Theme.rgba(
                                           Theme.highlightBackgroundColor,
                                           0.14)
                                border.width: 1
                                border.color: page.controller
                                             && page.controller.selectedCategoryId
                                                === String(modelData.categoryId)
                                             ? Theme.highlightColor
                                             : Theme.rgba(
                                                   Theme.secondaryColor, 0.24)
                            }

                            Label {
                                id: categoryLabel
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: Theme.paddingMedium
                                anchors.rightMargin: Theme.paddingMedium
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.title || ""
                                color: page.controller
                                       && page.controller.selectedCategoryId
                                          === String(modelData.categoryId)
                                       ? Theme.highlightColor
                                       : Theme.primaryColor
                                font.pixelSize: Theme.fontSizeSmall
                                horizontalAlignment: Text.AlignHCenter
                                truncationMode: TruncationMode.Fade
                            }
                        }
                    }
                }
            }

            Components.SectionTitle {
                objectName: "discoverPublicPlaylistSection"
                width: parent.width
                visible: page.publicPlaylistContentReady
                         && publicPlaylists.count > 0
                text: qsTr("Public playlists")
                detail: publicPlaylists.count > 0
                        ? qsTr("%1 playlists").arg(publicPlaylists.count) : ""
            }

            Components.PlaylistGrid {
                id: publicPlaylistGrid
                objectName: "discoverPublicPlaylistGrid"
                controller: page.controller
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                columns: page.width > page.height ? 3 : 2
                model: page.publicPlaylistContentReady
                       ? page.publicPlaylists : null
                viewportContentY: flickable.contentY
                viewportHeight: flickable.height
                visible: count > 0
                height: count > 0 ? Math.ceil(count / columns) * cellHeight : 0
                onPlaylistClicked: page.openPlaylist(playlistId)
            }

        }

        Components.AsyncStateView {
            id: discoverStateView
            objectName: "discoverStateView"
            anchors.fill: parent
            state: page.controller ? page.controller.discoverState : 2
            hasContent: page.hasDiscoverContent
                        && discoverStateView.state !== 4
            emptyText: qsTr("No discover content is available")
            errorText: qsTr("Unable to load discover content")
            onRetryRequested: page.refreshContent()
        }
    }

    Components.CompactPlayer {
        id: compactPlayer
        objectName: "discoverCompactPlayer"
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
        console.info("qqmusic_discover component=qml event=page_open")
        page.activateDeferredContent()
        if (page.autoRefresh)
            page.refreshContent()
    }

    onStatusChanged: page.activateDeferredContent()

    function activateDeferredContent() {
        if (headlessTest) {
            contentStage = 2
            return
        }
        if ((embedded || status === PageStatus.Active) && contentStage === 0
                && !contentStageTimer.running)
            contentStageTimer.start()
    }

    function refreshContent() {
        if (page.controller
                && typeof page.controller.refreshDiscover === "function")
            page.controller.refreshDiscover()
    }

    function openRecommended() {
        recommendedRequested()
        if (!headlessTest && !embedded)
            pageStack.pop()
    }

    function categoryItemCount() {
        var total = 0
        for (var groupIndex = 0;
             groupIndex < page.categoryGroups.length; ++groupIndex) {
            var items = page.categoryGroups[groupIndex].items || []
            total += items.length
        }
        return total
    }

    function selectCategory(categoryId) {
        if (!page.controller || !categoryId)
            return
        page.controller.selectCategory(categoryId)
    }

    function openPlaylist(playlistId) {
        playlistRequested(playlistId)
        if (!headlessTest) {
            pageStack.push(Qt.resolvedUrl("PlaylistPage.qml"),
                           { "controller": page.controller,
                             "playlistId": playlistId })
        }
    }

}
