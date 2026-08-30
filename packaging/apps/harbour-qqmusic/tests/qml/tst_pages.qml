import QtQuick 2.0
import QtTest 1.0
import Sailfish.Silica 1.0
import "../../qml/components" as Components
import "../../qml/pages" as Pages

TestCase {
    id: testCase
    name: "Pages"

    property alias __silica_applicationwindow_instance: silicaWindow

    SilicaWindowStub {
        id: silicaWindow
        anchors.fill: parent
    }

    Item {
        id: activePageContainer
        parent: null
        width: 960
        height: 960
        visible: true
        property int status: PageStatus.Active
        property int navigation: PageNavigation.NoNavigation
        property int navigationPending: PageNavigation.NoNavigation
        property int direction: PageNavigation.NoDirection
        property Item attachedContainer: null
    }

    QtObject {
        id: fakePlayback
        property var queue: []
        property var currentTrackValue: ({})
        property int currentIndex: -1
        property int positionMs: 0
        property int durationMs: 0
        property bool playing: false
        property bool canPlay: false
        property bool canPause: false
        property bool canGoNext: false
        function play() {}
        function pause() {}
    }

    ListModel {
        id: homeRecommendationModel
        ListElement {
            playlistId: "9451963602"
            title: "First recommendation"
            author: "QQ Music"
            artwork: ""
            trackCount: 20
        }
        ListElement {
            playlistId: "8451963603"
            title: "Second recommendation"
            author: "QQ Music"
            artwork: ""
            trackCount: 30
        }
    }

    ListModel {
        id: discoverPublicPlaylistModel
        ListElement {
            playlistId: "public-sanitized"
            title: "Public playlist"
            author: "QQ Music"
            artwork: ""
            trackCount: 12
        }
    }

    ListModel {
        id: discoverToplistModel
        ListElement {
            playlistId: "chart-sanitized"
            title: "Top chart"
            author: "QQ Music"
            artwork: ""
            trackCount: 10
        }
    }

    QtObject {
        id: fakeController
        property var recommendations: homeRecommendationModel
        property var publicPlaylists: discoverPublicPlaylistModel
        property var toplists: discoverToplistModel
        property var searchTracks: []
        property var searchPlaylists: []
        property var playlistTracks: []
        property var likedPlaylists: []
        property var createdPlaylists: []
        property var collectedPlaylists: []
        property var categoryGroups: []
        property string selectedCategoryId: ""
        property int homeState: 2
        property int discoverState: 2
        property int searchState: 2
        property int playlistState: 2
        property int myMusicState: 2
        property int createdPlaylistsState: 2
        property int collectedPlaylistsState: 2
        property int searchMode: 0
        property string playlistTitle: "很长的中文歌单标题用于验证页面边界不会互相遮挡"
        property string playlistAuthor: "QQ Music"
        property url playlistArtwork: ""
        property int playlistTrackCount: 0
        property url playlistSourceUrl: ""
        property string playlistPeriod: ""
        property bool loggedIn: false
        property string nickname: ""
        property url avatar: ""
        property string loginStatus: ""
        property string loginDiagnosticCode: ""
        property string loginQrImage: ""
        property int loginQrGeneration: 0
        property bool loginCancellable: false
        property var playback: fakePlayback
        property int homeRefreshes: 0
        property int discoverRefreshes: 0
        property int playlistRequests: 0
        property int toplistRequests: 0
        property string requestedToplistId: ""
        property int loginStarts: 0
        property int loginCancels: 0

        signal sessionChanged()
        signal loginChanged()

        function refreshHome() { ++homeRefreshes }
        function refreshDiscover() { ++discoverRefreshes }
        function selectCategory(categoryId) {
            selectedCategoryId = categoryId
        }
        function search(query) {}
        function loadMoreSearch() {}
        function openPlaylist(playlistId) { ++playlistRequests }
        function openToplist(toplistId) {
            ++toplistRequests
            requestedToplistId = toplistId
        }
        function refreshMyMusic() {}
        function startLogin() { ++loginStarts }
        function cancelLogin() { ++loginCancels }
        function playQueueIndex(index) {}
        function playSearchTrack(index) {}
        function playNextSearchTrack(index) {}
        function appendSearchTrack(index) {}
        function playPlaylistAt(index) {}
        function playNextPlaylistTrack(index) {}
        function appendPlaylistTrack(index) {}
    }

    Component {
        id: homeComponent
        Pages.HomePage {
            controller: fakeController
            headlessTest: true
        }
    }
    Component {
        id: interactiveHomeComponent
        Pages.HomePage {
            controller: fakeController
        }
    }
    Component {
        id: searchComponent
        Pages.SearchPage {
            controller: fakeController
            headlessTest: true
        }
    }
    Component {
        id: discoverComponent
        Pages.DiscoverPage {
            controller: fakeController
            headlessTest: true
        }
    }
    Component {
        id: topChartsPageComponent
        Pages.TopChartsPage {
            controller: fakeController
            headlessTest: true
        }
    }
    Component {
        id: myMusicComponent
        Pages.MyMusicPage {
            controller: fakeController
            headlessTest: true
        }
    }
    Component {
        id: playlistComponent
        Pages.PlaylistPage {
            controller: fakeController
            playlistId: "9451963602"
            headlessTest: true
        }
    }
    Component {
        id: toplistComponent
        Pages.PlaylistPage {
            controller: fakeController
            playlistId: "26"
            isToplist: true
            headlessTest: true
        }
    }
    Components.AsyncStateView {
        id: asyncStateView
        width: 540
        height: 820
        state: 1
        errorText: "无法连接QQ音乐，恢复网络后请重试"
    }

    function createPage(component, properties) {
        var page = component.createObject(activePageContainer, properties || {})
        verify(page !== null)
        page.__stack_container = activePageContainer
        tryCompare(page, "visible", true)
        return page
    }

    function cleanup() {
        asyncStateView.state = 1
        asyncStateView.hasContent = false
        fakeController.loggedIn = false
        fakeController.loginStatus = ""
        fakeController.loginDiagnosticCode = ""
        fakeController.loginQrImage = ""
        fakeController.loginQrGeneration = 0
        fakeController.loginCancellable = false
        fakeController.homeState = 2
        fakeController.discoverState = 2
        fakeController.categoryGroups = []
        fakeController.selectedCategoryId = ""
        fakeController.publicPlaylists = discoverPublicPlaylistModel
        fakeController.toplists = discoverToplistModel
        fakeController.playlistTracks = []
        fakePlayback.queue = []
        fakePlayback.currentTrackValue = ({})
        fakePlayback.currentIndex = -1
        fakePlayback.positionMs = 0
        fakePlayback.durationMs = 0
        fakePlayback.playing = false
        fakePlayback.canPlay = false
        fakePlayback.canPause = false
        fakePlayback.canGoNext = false
    }

    function setFakePlaybackTrack() {
        var track = {
            "mid": "track-sanitized",
            "title": "正在播放的歌曲",
            "artist": "Artist",
            "album": "Album",
            "artworkUrl": "",
            "durationMs": 180000,
            "playable": true
        }
        fakePlayback.queue = [track]
        fakePlayback.currentTrackValue = track
        fakePlayback.currentIndex = 0
        fakePlayback.positionMs = 45000
        fakePlayback.durationMs = 180000
        fakePlayback.canPlay = true
        fakePlayback.canGoNext = false
    }

    function test_home_navigation_separates_discover_from_pull_down_menu() {
        var before = fakeController.homeRefreshes
        var discoverBefore = fakeController.discoverRefreshes
        var page = createPage(homeComponent)
        var myMusicRequests = 0
        var discoverRequests = 0
        var topChartsRequests = 0
        var playlistRequests = []
        page.myMusicRequested.connect(function() { ++myMusicRequests })
        page.discoverRequested.connect(function() { ++discoverRequests })
        page.topChartsRequested.connect(function() { ++topChartsRequests })
        page.playlistRequested.connect(function(playlistId) {
            playlistRequests.push(playlistId)
        })
        var myMusicAction = findChild(page, "homeMyMusicMenuItem")
        var discoverMenuAction = findChild(page, "homeDiscoverMenuItem")
        var primaryTabs = findChild(page, "homePrimaryTabs")
        var recommendedTab = findChild(page, "homeRecommendedTab")
        var discoverTab = findChild(page, "homeDiscoverTab")
        var topChartsTab = findChild(page, "homeTopChartsTab")
        var primaryPager = findChild(page, "homePrimaryPager")
        var searchAction = findChild(page, "homeSearchMenuItem")
        var settingsAction = findChild(page, "homeSettingsMenuItem")
        var refreshAction = findChild(page, "homeRefreshMenuItem")
        var compactHeader = findChild(page, "homeCompactHeader")
        verify(page.searchActionVisible)
        verify(page.myMusicActionVisible)
        verify(page.settingsActionVisible)
        verify(page.discoverActionVisible)
        verify(!page.discoverMenuActionVisible)
        verify(page.discoverTabVisible)
        verify(page.topChartsTabVisible)
        verify(page.swipeNavigationEnabled)
        compare(page.primaryPageCount, 3)
        compare(page.primaryTabIndex, 0)
        compare(page.bottomNavigationCount, 0)
        compare(page.quickActionCount, 0)
        compare(page.discoverySectionCount, 1)
        compare(page.categorySectionCount, 0)
        compare(page.publicPlaylistSectionCount, 0)
        compare(page.toplistSectionCount, 0)
        compare(page.portraitGridColumns, 2)
        verify(page.compactHeader)
        verify(myMusicAction !== null)
        verify(discoverMenuAction === null)
        verify(primaryTabs !== null)
        verify(recommendedTab !== null)
        verify(discoverTab !== null)
        verify(topChartsTab !== null)
        verify(primaryPager !== null)
        verify(primaryPager.interactive)
        verify(searchAction !== null)
        verify(settingsAction !== null)
        verify(refreshAction !== null)
        verify(compactHeader !== null)
        var firstPlaylist = findChild(page, "playlistTile_9451963602")
        verify(firstPlaylist !== null)
        firstPlaylist.clicked(null)
        page.openPlaylist("8451963603")
        compare(playlistRequests.length, 2)
        compare(playlistRequests[0], "9451963602")
        compare(playlistRequests[1], "8451963603")
        page.openMyMusic()
        compare(myMusicRequests, 1)
        discoverTab.clicked(null)
        compare(discoverRequests, 1)
        tryCompare(page, "primaryTabIndex", 1)
        topChartsTab.clicked(null)
        compare(topChartsRequests, 1)
        tryCompare(page, "primaryTabIndex", 2)
        recommendedTab.clicked(null)
        tryCompare(page, "primaryTabIndex", 0)
        compare(fakeController.discoverRefreshes, discoverBefore + 1)
        compare(fakeController.homeRefreshes, before + 1)
        page.destroy()
    }

    function test_home_primary_pages_swipe_and_keep_loaded_content() {
        fakeController.categoryGroups = [
            { "title": "Language",
              "items": [
                  { "categoryId": "1", "title": "Mandarin" }
              ] }
        ]
        fakeController.selectedCategoryId = "1"
        fakeController.discoverState = 1
        var refreshesBefore = fakeController.discoverRefreshes
        var page = createPage(interactiveHomeComponent,
                              { "width": 540, "height": 960 })
        var pager = findChild(page, "homePrimaryPager")
        verify(pager !== null)
        verify(pager.interactive)
        compare(pager.itemWidth, pager.width)
        compare(pager.itemHeight, pager.height)
        compare(findChild(page, "homePullDownMenu").highlightColor,
                Qt.rgba(0, 0, 0, 0))
        tryCompare(page, "discoverTabReady", true)
        tryCompare(page, "topChartsTabReady", true)

        pager.currentIndex = 1
        tryCompare(page, "primaryTabIndex", 1)

        var discover = findChild(page, "homeEmbeddedDiscoverPage")
        verify(discover !== null)
        verify(discover.visible)
        verify(!findChild(discover, "discoverCompactHeader").visible)
        verify(!findChild(discover, "discoverPrimaryTabs").visible)
        tryCompare(discover, "categoryContentReady", true)
        verify(findChild(discover, "categoryItem_1") !== null)
        verify(findChild(discover, "categoryStrip_0") !== null)
        compare(fakeController.discoverRefreshes, refreshesBefore + 1)

        pager.currentIndex = 2
        tryCompare(page, "primaryTabIndex", 2)
        var charts = findChild(page, "homeEmbeddedTopChartsPage")
        verify(charts !== null)
        verify(charts.visible)
        verify(findChild(charts, "topChartsGrid") !== null)

        pager.currentIndex = 0
        tryCompare(page, "primaryTabIndex", 0)
        pager.currentIndex = 1
        tryCompare(page, "primaryTabIndex", 1)
        compare(findChild(page, "homeEmbeddedDiscoverPage"), discover)
        compare(fakeController.discoverRefreshes, refreshesBefore + 1)

        page.destroy()
        fakeController.categoryGroups = []
        fakeController.selectedCategoryId = ""
        fakeController.discoverState = 2
    }

    function test_discover_page_exposes_categories_and_routes_content() {
        fakeController.categoryGroups = [
            { "title": "Language",
              "items": [
                  { "categoryId": "1", "title": "Mandarin" },
                  { "categoryId": "2", "title": "Cantonese" }
              ] },
            { "title": "Style",
              "items": [
                  { "categoryId": "3", "title": "Pop" }
              ] }
        ]
        fakeController.selectedCategoryId = "1"
        fakeController.discoverState = 1

        var before = fakeController.discoverRefreshes
        var page = createPage(discoverComponent)
        var recommendedRequests = 0
        var topChartsRequests = 0
        page.recommendedRequested.connect(function() { ++recommendedRequests })
        page.topChartsRequested.connect(function() { ++topChartsRequests })
        compare(fakeController.discoverRefreshes, before + 1)
        compare(page.primaryTabIndex, 1)
        verify(findChild(page, "discoverPrimaryTabs") !== null)
        compare(findChild(page, "discoverPullDownMenu").highlightColor,
                Qt.rgba(0, 0, 0, 0))
        verify(findChild(page, "discoverRecommendedTab") !== null)
        verify(findChild(page, "discoverDiscoverTab") !== null)
        verify(findChild(page, "discoverTopChartsTab") !== null)
        compare(page.categorySectionCount, 2)
        compare(page.publicPlaylistSectionCount, 1)
        compare(page.toplistSectionCount, 0)
        verify(page.hasDiscoverContent)
        verify(findChild(page, "categoryItem_1") !== null)
        var languageStrip = findChild(page, "categoryStrip_0")
        var styleStrip = findChild(page, "categoryStrip_1")
        verify(languageStrip !== null)
        verify(styleStrip !== null)
        compare(languageStrip.count, 2)
        compare(styleStrip.count, 1)
        compare(languageStrip.orientation, ListView.Horizontal)
        compare(styleStrip.orientation, ListView.Horizontal)
        verify(findChild(page, "discoverPublicPlaylistGrid") !== null)
        verify(findChild(page, "discoverToplistGrid") === null)

        var playlistRequests = []
        page.playlistRequested.connect(function(playlistId) {
            playlistRequests.push(playlistId)
        })
        page.selectCategory("2")
        compare(fakeController.selectedCategoryId, "2")
        page.openPlaylist("public-sanitized")
        compare(playlistRequests.length, 1)
        compare(playlistRequests[0], "public-sanitized")
        findChild(page, "discoverRecommendedTab").clicked(null)
        compare(recommendedRequests, 1)
        findChild(page, "discoverTopChartsTab").clicked(null)
        compare(topChartsRequests, 1)

        page.destroy()
        fakeController.categoryGroups = []
        fakeController.selectedCategoryId = ""
        fakeController.discoverState = 2
    }

    function test_top_charts_page_is_independent_and_routes_chart() {
        fakeController.discoverState = 1
        var before = fakeController.discoverRefreshes
        var page = createPage(topChartsPageComponent)
        var toplistRequests = []
        page.toplistRequested.connect(function(toplistId) {
            toplistRequests.push(toplistId)
        })

        compare(page.primaryTabIndex, 2)
        compare(page.toplistSectionCount, 1)
        verify(page.hasToplistContent)
        verify(findChild(page, "topChartsPrimaryTabs") !== null)
        compare(findChild(page, "topChartsPullDownMenu").highlightColor,
                Qt.rgba(0, 0, 0, 0))
        verify(findChild(page, "topChartsRecommendedTab") !== null)
        verify(findChild(page, "topChartsDiscoverTab") !== null)
        verify(findChild(page, "topChartsTopChartsTab") !== null)
        verify(findChild(page, "topChartsGrid") !== null)
        verify(findChild(page, "playlistTile_chart-sanitized") !== null)
        compare(fakeController.discoverRefreshes, before + 1)

        page.openToplist("chart-sanitized")
        compare(toplistRequests.length, 1)
        compare(toplistRequests[0], "chart-sanitized")
        page.destroy()
        fakeController.discoverState = 2
    }

    function test_discover_page_async_states_and_retry() {
        fakeController.categoryGroups = []
        fakeController.publicPlaylists = []
        fakeController.toplists = []
        fakeController.discoverState = 2
        var homeBefore = fakeController.homeRefreshes
        var before = fakeController.discoverRefreshes
        var page = createPage(discoverComponent)
        var stateView = findChild(page, "discoverStateView")
        verify(stateView !== null)
        compare(fakeController.discoverRefreshes, before + 1)
        compare(fakeController.homeRefreshes, homeBefore)
        verify(!page.hasDiscoverContent)
        compare(stateView.state, 2)
        verify(stateView.blocking)

        fakeController.homeState = 1
        fakeController.discoverState = 4
        tryCompare(stateView, "state", 4)
        verify(stateView.blocking)
        stateView.retryRequested()
        compare(fakeController.discoverRefreshes, before + 2)
        compare(fakeController.homeRefreshes, homeBefore)

        fakeController.discoverState = 0
        tryCompare(stateView, "state", 0)
        verify(stateView.blocking)
        page.destroy()
        fakeController.publicPlaylists = discoverPublicPlaylistModel
        fakeController.toplists = discoverToplistModel
        fakeController.homeState = 2
        fakeController.discoverState = 2
    }

    function test_home_does_not_render_separate_category_or_source_sections() {
        fakeController.categoryGroups = [
            { "title": "Language",
              "items": [
                  { "categoryId": "1", "title": "Mandarin" },
                  { "categoryId": "2", "title": "Cantonese" }
              ] }
        ]
        fakeController.selectedCategoryId = "1"
        fakeController.publicPlaylists = [{ "id": "public-sanitized" }]
        fakeController.toplists = [{ "id": "chart-sanitized" }]

        var page = createPage(homeComponent)
        var firstItem = findChild(page, "categoryItem_1")
        var secondItem = findChild(page, "categoryItem_2")
        verify(firstItem === null)
        verify(secondItem === null)
        compare(page.categorySectionCount, 0)
        compare(page.publicPlaylistSectionCount, 0)
        compare(page.toplistSectionCount, 0)

        page.destroy()
        fakeController.categoryGroups = []
        fakeController.selectedCategoryId = ""
        fakeController.publicPlaylists = []
        fakeController.toplists = []
    }

    function test_now_playing_bar_is_docked_without_covering_page_content() {
        setFakePlaybackTrack()

        var home = createPage(homeComponent, { "width": 540, "height": 960 })
        var homeBar = findChild(home, "homeCompactPlayer")
        var homeContent = findChild(home, "homeContentFlickable")
        verify(homeBar !== null)
        verify(homeContent !== null)
        verify(homeBar.visible)
        compare(homeBar.y + homeBar.height, home.height)
        verify(homeContent.y + homeContent.height <= homeBar.y)
        home.destroy()

        var search = createPage(searchComponent, { "width": 540, "height": 960 })
        var searchBar = findChild(search, "searchCompactPlayer")
        var searchResults = findChild(search, "searchTrackResults")
        verify(searchBar !== null)
        verify(searchResults !== null)
        verify(searchBar.visible)
        compare(searchBar.y + searchBar.height, search.height)
        verify(searchResults.y + searchResults.height <= searchBar.y)
        search.destroy()

        var playlist = createPage(playlistComponent,
                                  { "width": 540, "height": 960 })
        var playlistBar = findChild(playlist, "playlistCompactPlayer")
        var playlistTracks = findChild(playlist, "playlistTrackList")
        verify(playlistBar !== null)
        verify(playlistTracks !== null)
        verify(playlistBar.visible)
        compare(playlistBar.y + playlistBar.height, playlist.height)
        verify(playlistTracks.y + playlistTracks.height <= playlistBar.y)
        playlist.destroy()
    }

    function test_search_preserves_query_and_debounces() {
        fakeController.searchMode = 0
        var page = createPage(searchComponent)
        compare(page.query, "")
        compare(page.debounceInterval, 350)
        compare(page.selectedKind, 0)
        verify(findChild(page, "searchCompactHeader") !== null)
        page.destroy()
    }

    function test_search_initial_kind_matches_controller_mode() {
        fakeController.searchMode = 1
        var page = createPage(searchComponent)
        compare(page.selectedKind, 1)
        compare(fakeController.searchMode, 1)
        page.destroy()
        fakeController.searchMode = 0
    }

    function test_my_music_has_logged_out_state() {
        fakeController.loggedIn = false
        var page = createPage(myMusicComponent, { "width": 540, "height": 960 })
        verify(page.loggedOutStateVisible)
        verify(page.loggedOutContentTop > page.headerBottom)
        verify(page.loggedOutContentBottom <= page.height)
        fakeController.loggedIn = true
        fakeController.sessionChanged()
        verify(!page.loggedOutStateVisible)
        fakeController.loggedIn = false
        page.destroy()
    }

    function test_my_music_native_qr_login_states_and_actions() {
        fakeController.loggedIn = false
        fakeController.loginStatus = ""
        fakeController.loginQrImage = ""
        fakeController.loginCancellable = false
        var startsBefore = fakeController.loginStarts
        var cancelsBefore = fakeController.loginCancels
        var page = createPage(myMusicComponent, { "width": 540, "height": 960 })
        var stateLabel = findChild(page, "loginStateLabel")
        var diagnosticLabel = findChild(page, "loginDiagnosticLabel")
        var qrFrame = findChild(page, "loginQrFrame")
        var qrImage = findChild(page, "loginQrImage")
        var busy = findChild(page, "loginBusyIndicator")
        var startButton = findChild(page, "loginStartButton")
        var cancelButton = findChild(page, "loginCancelButton")
        verify(stateLabel !== null)
        verify(diagnosticLabel !== null)
        verify(qrFrame !== null)
        verify(qrImage !== null)
        verify(busy !== null)
        verify(startButton !== null)
        verify(cancelButton !== null)
        verify(startButton.visible)
        verify(!cancelButton.visible)
        verify(!qrFrame.visible)
        verify(!busy.visible)
        startButton.clicked(null)
        compare(fakeController.loginStarts, startsBefore + 1)

        fakeController.loginStatus = "opening"
        fakeController.loginCancellable = true
        tryCompare(busy, "visible", true)
        tryCompare(cancelButton, "visible", true)
        verify(!startButton.visible)

        var qrData = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="
        fakeController.loginQrImage = qrData
        fakeController.loginQrGeneration = 1
        fakeController.loginStatus = "qr"
        tryCompare(qrFrame, "visible", true)
        compare(qrImage.generation, 1)
        compare(qrImage.source.toString(), qrData)
        tryCompare(qrImage, "status", Image.Ready)
        verify(!busy.visible)
        verify(page.loggedOutContentTop > page.headerBottom)
        verify(page.loggedOutContentBottom <= page.height)

        fakeController.loginStatus = "scanned"
        tryCompare(busy, "visible", true)
        verify(qrFrame.visible)
        verify(cancelButton.visible)

        fakeController.loginStatus = "validating"
        fakeController.loginCancellable = false
        tryCompare(qrFrame, "visible", false)
        verify(busy.visible)
        verify(!cancelButton.visible)

        fakeController.loginStatus = "timeout"
        tryCompare(startButton, "visible", true)
        verify(!busy.visible)
        fakeController.loginStatus = "cancelled"
        verify(startButton.visible)
        fakeController.loginStatus = "error"
        fakeController.loginDiagnosticCode = "qq.login.mqtt.connect.send"
        verify(startButton.visible)
        tryCompare(diagnosticLabel, "visible", true)
        verify(diagnosticLabel.text.indexOf("qq.login.mqtt.connect.send") >= 0)

        fakeController.loginStatus = "qr"
        fakeController.loginCancellable = true
        tryCompare(cancelButton, "visible", true)
        cancelButton.clicked(null)
        compare(fakeController.loginCancels, cancelsBefore + 1)

        fakeController.loggedIn = true
        fakeController.sessionChanged()
        tryCompare(page, "loggedOutStateVisible", false)
        fakeController.loggedIn = false
        fakeController.loginStatus = "success"
        tryCompare(page, "loginStartVisible", true)
        compare(stateLabel.text, "Sign in to access My Music")
        page.destroy()
    }

    function test_my_music_relogin_preserves_active_session_until_success() {
        fakeController.loggedIn = true
        fakeController.loginStatus = ""
        fakeController.loginQrImage = ""
        fakeController.loginCancellable = false
        var startsBefore = fakeController.loginStarts
        var cancelsBefore = fakeController.loginCancels
        var page = createPage(myMusicComponent, { "width": 540, "height": 960 })
        var reloginAction = findChild(page, "reloginQrAction")
        var loginPanel = findChild(page, "loginPanel")
        var busy = findChild(page, "loginBusyIndicator")
        var cancelButton = findChild(page, "loginCancelButton")
        verify(reloginAction !== null)
        verify(loginPanel !== null)
        verify(reloginAction.visible)
        verify(!page.loginFlowVisible)

        reloginAction.clicked(null)
        compare(fakeController.loginStarts, startsBefore + 1)
        verify(page.reloginRequested)
        verify(page.loginFlowVisible)
        verify(loginPanel.visible)
        verify(fakeController.loggedIn)

        fakeController.loginStatus = "opening"
        fakeController.loginCancellable = true
        fakeController.loginChanged()
        tryCompare(busy, "visible", true)
        tryCompare(cancelButton, "visible", true)
        cancelButton.clicked(null)
        compare(fakeController.loginCancels, cancelsBefore + 1)
        verify(!page.reloginRequested)
        verify(!page.loginFlowVisible)
        verify(fakeController.loggedIn)

        fakeController.loginStatus = ""
        fakeController.loginCancellable = false
        reloginAction.clicked(null)
        verify(page.reloginRequested)
        fakeController.loginStatus = "success"
        fakeController.loginChanged()
        tryCompare(page, "reloginRequested", false)
        verify(fakeController.loggedIn)

        page.destroy()
        fakeController.loggedIn = false
        fakeController.loginStatus = ""
        fakeController.loginCancellable = false
    }

    function test_my_music_library_states_are_independent() {
        fakeController.loggedIn = true
        fakeController.createdPlaylistsState = 1
        fakeController.collectedPlaylistsState = 4
        var page = createPage(myMusicComponent, { "width": 540, "height": 960 })
        var createdState = findChild(page, "createdLibraryState")
        var collectedState = findChild(page, "collectedLibraryState")
        var aggregateState = findChild(page, "myMusicAggregateState")
        verify(createdState !== null)
        verify(collectedState !== null)
        verify(aggregateState !== null)
        compare(createdState.visible, false)
        compare(collectedState.visible, true)
        compare(aggregateState.visible, false)

        fakeController.createdPlaylistsState = 4
        fakeController.collectedPlaylistsState = 1
        tryCompare(createdState, "visible", true)
        tryCompare(collectedState, "visible", false)

        fakeController.createdPlaylistsState = 0
        fakeController.collectedPlaylistsState = 2
        tryCompare(createdState, "visible", true)
        tryCompare(collectedState, "visible", true)
        fakeController.myMusicState = 0
        compare(aggregateState.visible, false)

        fakeController.createdPlaylistsState = 2
        fakeController.collectedPlaylistsState = 4
        fakeController.myMusicState = 4
        compare(aggregateState.visible, false)

        fakeController.myMusicState = 3
        tryCompare(aggregateState, "visible", true)
        verify(aggregateState.offlinePromptVisible)
        verify(!aggregateState.blocking)

        page.destroy()
        fakeController.loggedIn = false
        fakeController.createdPlaylistsState = 2
        fakeController.collectedPlaylistsState = 2
        fakeController.myMusicState = 2
    }

    function test_playlist_requests_content_and_bounds_title() {
        var before = fakeController.playlistRequests
        var page = createPage(playlistComponent)
        compare(fakeController.playlistRequests, before + 1)
        compare(page.playlistId, "9451963602")
        verify(page.compactHeader)
        verify(findChild(page, "playlistCompactHeader") !== null)
        page.destroy()
    }

    function test_toplist_uses_shared_detail_page_and_typed_request() {
        var before = fakeController.toplistRequests
        var page = createPage(toplistComponent)
        compare(fakeController.toplistRequests, before + 1)
        compare(fakeController.requestedToplistId, "26")
        verify(page.isToplist)
        page.destroy()
    }

    function test_toplist_period_visibility_follows_controller() {
        fakeController.playlistPeriod = "2026-07-27"
        var page = createPage(toplistComponent)
        var periodLabel = findChild(page, "playlistPeriodLabel")
        verify(periodLabel !== null)
        verify(page.toplistPeriodVisible)
        compare(page.toplistPeriodText, "2026-07-27")
        compare(periodLabel.text, "2026-07-27")

        fakeController.playlistPeriod = "2026-07-28"
        tryCompare(page, "toplistPeriodText", "2026-07-28")
        tryCompare(periodLabel, "text", "2026-07-28")
        fakeController.playlistPeriod = ""
        tryCompare(page, "toplistPeriodVisible", false)
        compare(periodLabel.text, "")
        page.destroy()

        fakeController.playlistPeriod = "2026-07-29"
        page = createPage(playlistComponent)
        periodLabel = findChild(page, "playlistPeriodLabel")
        verify(periodLabel !== null)
        verify(!page.toplistPeriodVisible)
        compare(page.toplistPeriodText, "")
        compare(periodLabel.text, "")
        page.destroy()
        fakeController.playlistPeriod = ""
    }

    function test_error_state_fits_portrait_bounds() {
        asyncStateView.hasContent = false
        asyncStateView.state = 4
        tryCompare(asyncStateView, "contentFits", true)
        verify(asyncStateView.stateContentHeight < asyncStateView.height)
    }

    function test_offline_cache_prompt_is_visible_and_non_blocking() {
        asyncStateView.hasContent = true
        asyncStateView.state = 3
        tryCompare(asyncStateView, "offlinePromptVisible", true)
        verify(!asyncStateView.blocking)
        verify(asyncStateView.offlinePromptFits)
        verify(asyncStateView.offlineRetryVisible)
        verify(asyncStateView.offlineText.indexOf("Offline") >= 0)
    }
}
