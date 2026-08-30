import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components" as Components

Page {
    id: page

    property var controller
    property bool cacheSelectorReady: false
    readonly property bool signOutVisible: !!(controller
                                                && controller.loggedIn)

    allowedOrientations: Orientation.All

    SilicaFlickable {
        objectName: "settingsContentFlickable"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: compactPlayer.visible ? compactPlayer.top : parent.bottom
        contentHeight: settingsColumn.height + Theme.paddingLarge
        VerticalScrollDecorator {}

        Column {
            id: settingsColumn
            width: parent.width

            Item {
                objectName: "settingsCompactHeader"
                width: parent.width
                height: Theme.itemSizeMedium

                Label {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.horizontalPageMargin
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Settings")
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeLarge
                    font.weight: Font.DemiBold
                    truncationMode: TruncationMode.Fade
                }
            }

            Components.SectionTitle {
                width: parent.width
                text: qsTr("Playback")
            }

            TextSwitch {
                objectName: "resumeQueueSwitch"
                width: parent.width
                text: qsTr("Resume last queue")
                description: qsTr("Continue from the saved queue after reopening the app")
                checked: page.controller && page.controller.settings
                         ? page.controller.settings.resumeEnabled : true
                onClicked: {
                    if (page.controller && page.controller.settings)
                        page.controller.settings.resumeEnabled = checked
                }
            }

            Components.SectionTitle {
                width: parent.width
                text: qsTr("Cache")
                detail: qsTr("Used %1").arg(page.formatBytes(
                            page.controller
                            ? page.controller.cacheUsageBytes : 0))
            }

            ComboBox {
                id: cacheLimit
                width: parent.width
                label: qsTr("Limit")
                currentIndex: page.limitIndex(page.controller
                                              ? page.controller.cacheLimitBytes
                                              : 134217728)
                menu: ContextMenu {
                    MenuItem { text: qsTr("64 MiB") }
                    MenuItem { text: qsTr("96 MiB") }
                    MenuItem { text: qsTr("128 MiB") }
                }
                onCurrentIndexChanged: {
                    if (page.cacheSelectorReady && page.controller)
                        page.controller.setCacheLimitBytes(
                            [67108864, 100663296, 134217728][currentIndex])
                }
            }

            BackgroundItem {
                objectName: "clearCacheAction"
                width: parent.width
                height: Theme.itemSizeMedium
                enabled: !!page.controller
                onClicked: cacheRemorse.execute(qsTr("Clearing cache"),
                                                function() {
                    page.controller.clearCache()
                })

                Image {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.iconSizeSmall
                    height: width
                    source: "image://theme/icon-m-delete"
                }
                Label {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.horizontalPageMargin
                                      + Theme.iconSizeSmall
                                      + Theme.paddingMedium
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Clear cache")
                    color: parent.highlighted ? Theme.highlightColor
                                              : Theme.primaryColor
                    font.pixelSize: Theme.fontSizeSmall
                    truncationMode: TruncationMode.Fade
                }
            }

            Components.SectionTitle {
                width: parent.width
                text: qsTr("Account")
                detail: page.controller && page.controller.loggedIn
                        ? qsTr("Signed in") : qsTr("Signed out")
            }

            BackgroundItem {
                objectName: "signOutAction"
                visible: page.signOutVisible
                width: parent.width
                height: visible ? Theme.itemSizeMedium : 0
                enabled: !!page.controller
                onClicked: logoutRemorse.execute(qsTr("Signing out"),
                                                  function() {
                    page.controller.logout()
                })

                Image {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.iconSizeSmall
                    height: width
                    source: "image://theme/icon-m-close"
                }
                Label {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.horizontalPageMargin
                                      + Theme.iconSizeSmall
                                      + Theme.paddingMedium
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Sign out")
                    color: parent.highlighted ? Theme.highlightColor
                                              : Theme.primaryColor
                    font.pixelSize: Theme.fontSizeSmall
                    truncationMode: TruncationMode.Fade
                }
            }

            Button {
                visible: page.controller
                         && page.controller.authenticationRemovalPending
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Remove expired session")
                onClicked: page.controller.confirmExpiredSessionRemoval()
            }

            Components.SectionTitle {
                width: parent.width
                text: qsTr("About")
                detail: "1.0.0"
            }

            DetailItem {
                label: qsTr("Application")
                value: qsTr("ZZ Music 1.0.0")
            }
            DetailItem {
                label: qsTr("Upstream")
                value: qsTr("Listen1 QQ adapter")
            }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                text: qsTr("Unofficial community client. Not affiliated with Tencent or QQ Music.")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.Wrap
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Licenses and notices")
                onClicked: pageStack.push(licensePage)
            }
        }
    }

    Components.CompactPlayer {
        id: compactPlayer
        objectName: "settingsCompactPlayer"
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
        id: licensePage
        Page {
            allowedOrientations: Orientation.All
            SilicaFlickable {
                anchors.fill: parent
                contentHeight: licenseColumn.height + Theme.paddingLarge
                VerticalScrollDecorator {}
                Column {
                    id: licenseColumn
                    width: parent.width
                    Item {
                        width: parent.width
                        height: Theme.itemSizeMedium
                        Label {
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.horizontalPageMargin
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.horizontalPageMargin
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Licenses and notices")
                            color: Theme.highlightColor
                            font.pixelSize: Theme.fontSizeLarge
                            font.weight: Font.DemiBold
                            truncationMode: TruncationMode.Fade
                        }
                    }
                    Components.SectionTitle {
                        width: parent.width
                        text: qsTr("ZZ Music")
                    }
                    Label {
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        x: Theme.horizontalPageMargin
                        text: qsTr("MIT License")
                        wrapMode: Text.Wrap
                    }
                    Components.SectionTitle {
                        width: parent.width
                        text: qsTr("Listen1")
                    }
                    Label {
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        x: Theme.horizontalPageMargin
                        text: qsTr("QQ provider behavior derived from Listen1 under the MIT License.")
                        wrapMode: Text.Wrap
                    }
                    Components.SectionTitle {
                        width: parent.width
                        text: qsTr("Qt")
                    }
                    Label {
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        x: Theme.horizontalPageMargin
                        text: qsTr("Qt components are distributed under their applicable LGPL and GPL terms.")
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }

    RemorsePopup { id: cacheRemorse }
    RemorsePopup { id: logoutRemorse }

    function limitIndex(bytes) {
        if (bytes <= 67108864)
            return 0
        if (bytes <= 100663296)
            return 1
        return 2
    }

    function formatBytes(bytes) {
        return (bytes / 1048576).toFixed(1) + " " + qsTr("MiB")
    }

    Component.onCompleted: cacheSelectorReady = true
}
