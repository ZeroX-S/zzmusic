import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components" as Components

Page {
    id: page

    property var controller
    property bool headlessTest: false
    property bool reloginRequested: false
    property int libraryContentStage: headlessTest ? 3 : 0
    readonly property bool likedContentReady: libraryContentStage >= 1
    readonly property bool createdContentReady: libraryContentStage >= 2
    readonly property bool collectedContentReady: libraryContentStage >= 3
    readonly property bool loggedOutStateVisible: !controller
                                                  || !controller.loggedIn
    readonly property bool loginFlowVisible: loggedOutStateVisible
                                              || reloginRequested
    readonly property real headerBottom: pageHeader.y + pageHeader.height
    readonly property real loggedOutContentTop: loggedOutColumn.y
    readonly property real loggedOutContentBottom: loggedOutColumn.y
                                                   + loggedOutColumn.height
    readonly property string loginStatus: controller
                                          ? controller.loginStatus : ""
    readonly property string loginDiagnosticCode: controller
                                                  ? controller.loginDiagnosticCode
                                                  : ""
    readonly property bool loginQrVisible: loginFlowVisible
                                            && (loginStatus === "qr"
                                                || loginStatus === "scanned")
                                            && controller
                                            && controller.loginQrImage !== ""
    readonly property bool loginBusyVisible: loginFlowVisible
                                              && (loginStatus === "opening"
                                                  || loginStatus === "scanned"
                                                  || loginStatus === "validating")
    readonly property bool loginStartVisible: loginFlowVisible
                                               && (loginStatus === ""
                                                   || loginStatus === "cancelled"
                                                   || loginStatus === "timeout"
                                                   || loginStatus === "error"
                                                   || loginStatus === "success")
    readonly property bool loginCancelVisible: loginFlowVisible
                                                && controller
                                                && controller.loginCancellable
                                                && loginStatus !== "validating"

    allowedOrientations: Orientation.All

    Timer {
        id: libraryContentTimer
        interval: 24
        repeat: false
        onTriggered: {
            if (page.libraryContentStage < 3) {
                ++page.libraryContentStage
                console.info("zzmusic_performance component=my_music event=content_stage stage="
                             + page.libraryContentStage)
            }
            if (page.libraryContentStage < 3)
                restart()
        }
    }

    SilicaFlickable {
        id: musicFlickable
        objectName: "myMusicContentFlickable"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: compactPlayer.visible ? compactPlayer.top : parent.bottom
        contentHeight: Math.max(
            height,
            page.loginFlowVisible
                ? page.loggedOutContentBottom + Theme.paddingLarge
                : libraryColumn.height + Theme.paddingLarge)
        VerticalScrollDecorator {}

        PullDownMenu {
            MenuItem {
                visible: page.controller && page.controller.loggedIn
                text: qsTr("Refresh")
                onClicked: page.controller.refreshMyMusic()
            }
        }

        Column {
            id: libraryColumn
            width: parent.width
            visible: !page.reloginRequested

            Item {
                id: pageHeader
                objectName: "myMusicCompactHeader"
                width: parent.width
                height: Theme.itemSizeMedium

                Label {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.horizontalPageMargin
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    text: page.loggedOutStateVisible
                          ? qsTr("Sign in") : qsTr("My Music")
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeLarge
                    font.weight: Font.DemiBold
                    truncationMode: TruncationMode.Fade
                }
            }

            Item {
                visible: page.controller && page.controller.loggedIn
                width: parent.width
                height: visible ? Theme.itemSizeLarge : 0

                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.itemSizeMedium
                    height: width
                    color: Theme.rgba(Theme.highlightBackgroundColor, 0.18)

                    Image {
                        anchors.centerIn: parent
                        width: Theme.iconSizeMedium
                        height: width
                        source: "image://theme/icon-m-contact"
                        opacity: Theme.opacityLow
                    }
                }

                Image {
                    id: avatar
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.itemSizeMedium
                    height: width
                    source: page.controller ? page.controller.avatar : ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize.width: Math.max(1, Math.round(width))
                    sourceSize.height: Math.max(1, Math.round(height))
                }
                Column {
                    anchors.left: avatar.right
                    anchors.right: parent.right
                    anchors.leftMargin: Theme.paddingLarge
                    anchors.rightMargin: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter

                    Label {
                        width: parent.width
                        text: page.controller ? page.controller.nickname : ""
                        color: Theme.primaryColor
                        font.pixelSize: Theme.fontSizeMedium
                        font.weight: Font.DemiBold
                        truncationMode: TruncationMode.Fade
                    }
                    Label {
                        width: parent.width
                        text: qsTr("Signed in")
                        color: Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeExtraSmall
                        truncationMode: TruncationMode.Fade
                    }
                }
            }

            BackgroundItem {
                objectName: "reloginQrAction"
                visible: page.controller && page.controller.loggedIn
                width: parent.width
                height: visible ? Theme.itemSizeMedium : 0
                enabled: !!page.controller
                onClicked: {
                    page.reloginRequested = true
                    console.info("qqmusic_login component=qml event=relogin_requested")
                    page.controller.startLogin()
                }

                Image {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.iconSizeSmall
                    height: width
                    source: "image://theme/icon-m-refresh"
                }
                Label {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.horizontalPageMargin
                                      + Theme.iconSizeSmall
                                      + Theme.paddingMedium
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Verify sign-in with a new QR code")
                    color: parent.highlighted ? Theme.highlightColor
                                              : Theme.primaryColor
                    font.pixelSize: Theme.fontSizeSmall
                    truncationMode: TruncationMode.Fade
                }
            }

            Components.SectionTitle {
                width: parent.width
                visible: likedGrid.count > 0
                text: qsTr("Liked")
            }
            Components.PlaylistGrid {
                id: likedGrid
                controller: page.controller
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                model: page.likedContentReady && page.controller
                       ? page.controller.likedPlaylists : null
                viewportContentY: musicFlickable.contentY
                viewportHeight: musicFlickable.height
                onPlaylistClicked: page.openPlaylist(playlistId)
            }

            Components.SectionTitle {
                width: parent.width
                visible: createdGrid.count > 0
                text: qsTr("Created")
            }
            Components.PlaylistGrid {
                id: createdGrid
                controller: page.controller
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                model: page.createdContentReady && page.controller
                       ? page.controller.createdPlaylists : null
                viewportContentY: musicFlickable.contentY
                viewportHeight: musicFlickable.height
                onPlaylistClicked: page.openPlaylist(playlistId)
            }

            Column {
                id: createdStateColumn
                objectName: "createdLibraryState"
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                spacing: Theme.paddingMedium
                visible: page.controller && page.controller.loggedIn
                         && page.controller.createdPlaylistsState !== 1
                         && !(page.controller.createdPlaylistsState === 3
                              && (likedGrid.count > 0 || createdGrid.count > 0))

                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: createdStateColumn.visible && page.controller
                             && page.controller.createdPlaylistsState === 0
                    visible: running
                    size: BusyIndicatorSize.Small
                }
                Label {
                    width: parent.width
                    text: page.createdStateText()
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeSmall
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: page.controller
                             && page.controller.createdPlaylistsState === 4
                    text: qsTr("Retry")
                    onClicked: page.controller.refreshMyMusic()
                }
            }

            Components.SectionTitle {
                width: parent.width
                visible: collectedGrid.count > 0
                text: qsTr("Collected")
            }
            Components.PlaylistGrid {
                id: collectedGrid
                controller: page.controller
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                model: page.collectedContentReady && page.controller
                       ? page.controller.collectedPlaylists : null
                viewportContentY: musicFlickable.contentY
                viewportHeight: musicFlickable.height
                onPlaylistClicked: page.openPlaylist(playlistId)
            }

            Column {
                id: collectedStateColumn
                objectName: "collectedLibraryState"
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                spacing: Theme.paddingMedium
                visible: page.controller && page.controller.loggedIn
                         && page.controller.collectedPlaylistsState !== 1
                         && !(page.controller.collectedPlaylistsState === 3
                              && collectedGrid.count > 0)

                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: collectedStateColumn.visible && page.controller
                             && page.controller.collectedPlaylistsState === 0
                    visible: running
                    size: BusyIndicatorSize.Small
                }
                Label {
                    width: parent.width
                    text: page.collectedStateText()
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeSmall
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: page.controller
                             && page.controller.collectedPlaylistsState === 4
                    text: qsTr("Retry")
                    onClicked: page.controller.refreshMyMusic()
                }
            }

        }

        Column {
            id: loggedOutColumn
            objectName: "loginPanel"
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.max(page.headerBottom + Theme.paddingLarge,
                        (musicFlickable.height - height) / 2)
            width: parent.width - 2 * Theme.horizontalPageMargin
            visible: page.loginFlowVisible
            spacing: Theme.paddingLarge

            Label {
                objectName: "loginStateLabel"
                width: parent.width
                text: page.loginStateText()
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeMedium
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }

            Label {
                objectName: "loginDiagnosticLabel"
                visible: page.loginStatus === "error"
                         && page.loginDiagnosticCode !== ""
                width: parent.width
                text: page.loginFailureText()
                color: Theme.errorColor
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }

            BusyIndicator {
                objectName: "loginBusyIndicator"
                anchors.horizontalCenter: parent.horizontalCenter
                running: page.loginBusyVisible
                visible: running
                size: BusyIndicatorSize.Small
            }

            Rectangle {
                id: qrFrame
                objectName: "loginQrFrame"
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(parent.width, Theme.itemSizeHuge * 2)
                height: visible ? width : 0
                visible: page.loginQrVisible
                color: "white"

                Image {
                    objectName: "loginQrImage"
                    anchors.fill: parent
                    anchors.margins: Theme.paddingSmall
                    property int generation: page.controller
                                             ? page.controller.loginQrGeneration : 0
                    source: page.controller
                            ? page.controller.loginQrImage : ""
                    cache: false
                    asynchronous: false
                    fillMode: Image.PreserveAspectFit
                    onStatusChanged: {
                        if (status === Image.Ready && source !== "") {
                            console.info("qqmusic_login component=qml event=qr_image_ready generation="
                                         + generation)
                        } else if (status === Image.Error) {
                            console.warn("qqmusic_login component=qml event=qr_image_error")
                        }
                    }
                }
            }

            Button {
                objectName: "loginStartButton"
                anchors.horizontalCenter: parent.horizontalCenter
                visible: page.loginStartVisible
                text: page.loginStatus === ""
                      || page.loginStatus === "success"
                      ? qsTr("Sign in with QR code") : qsTr("Try again")
                onClicked: {
                    console.info("qqmusic_login component=qml event=button_triggered")
                    page.controller.startLogin()
                }
            }

            Button {
                objectName: "loginCancelButton"
                anchors.horizontalCenter: parent.horizontalCenter
                visible: page.loginCancelVisible
                text: qsTr("Cancel")
                onClicked: {
                    page.controller.cancelLogin()
                    if (page.controller.loggedIn)
                        page.reloginRequested = false
                }
            }
        }

        Components.AsyncStateView {
            objectName: "myMusicAggregateState"
            anchors.fill: parent
            visible: !page.loggedOutStateVisible && offlineWithCache
            state: page.controller ? page.controller.myMusicState : 2
            hasContent: likedGrid.count > 0 || createdGrid.count > 0
                        || collectedGrid.count > 0
            emptyText: qsTr("No account playlists were returned")
            errorText: qsTr("Unable to load My Music")
            onRetryRequested: page.controller.refreshMyMusic()
        }
    }

    Components.CompactPlayer {
        id: compactPlayer
        objectName: "myMusicCompactPlayer"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        controller: page.controller
        playback: page.controller ? page.controller.playback : null
        onPlayerRequested: pageStack.push(
            Qt.resolvedUrl("PlayerPage.qml"),
            { "controller": page.controller })
    }

    function openPlaylist(playlistId) {
        pageStack.push(Qt.resolvedUrl("PlaylistPage.qml"),
                       { "controller": page.controller,
                         "playlistId": playlistId })
    }

    function createdStateText() {
        if (!controller)
            return ""
        if (controller.createdPlaylistsState === 0)
            return qsTr("Loading liked and created playlists")
        if (controller.createdPlaylistsState === 2)
            return qsTr("No liked or created playlists")
        return qsTr("Unable to load liked and created playlists")
    }

    function collectedStateText() {
        if (!controller)
            return ""
        if (controller.collectedPlaylistsState === 0)
            return qsTr("Loading collected playlists")
        if (controller.collectedPlaylistsState === 2)
            return qsTr("No collected playlists")
        return qsTr("Unable to load collected playlists")
    }

    function loginStateText() {
        if (loginStatus === "opening")
            return qsTr("Preparing QR code")
        if (loginStatus === "qr")
            return qsTr("Scan with QQ Music")
        if (loginStatus === "scanned")
            return qsTr("QR code scanned. Confirm on your phone.")
        if (loginStatus === "validating")
            return qsTr("Signing in")
        if (loginStatus === "cancelled")
            return qsTr("Sign-in cancelled")
        if (loginStatus === "timeout")
            return qsTr("QR code expired")
        if (loginStatus === "error")
            return qsTr("Unable to sign in")
        return qsTr("Sign in to access My Music")
    }

    function loginFailureText() {
        var code = loginDiagnosticCode
        var message = qsTr("Sign-in failed. Please try again.")
        if (code.indexOf("qq.login.mqtt.") === 0)
            message = qsTr("Unable to connect to the QR sign-in channel. Check the network and try again.")
        else if (code.indexOf("qq.login.network.") === 0)
            message = qsTr("Unable to reach the QR code service. Check the network and try again.")
        else if (code.indexOf("qq.session.validation.") === 0)
            message = qsTr("The scanned account could not be verified. Please generate a new QR code.")
        else if (code.indexOf("qq.session.secret.") === 0)
            message = qsTr("The account was verified but the session could not be saved.")
        return message + "\n" + code
    }

    Connections {
        target: page.controller
        onSessionChanged: {
            if (page.controller.loggedIn) {
                page.reloginRequested = false
                page.controller.refreshMyMusic()
            }
        }
        onLoginChanged: {
            if (page.reloginRequested
                    && page.controller.loggedIn
                    && page.controller.loginStatus === "success") {
                page.reloginRequested = false
                page.controller.refreshMyMusic()
            }
        }
    }

    Component.onCompleted: {
        console.info("qqmusic_login component=qml event=page_open logged_in="
                     + Boolean(controller && controller.loggedIn))
        page.activateDeferredContent()
        if (controller && controller.loggedIn
                && !page.hasLibraryContent()
                && controller.createdPlaylistsState !== 0
                && controller.collectedPlaylistsState !== 0)
            controller.refreshMyMusic()
    }

    Component.onDestruction: {
        if (reloginRequested && controller && controller.loginCancellable)
            controller.cancelLogin()
    }

    onStatusChanged: page.activateDeferredContent()

    function activateDeferredContent() {
        if (headlessTest) {
            libraryContentStage = 3
            return
        }
        if (status === PageStatus.Active && libraryContentStage === 0
                && !libraryContentTimer.running)
            libraryContentTimer.start()
    }

    function modelCount(model) {
        return model && model.count !== undefined ? model.count : 0
    }

    function hasLibraryContent() {
        return controller
                && (modelCount(controller.likedPlaylists) > 0
                    || modelCount(controller.createdPlaylists) > 0
                    || modelCount(controller.collectedPlaylists) > 0)
    }
}
