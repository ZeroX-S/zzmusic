import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components" as Components

Page {
    id: page

    property var controller
    property bool headlessTest: false
    readonly property bool searchActionVisible: true
    readonly property bool myMusicActionVisible: true
    readonly property bool settingsActionVisible: true
    readonly property bool discoverActionVisible: true
    readonly property bool discoverMenuActionVisible: false
    readonly property bool discoverTabVisible: true
    readonly property bool topChartsTabVisible: true
    readonly property bool swipeNavigationEnabled: true
    readonly property int primaryPageCount: 3
    property int primaryTabIndex: 0
    property bool discoverPageReady: false
    property bool topChartsPageReady: false
    property bool discoverRefreshRequested: false
    property string pendingNavigationMethod: "swipe"
    readonly property bool discoverTabLoaded: discoverPageReady
    readonly property bool discoverTabReady: discoverPageReady
    readonly property bool topChartsTabReady: topChartsPageReady
    readonly property int bottomNavigationCount: 0
    readonly property int quickActionCount: 0
    readonly property int discoverySectionCount: 1
    readonly property int categorySectionCount: 0
    readonly property int publicPlaylistSectionCount: 0
    readonly property int toplistSectionCount: 0
    readonly property bool landscapeGrid: width > height
    readonly property int portraitGridColumns: 2
    readonly property bool compactHeader: true
    signal myMusicRequested()
    signal discoverRequested()
    signal topChartsRequested()
    signal primaryPageChangedByUser(int index, string method)
    signal playlistRequested(string playlistId)

    allowedOrientations: Orientation.All

    Column {
        id: pageChrome
        objectName: "homePageChrome"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: childrenRect.height

        Item {
            id: homeHeader
            objectName: "homeCompactHeader"
            width: parent.width
            height: Theme.itemSizeLarge

            Column {
                anchors.left: parent.left
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.right: parent.right
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
                    text: page.controller && page.controller.loggedIn
                          ? page.controller.nickname : qsTr("Discover")
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeSmall
                    truncationMode: TruncationMode.Fade
                }
            }
        }

        Components.PrimaryTabs {
            id: primaryTabs
            objectName: "homePrimaryTabs"
            width: parent.width
            currentIndex: page.primaryTabIndex
            objectNamePrefix: "home"
            recommendedText: qsTr("Recommended")
            discoverText: qsTr("Discover")
            topChartsText: qsTr("Top charts")
            visible: page.discoverTabVisible && page.topChartsTabVisible
            onRecommendedClicked: page.openRecommended("tab")
            onDiscoverClicked: page.openDiscover("tab")
            onTopChartsClicked: page.openTopCharts("tab")
        }
    }

    SlideshowView {
        id: primaryPager
        objectName: "homePrimaryPager"
        anchors.top: pageChrome.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: compactPlayer.visible ? compactPlayer.top : parent.bottom
        itemWidth: width
        itemHeight: height
        model: page.primaryPageCount
        currentIndex: page.primaryTabIndex
        interactive: page.swipeNavigationEnabled
        clip: true

        onCurrentIndexChanged: {
            if (page.primaryTabIndex !== currentIndex)
                page.primaryTabIndex = currentIndex
            page.activatePrimaryPage(page.pendingNavigationMethod)
            page.pendingNavigationMethod = "swipe"
        }

        delegate: Item {
            width: primaryPager.itemWidth
            height: primaryPager.itemHeight

            Loader {
                objectName: "homePrimarySlideLoader_" + index
                anchors.fill: parent
                active: true
                asynchronous: false
                sourceComponent: index === 0 ? recommendedSlide
                                 : index === 1 ? discoverSlide
                                               : topChartsSlide
            }
        }
    }

    Component {
        id: recommendedSlide

        Item {
            SilicaFlickable {
                id: flickable
                objectName: "homeContentFlickable"
                anchors.fill: parent
                contentHeight: contentColumn.height + Theme.paddingLarge

                PullDownMenu {
                    id: homePullDownMenu
                    objectName: "homePullDownMenu"
                    highlightColor: active
                                    ? Theme.highlightBackgroundColor
                                    : "transparent"

                    MenuItem {
                        objectName: "homeSearchMenuItem"
                        text: qsTr("Search")
                        onClicked: pageStack.push(
                            Qt.resolvedUrl("SearchPage.qml"),
                            { "controller": page.controller })
                    }
                    MenuItem {
                        objectName: "homeMyMusicMenuItem"
                        text: qsTr("My Music")
                        onClicked: page.openMyMusic()
                    }
                    MenuItem {
                        objectName: "homeRefreshMenuItem"
                        text: qsTr("Refresh")
                        onClicked: page.controller.refreshHome()
                    }
                    MenuItem {
                        objectName: "homeSettingsMenuItem"
                        text: qsTr("Settings")
                        onClicked: pageStack.push(
                            Qt.resolvedUrl("SettingsPage.qml"),
                            { "controller": page.controller })
                    }
                }

                VerticalScrollDecorator {}

                Column {
                    id: contentColumn
                    width: parent.width

                    Components.SectionTitle {
                        width: parent.width
                        visible: recommendations.count > 0
                        text: page.controller && page.controller.loggedIn
                              ? qsTr("For You") : qsTr("Recommended")
                    }

                    Components.PlaylistGrid {
                        id: recommendations
                        controller: page.controller
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        columns: page.landscapeGrid
                                 ? 3 : page.portraitGridColumns
                        model: page.controller
                               ? page.controller.recommendations : null
                        viewportContentY: flickable.contentY
                        viewportHeight: flickable.height
                        onPlaylistClicked: page.openPlaylist(playlistId)
                    }
                }

                Components.AsyncStateView {
                    anchors.fill: parent
                    state: page.controller ? page.controller.homeState : 2
                    hasContent: recommendations.count > 0
                    emptyText: qsTr("No recommendations are available")
                    errorText: qsTr("QQ Music could not be reached")
                    onRetryRequested: page.controller.refreshHome()
                }
            }
        }
    }

    Component {
        id: discoverSlide

        DiscoverPage {
            objectName: "homeEmbeddedDiscoverPage"
            visible: true
            controller: page.controller
            embedded: true
            autoRefresh: false
            onRecommendedRequested: page.openRecommended("page")
            Component.onCompleted: page.discoverPageReady = true
        }
    }

    Component {
        id: topChartsSlide

        TopChartsPage {
            objectName: "homeEmbeddedTopChartsPage"
            visible: true
            controller: page.controller
            embedded: true
            autoRefresh: false
            onRecommendedRequested: page.openRecommended("page")
            onDiscoverRequested: page.openDiscover("page")
            Component.onCompleted: page.topChartsPageReady = true
        }
    }

    Components.CompactPlayer {
        id: compactPlayer
        objectName: "homeCompactPlayer"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        controller: page.controller
        playback: page.controller ? page.controller.playback : null
        visible: compactPlayer.hasTrack
        onPlayerRequested: pageStack.push(
            Qt.resolvedUrl("PlayerPage.qml"),
            { "controller": page.controller })
    }

    Component.onCompleted: {
        if (controller)
            controller.refreshHome()
    }

    function openMyMusic() {
        myMusicRequested()
        if (!headlessTest) {
            pageStack.push(Qt.resolvedUrl("MyMusicPage.qml"),
                           { "controller": page.controller })
        }
    }

    function openDiscover(method) {
        discoverRequested()
        navigateToPrimaryPage(1, method || "tab")
    }

    function openRecommended(method) {
        navigateToPrimaryPage(0, method || "tab")
    }

    function openTopCharts(method) {
        topChartsRequested()
        navigateToPrimaryPage(2, method || "tab")
    }

    function navigateToPrimaryPage(index, method) {
        if (index < 0 || index >= primaryPageCount)
            return
        pendingNavigationMethod = method || "swipe"
        if (primaryTabIndex === index) {
            activatePrimaryPage(pendingNavigationMethod)
            pendingNavigationMethod = "swipe"
            return
        }
        primaryTabIndex = index
    }

    function activatePrimaryPage(method) {
        if (primaryTabIndex > 0)
            ensureDiscoverData()
        primaryPageChangedByUser(primaryTabIndex, method || "swipe")
        console.info("zzmusic_navigation component=home event=primary_page_changed index="
                     + primaryTabIndex + " method=" + (method || "swipe"))
    }

    function ensureDiscoverData() {
        if (discoverRefreshRequested || !controller
                || typeof controller.refreshDiscover !== "function")
            return
        discoverRefreshRequested = true
        controller.refreshDiscover()
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
