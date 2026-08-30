import QtQuick 2.0
import Nemo.Notifications 1.0
import Sailfish.Silica 1.0

Item {
    id: root

    property var controller
    property bool foreground: Qt.application.state === Qt.ApplicationActive
    property int foregroundMessageTimeout: 8000
    property string foregroundMessage: ""
    property bool foregroundMessageVisible: false

    z: 1000

    Notification {
        id: notification
        appName: qsTr("ZZ Music")
        appIcon: "harbour-qqmusic"
        category: "x-jolla.music"
        urgency: Notification.Normal
        expireTimeout: 8000
        isTransient: true
    }

    Rectangle {
        id: foregroundBanner

        objectName: "foregroundPlaybackFailure"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Theme.paddingMedium
        height: Math.max(Theme.itemSizeMedium,
                         foregroundLabel.implicitHeight
                         + 2 * Theme.paddingMedium)
        visible: root.foregroundMessageVisible
        color: Theme.rgba(Theme.highlightBackgroundColor, 0.96)

        Label {
            id: foregroundLabel

            anchors.fill: parent
            anchors.margins: Theme.paddingMedium
            text: root.foregroundMessage
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeSmall
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            maximumLineCount: 2
            truncationMode: TruncationMode.Fade
        }
    }

    Timer {
        id: foregroundTimer

        interval: root.foregroundMessageTimeout
        onTriggered: root.foregroundMessageVisible = false
    }

    Connections {
        target: root.controller
        onAuthenticationExpired: {
            root.publishMessage(qsTr("QQ Music sign-in expired"),
                                qsTr("Open the app to remove the expired session."))
        }
        onAccountActionFailed: {
            root.publishMessage(qsTr("Account action failed"),
                                qsTr("Try again."))
        }
        onTerminalPlaybackFailure: {
            if (root.foreground) {
                root.showForegroundMessage(
                            qsTr("Playback stopped") + "\n"
                            + qsTr("Unable to play the current track"))
                return
            }
            notification.summary = qsTr("Playback stopped")
            notification.body = trackTitle.length > 0
                                ? qsTr("Unable to play %1").arg(trackTitle)
                                : qsTr("Unable to play the current track")
            notification.publish()
        }
    }

    function showForegroundMessage(message) {
        root.foregroundMessage = message
        root.foregroundMessageVisible = true
        foregroundTimer.restart()
    }

    function publishMessage(summary, body) {
        if (root.foreground) {
            root.showForegroundMessage(summary + "\n" + body)
            return
        }
        notification.summary = summary
        notification.body = body
        notification.publish()
    }
}
