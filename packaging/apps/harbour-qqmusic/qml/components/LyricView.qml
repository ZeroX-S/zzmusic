import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: root

    property var lyrics
    property var playback
    property int state: 2
    property bool autoFollow: true
    readonly property int highlightedIndex: lyrics ? lyrics.currentIndex : -1
    readonly property int focusedIndex: lyricList.currentIndex
    readonly property bool autoFollowEnabled: autoFollow
    readonly property bool tapToSeekEnabled: true
    readonly property bool hasLyrics: lyrics && lyrics.count > 0
    readonly property bool loadingIndicatorVisible: state === 0
    readonly property bool stateMessageVisible: state !== 1 || !hasLyrics
    readonly property bool backgroundTransparent: true
    readonly property string stateMessage: state === 0
                                                   ? qsTr("Loading lyrics")
                                         : state === 4
                                                   ? qsTr("Unable to load lyrics")
                                                   : qsTr("No lyrics available")
    readonly property bool stateContentFits: stateColumn.y >= 0
                                             && stateColumn.y
                                                + stateColumn.height <= height

    clip: true

    SilicaListView {
        id: lyricList

        anchors.fill: parent
        model: root.lyrics
        visible: root.hasLyrics
        spacing: Theme.paddingMedium
        currentIndex: root.highlightedIndex
        preferredHighlightBegin: height * 0.42
        preferredHighlightEnd: height * 0.58
        highlightRangeMode: ListView.ApplyRange
        highlightMoveDuration: 220
        VerticalScrollDecorator {}

        header: Item { height: lyricList.height * 0.38 }
        footer: Item { height: lyricList.height * 0.38 }

        delegate: Item {
            objectName: "lyricLine_" + index
            property bool currentLine: model.current
            width: lyricList.width
            height: Math.max(Theme.itemSizeMedium,
                             lyricColumn.height
                             + 2 * (currentLine ? Theme.paddingLarge
                                                : Theme.paddingMedium))
            opacity: currentLine ? 1.0
                                 : Math.abs(index - root.highlightedIndex) <= 1
                                   ? Theme.opacityHigh : 0.48
            Behavior on opacity { NumberAnimation { duration: 160 } }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    root.autoFollow = true
                    followResume.stop()
                    if (root.playback) {
                        root.playback.seek(model.timeMs)
                        root.followCurrent()
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(2, Theme.paddingSmall / 3)
                height: parent.currentLine ? Theme.itemSizeSmall : 0
                visible: parent.currentLine
                color: Theme.highlightColor
            }

            Column {
                id: lyricColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.rightMargin: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.paddingSmall

                Label {
                    width: parent.width
                    text: model.text
                    color: model.current ? Theme.highlightColor
                                         : Theme.primaryColor
                    font.pixelSize: model.current ? Theme.fontSizeMedium
                                                  : Theme.fontSizeSmall
                    font.weight: model.current ? Font.DemiBold : Font.Normal
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                }
                Label {
                    visible: model.translation.length > 0
                    width: parent.width
                    text: model.translation
                    color: model.current ? Theme.highlightColor
                                         : Theme.secondaryColor
                    opacity: Theme.opacityHigh
                    font.pixelSize: Theme.fontSizeExtraSmall
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                }
            }
        }

        onMovementStarted: {
            root.autoFollow = false
            followResume.stop()
        }
        onMovementEnded: followResume.restart()
    }

    Timer {
        id: followResume
        interval: 4000
        repeat: false
        onTriggered: {
            root.autoFollow = true
            root.followCurrent()
        }
    }

    Column {
        id: stateColumn

        anchors.centerIn: parent
        width: Math.max(0, parent.width - 2 * Theme.horizontalPageMargin)
        spacing: Theme.paddingMedium
        visible: root.stateMessageVisible

        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            running: root.loadingIndicatorVisible
            visible: running
            size: BusyIndicatorSize.Medium
        }

        Label {
            width: parent.width
            text: root.stateMessage
            color: Theme.secondaryHighlightColor
            font.pixelSize: Theme.fontSizeMedium
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }
    }

    Connections {
        target: root.lyrics
        onCurrentIndexChanged: {
            root.followCurrent()
        }
    }

    function followCurrent() {
        if (root.autoFollow && root.highlightedIndex >= 0)
            lyricList.positionViewAtIndex(root.highlightedIndex,
                                          ListView.Center)
    }
}
