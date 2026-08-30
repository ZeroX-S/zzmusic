import QtQuick 2.0
import QtTest 1.0
import Sailfish.Silica 1.0
import "../../qml/components" as Components
import "../../qml/cover" as Cover
import "../../qml/internal" as Internal
import "../../qml/pages" as Pages

TestCase {
    id: testCase
    name: "PlayerLayout"

    property alias __silica_applicationwindow_instance: silicaWindow

    SilicaWindowStub {
        id: silicaWindow
        anchors.fill: parent
    }

    ListModel {
        id: fakeLyrics
        property int currentIndex: 0
        ListElement {
            timeMs: 0
            text: "很长的中文歌词用于验证布局"
            translation: "A translated lyric line used for layout bounds"
            current: true
        }
        ListElement {
            timeMs: 5000
            text: "下一句歌词"
            translation: "Next line"
            current: false
        }
    }

    ListModel {
        id: emptyLyrics
        property int currentIndex: -1
    }

    QtObject {
        id: fakePlayback
        property int state: 4
        property int repeatMode: 0
        property var queue: [{
            "mid": "001SANITIZED01",
            "title": "一首用于验证纵横屏布局且不会遮挡控制按钮的超长中文歌曲标题",
            "artist": "Artist",
            "album": "Album",
            "artworkUrl": "",
            "durationMs": 180000,
            "playable": true
        }]
        property int currentIndex: 0
        property var currentTrackValue: queue[0]
        property int positionMs: 42000
        property int durationMs: 180000
        property int audioSampleRateHz: 44100
        property int audioBitrateKbps: 927
        property string audioFormat: "FLAC"
        property bool audioLossless: true
        property bool playing: false
        property bool canPlay: true
        property bool canPause: false
        property bool canGoNext: true
        property bool canGoPrevious: true
        property int previousCalls: 0
        property int nextCalls: 0
        function play() { playing = true }
        function pause() { playing = false }
        function stop() {}
        function previous() { ++previousCalls }
        function next() {
            ++nextCalls
            currentIndex = Math.min(queue.length - 1, currentIndex + 1)
        }
        function seek(value) { positionMs = value }
        function remove(index) {}
        function clear() { queue = [] }
    }

    QtObject {
        id: fakeSettings
        property bool resumeEnabled: true
    }

    QtObject {
        id: fakeController
        property var playback: fakePlayback
        property var lyrics: fakeLyrics
        property int lyricState: 1
        property var settings: fakeSettings
        property int cacheUsageBytes: 0
        property int cacheLimitBytes: 134217728
        property bool loggedIn: false
        property bool authenticationRemovalPending: false
        property int artworkRequests: 0
        property int foregroundRequests: 0
        signal authenticationExpired()
        signal accountActionFailed()
        signal terminalPlaybackFailure(int errorKind, string trackTitle)
        signal artworkAvailable(url remoteUrl, url localUrl)
        signal artworkCacheReset()
        signal artworkForegroundAvailable(url remoteUrl, bool lightText)
        function playQueueIndex(index) { fakePlayback.currentIndex = index }
        function setCacheLimitBytes(bytes) { cacheLimitBytes = bytes }
        function clearCache() { return true }
        function logout() {}
        function confirmExpiredSessionRemoval() {
            authenticationRemovalPending = false
        }
        function artworkSource(remoteUrl) {
            ++artworkRequests
            return ""
        }
        function requestArtworkForeground(remoteUrl) {
            ++foregroundRequests
            return true
        }
    }

    Component {
        id: playerComponent
        Pages.PlayerPage {
            controller: fakeController
            headlessTest: true
        }
    }

    Component {
        id: settingsComponent
        Pages.SettingsPage { controller: fakeController }
    }

    Component {
        id: notificationsComponent
        Internal.AppNotifications { controller: fakeController }
    }

    Component {
        id: lyricComponent
        Components.LyricView {
            lyrics: fakeLyrics
            playback: fakePlayback
            state: fakeController.lyricState
        }
    }

    Component {
        id: coverComponent
        Cover.AppCover { controller: fakeController }
    }

    Component {
        id: compactPlayerComponent
        Components.CompactPlayer {
            controller: fakeController
            playback: fakePlayback
        }
    }

    Component {
        id: cachedArtworkComponent
        Components.CachedArtwork { controller: fakeController }
    }

    ListModel {
        id: playlistModel
        ListElement {
            playlistId: "9451963602"
            title: "适合在手机上显示的歌单标题"
            author: "QQ Music"
            artwork: ""
            trackCount: 12
        }
        ListElement {
            playlistId: "8451963603"
            title: "第二个用于验证不同 ID 的歌单"
            author: "QQ Music"
            artwork: ""
            trackCount: 8
        }
    }

    Component {
        id: playlistGridComponent
        Components.PlaylistGrid {
            controller: fakeController
            model: playlistModel
            columns: 2
        }
    }

    function createPlayer(width, height) {
        var page = playerComponent.createObject(testCase,
                                                { "width": width,
                                                  "height": height })
        verify(page !== null)
        return page
    }

    function test_portrait_bounds_and_touch_targets() {
        var page = createPlayer(540, 960)
        verify(!page.landscapeLayoutActive)
        verify(!page.usesScaleTransform)
        verify(page.minimumTouchTarget >= Theme.itemSizeSmall)
        verify(page.layoutBoundsValid)
        verify(page.compactHeader)
        verify(page.portraitArtworkExtent > 0)
        verify(page.portraitArtworkExtent
               <= page.width - 2 * Theme.horizontalPageMargin)
        verify(page.portraitStackCompact)
        verify(page.portraitArtworkToControlsGap
               <= page.portraitMetadataExtent + Theme.itemSizeMedium
                  + 3 * Theme.paddingSmall)
        var media = findChild(page, "playerPortraitMediaFrame")
        var metadata = findChild(page, "playerPortraitMetadataFrame")
        var progress = findChild(page, "playerPortraitProgressFrame")
        var controls = findChild(page, "playerPortraitTransportControls")
        var audioDetails = findChild(page,
                                     "playerPortraitAudioDetailsFrame")
        var audioBitrateLabel = findChild(page, "playerAudioBitrateLabel")
        var audioBitrate = findChild(page, "playerAudioBitrateValue")
        verify(media !== null)
        verify(metadata !== null)
        verify(progress !== null)
        verify(controls !== null)
        verify(audioDetails !== null)
        verify(audioBitrateLabel !== null)
        verify(audioBitrate !== null)
        verify(findChild(page, "playerAudioFormatValue") === null)
        verify(findChild(page, "playerAudioSampleRateValue") === null)
        compare(Math.round(metadata.y),
                Math.round(media.y + media.height + Theme.paddingSmall))
        compare(Math.round(progress.y),
                Math.round(metadata.y + metadata.height + Theme.paddingSmall))
        compare(Math.round(controls.y),
                Math.round(progress.y + progress.height))
        compare(Math.round(audioDetails.y),
                Math.round(controls.y + controls.height
                           + Theme.paddingSmall))
        verify(audioDetails.y + audioDetails.height <= page.height)
        compare(page.playbackPositionText, "0:42")
        compare(page.playbackDurationText, "3:00")
        compare(page.playbackTimeText, "0:42 / 3:00")
        compare(page.audioBitrateText, "927 kbps · Lossless")
        compare(audioBitrateLabel.text, "Bitrate")
        compare(audioBitrate.text, "927 kbps · Lossless")
        page.destroy()
    }

    function test_playback_mode_cycles_through_shuffle() {
        fakePlayback.repeatMode = 0
        var page = createPlayer(540, 960)
        var repeatButton = findChild(page, "playerRepeatButton")
        verify(repeatButton !== null)
        repeatButton.clicked(null)
        compare(fakePlayback.repeatMode, 1)
        repeatButton.clicked(null)
        compare(fakePlayback.repeatMode, 2)
        repeatButton.clicked(null)
        compare(fakePlayback.repeatMode, 3)
        verify(repeatButton.icon.source.toString().indexOf("shuffle") >= 0)
        repeatButton.clicked(null)
        compare(fakePlayback.repeatMode, 0)
        page.destroy()
    }

    function test_cached_artwork_updates_only_matching_source() {
        fakeController.artworkRequests = 0
        var artwork = cachedArtworkComponent.createObject(testCase, {
            "remoteSource": "https://y.gtimg.cn/music/photo_new/match.jpg"
        })
        verify(artwork !== null)
        compare(fakeController.artworkRequests, 1)
        fakeController.artworkAvailable(
            "https://y.gtimg.cn/music/photo_new/other.jpg",
            "file:///tmp/other.jpg")
        compare(artwork.resolvedSource.toString(), "")
        fakeController.artworkAvailable(
            "https://y.gtimg.cn/music/photo_new/match.jpg",
            "file:///tmp/match.jpg")
        compare(artwork.resolvedSource.toString(), "file:///tmp/match.jpg")
        artwork.active = false
        compare(artwork.resolvedSource.toString(), "")
        artwork.active = true
        compare(fakeController.artworkRequests, 2)
        fakeController.artworkCacheReset()
        compare(fakeController.artworkRequests, 3)
        artwork.destroy()
    }

    function test_portrait_cover_and_lyrics_tabs_are_synchronized() {
        var page = createPlayer(540, 960)
        var coverTab = findChild(page, "playerCoverTab")
        var lyricsTab = findChild(page, "playerLyricsTab")
        verify(coverTab !== null)
        verify(lyricsTab !== null)
        compare(page.portraitSlideIndex, 0)

        lyricsTab.clicked(null)
        compare(page.portraitSlideIndex, 1)
        coverTab.clicked(null)
        compare(page.portraitSlideIndex, 0)
        page.destroy()
    }

    function test_landscape_uses_distinct_two_column_layout() {
        var page = createPlayer(960, 540)
        verify(page.landscapeLayoutActive)
        verify(!page.usesScaleTransform)
        verify(page.layoutBoundsValid)
        var details = findChild(page, "playerLandscapeAudioDetailsFrame")
        var controls = findChild(page, "playerLandscapeTransportControls")
        verify(details !== null)
        verify(controls !== null)
        compare(Math.round(controls.y + controls.height
                           + Theme.paddingSmall),
                Math.round(details.y))
        compare(Math.round(details.y + details.height),
                Math.round(details.parent.height))
        page.destroy()
    }

    function test_artwork_loading_is_async_and_has_feedback() {
        var page = createPlayer(540, 960)
        verify(page.artworkDecodeAsynchronous)
        var artwork = findChild(page, "playerArtworkImage")
        var busy = findChild(page, "playerArtworkBusyIndicator")
        verify(artwork !== null)
        verify(busy !== null)
        verify(artwork.asynchronous)
        compare(page.lastArtworkState, "empty")
        verify(!busy.running)
        page.destroy()
    }

    function test_settings_and_notifications_construct() {
        var settings = settingsComponent.createObject(testCase,
                                                      { "width": 540,
                                                        "height": 960 })
        verify(settings !== null)
        var header = findChild(settings, "settingsCompactHeader")
        var resumeSwitch = findChild(settings, "resumeQueueSwitch")
        var clearCache = findChild(settings, "clearCacheAction")
        var signOut = findChild(settings, "signOutAction")
        verify(header !== null)
        verify(resumeSwitch !== null)
        verify(clearCache !== null)
        verify(signOut !== null)
        compare(header.height, Theme.itemSizeMedium)
        compare(resumeSwitch.checked, true)
        verify(clearCache.enabled)
        verify(!settings.signOutVisible)
        fakeController.loggedIn = true
        tryCompare(settings, "signOutVisible", true)
        compare(settings.limitIndex(67108864), 0)
        compare(settings.limitIndex(100663296), 1)
        compare(settings.limitIndex(134217728), 2)
        verify(settings.formatBytes(1048576).indexOf("1.0") === 0)

        var notifications = notificationsComponent.createObject(testCase)
        verify(notifications !== null)
        notifications.destroy()
        settings.destroy()
        fakeController.loggedIn = false
    }

    function test_lyric_states_are_explicit_and_bounded() {
        var lyrics = lyricComponent.createObject(
                    testCase, { "width": 540, "height": 640, "state": 0 })
        verify(lyrics !== null)
        verify(lyrics.loadingIndicatorVisible)
        verify(lyrics.stateMessageVisible)
        verify(lyrics.stateContentFits)
        verify(lyrics.stateMessage.length > 0)

        lyrics.lyrics = emptyLyrics
        lyrics.state = 2
        verify(!lyrics.loadingIndicatorVisible)
        verify(lyrics.stateMessageVisible)
        var emptyMessage = lyrics.stateMessage
        verify(emptyMessage.length > 0)

        lyrics.state = 4
        verify(lyrics.stateMessageVisible)
        verify(lyrics.stateMessage.length > 0)
        verify(lyrics.stateMessage !== emptyMessage)

        lyrics.lyrics = fakeLyrics
        lyrics.state = 1
        verify(!lyrics.stateMessageVisible)
        compare(lyrics.focusedIndex, 0)
        verify(lyrics.autoFollowEnabled)
        verify(lyrics.tapToSeekEnabled)
        verify(lyrics.backgroundTransparent)
        var currentLine = findChild(lyrics, "lyricLine_0")
        var nextLine = findChild(lyrics, "lyricLine_1")
        verify(currentLine !== null)
        verify(nextLine !== null)
        verify(currentLine.color === undefined)
        verify(nextLine.color === undefined)
        verify(currentLine.currentLine)
        verify(currentLine.opacity > nextLine.opacity)
        lyrics.destroy()
    }

    function test_compact_player_has_stable_bottom_bar_layout_and_progress() {
        fakePlayback.playing = false
        var compact = compactPlayerComponent.createObject(
                    silicaWindow, { "width": 540 })
        verify(compact !== null)
        verify(compact.hasTrack)
        verify(compact.dockedPresentation)
        verify(compact.progressOnTop)
        verify(compact.layoutBoundsValid)
        verify(compact.height >= Theme.itemSizeLarge)
        compare(Math.round(compact.progressFraction * 100), 23)

        var artwork = findChild(compact, "compactPlayerArtwork")
        var progress = findChild(compact, "compactPlayerProgress")
        var mode = findChild(compact, "compactPlayerMode")
        var previous = findChild(compact, "compactPlayerPrevious")
        var toggle = findChild(compact, "compactPlayerToggle")
        var next = findChild(compact, "compactPlayerNext")
        verify(artwork !== null)
        verify(progress !== null)
        verify(mode !== null)
        verify(previous !== null)
        verify(toggle !== null)
        verify(next !== null)
        compare(compact.visibleTransportControlCount, 4)
        verify(artwork.asynchronous)
        verify(progress.width > 0)
        toggle.clicked(null)
        compare(fakePlayback.playing, true)
        var previousCalls = fakePlayback.previousCalls
        previous.clicked(null)
        compare(fakePlayback.previousCalls, previousCalls + 1)
        fakePlayback.repeatMode = 2
        mode.clicked(null)
        compare(fakePlayback.repeatMode, 3)
        verify(mode.icon.source.toString().indexOf("shuffle") >= 0)
        compact.destroy()
    }

    function test_playlist_grid_exposes_async_artwork_and_track_value() {
        var grid = playlistGridComponent.createObject(
                    testCase, { "width": 540 })
        verify(grid !== null)
        tryCompare(grid, "count", 2)
        var meta = findChild(grid, "playlistMeta_9451963602")
        var artwork = findChild(grid, "playlistArtwork_9451963602")
        verify(meta !== null)
        verify(artwork !== null)
        verify(meta.text.indexOf("12") >= 0)
        verify(artwork.asynchronous)
        verify(grid.stableCellGeometry)
        grid.destroy()
    }

    function test_playlist_grid_emits_explicit_model_playlist_id() {
        var grid = playlistGridComponent.createObject(
                    testCase, { "width": 540 })
        verify(grid !== null)
        tryCompare(grid, "count", 2)
        var clickedIds = []
        grid.playlistClicked.connect(function(playlistId) {
            clickedIds.push(playlistId)
        })
        var firstTile = findChild(grid, "playlistTile_9451963602")
        var secondTile = findChild(grid, "playlistTile_8451963603")
        verify(firstTile !== null)
        verify(secondTile !== null)
        verify(firstTile.enabled)
        verify(secondTile.enabled)
        firstTile.clicked(null)
        secondTile.clicked(null)
        compare(clickedIds.length, 2)
        compare(clickedIds[0], "9451963602")
        compare(clickedIds[1], "8451963603")
        grid.destroy()
    }

    function test_foreground_playback_failure_is_visible_and_sanitized() {
        var notifications = notificationsComponent.createObject(
                    testCase, { "width": 540, "height": 960,
                                "foreground": true })
        verify(notifications !== null)
        fakeController.terminalPlaybackFailure(
                    1, "https://private.invalid/token=never-show")
        tryCompare(notifications, "foregroundMessageVisible", true)
        verify(notifications.foregroundMessage.length > 0)
        verify(notifications.foregroundMessage.indexOf("private.invalid") < 0)
        verify(notifications.foregroundMessage.indexOf("token") < 0)
        verify(notifications.foregroundMessage.indexOf("https://") < 0)
        var banner = findChild(notifications, "foregroundPlaybackFailure")
        verify(banner !== null)
        verify(banner.y >= 0)
        verify(banner.y + banner.height <= notifications.height)
        notifications.destroy()
    }

    function test_foreground_account_expiry_and_actions_are_visible() {
        var notifications = notificationsComponent.createObject(
                    testCase, { "width": 540, "height": 960,
                                "foreground": true })
        verify(notifications !== null)
        fakeController.authenticationExpired()
        tryCompare(notifications, "foregroundMessageVisible", true)
        verify(notifications.foregroundMessage.indexOf("expired") >= 0)
        verify(notifications.foregroundMessage.indexOf("expired session") >= 0)

        fakeController.accountActionFailed()
        verify(notifications.foregroundMessage.indexOf("failed") >= 0)
        verify(notifications.foregroundMessage.indexOf("again") >= 0)
        notifications.destroy()
    }

    function test_cover_constructs_against_target_silica_api() {
        var originalTrack = fakePlayback.currentTrackValue
        fakePlayback.playing = false
        fakePlayback.canPlay = true
        var contrastTrack = {
            "mid": "001SANITIZED01",
            "title": "Contrast title",
            "artist": "Artist",
            "album": "Album",
            "artworkUrl": "https://y.gtimg.cn/music/photo_new/contrast.jpg",
            "durationMs": 180000,
            "playable": true
        }
        fakePlayback.currentTrackValue = contrastTrack
        var cover = coverComponent.createObject(testCase,
                                                { "width": 300,
                                                  "height": 400 })
        verify(cover !== null)
        compare(Math.round(cover.progressFraction * 100), 23)
        verify(cover.contentBoundsValid)
        verify(cover.metadataClearsActions)
        verify(cover.actionReserveHeight >= Theme.itemSizeLarge)
        compare(cover.titleLineLimit, 2)
        var metadata = findChild(cover, "coverMetadata")
        var title = findChild(cover, "coverTrackTitle")
        var artist = findChild(cover, "coverTrackArtist")
        var actionSpace = findChild(cover, "coverActionSpace")
        var primaryActions = findChild(cover, "primaryCoverActions")
        var skipActions = findChild(cover, "skipCoverActions")
        var playPauseAction = findChild(cover, "coverPlayPauseAction")
        var moreAction = findChild(cover, "coverMoreControlsAction")
        var previousAction = findChild(cover, "coverPreviousAction")
        var nextAction = findChild(cover, "coverNextAction")
        var progress = findChild(cover, "coverPlaybackProgress")
        var redundantState = findChild(cover, "coverPlaybackState")
        verify(metadata !== null)
        verify(title !== null)
        verify(artist !== null)
        verify(actionSpace !== null)
        verify(primaryActions !== null)
        verify(skipActions === null)
        verify(playPauseAction !== null)
        verify(moreAction === null)
        verify(previousAction === null)
        verify(nextAction !== null)
        verify(progress !== null)
        verify(redundantState === null)
        verify(actionSpace.color === undefined)
        verify(metadata.y + metadata.height <= actionSpace.y)
        compare(title.maximumLineCount, 2)
        verify(cover.textContrastAdaptive)
        verify(cover.coverActionTintAdaptive)
        compare(cover.visibleCoverActionCount, 2)
        compare(primaryActions.iconBackground, false)
        verify(primaryActions.enabled)
        var nextCalls = fakePlayback.nextCalls
        cover.nextTrack()
        compare(fakePlayback.nextCalls, nextCalls + 1)
        fakeController.artworkForegroundAvailable(contrastTrack.artworkUrl,
                                                    false)
        compare(cover.useLightText, false)
        verify(cover.artworkContrastResolved)
        verify(String(playPauseAction.iconSource).indexOf("?#ff151515") >= 0)
        verify(String(nextAction.iconSource).indexOf("?#ff151515") >= 0)
        compare(title.color, cover.coverPrimaryTextColor)
        compare(artist.color, cover.coverSecondaryTextColor)
        fakeController.artworkForegroundAvailable(contrastTrack.artworkUrl,
                                                    true)
        compare(cover.useLightText, true)
        verify(String(playPauseAction.iconSource).indexOf("?#fffafafa") >= 0)
        verify(String(nextAction.iconSource).indexOf("?#fffafafa") >= 0)
        verify(progress.width > 0)
        cover.destroy()
        fakePlayback.currentTrackValue = originalTrack
    }
}
