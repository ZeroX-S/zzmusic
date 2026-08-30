import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: root

    property int state: 2
    property bool hasContent: false
    property string emptyText: qsTr("Nothing here yet")
    property string errorText: qsTr("Unable to load content")
    property string loadingText: qsTr("Loading")
    property string offlineText: qsTr("Offline - showing cached content")
    property bool retryVisible: true
    readonly property bool blocking: state === 0
                                      || state === 2
                                      || (state === 4 && !hasContent)
    readonly property bool offlineWithCache: state === 3
    readonly property bool offlinePromptVisible: offlineWithCache
    readonly property bool offlineRetryVisible: offlineWithCache && retryVisible
    readonly property real offlinePromptHeight: offlineBanner.height
    readonly property bool offlinePromptFits: offlineBanner.y >= 0
                                               && offlineBanner.y
                                                  + offlineBanner.height
                                                  <= height
    readonly property real stateContentHeight: stateColumn.height
    readonly property bool contentFits: stateColumn.y >= 0
                                        && stateColumn.y + stateColumn.height
                                           <= height

    signal retryRequested()

    visible: blocking || offlineWithCache

    Column {
        id: stateColumn

        anchors.centerIn: parent
        width: Math.max(0, parent.width - 2 * Theme.horizontalPageMargin)
        spacing: Theme.paddingLarge
        visible: root.blocking

        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            running: root.state === 0
            visible: running
            size: BusyIndicatorSize.Medium
        }

        Label {
            width: parent.width
            text: root.state === 0 ? root.loadingText
                                  : root.state === 2 ? root.emptyText
                                                     : root.errorText
            color: Theme.secondaryHighlightColor
            font.pixelSize: Theme.fontSizeMedium
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.state === 4 && root.retryVisible
            text: qsTr("Retry")
            onClicked: root.retryRequested()
        }
    }

    Rectangle {
        id: offlineBanner

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.max(offlineLabel.implicitHeight,
                         offlineRetry.height) + 2 * Theme.paddingSmall
        visible: root.offlineWithCache
        color: Theme.rgba(Theme.highlightBackgroundColor, 0.92)
        z: 1

        Label {
            id: offlineLabel

            anchors.left: parent.left
            anchors.right: offlineRetry.left
            anchors.leftMargin: Theme.horizontalPageMargin
            anchors.rightMargin: Theme.paddingMedium
            anchors.verticalCenter: parent.verticalCenter
            text: root.offlineText
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            maximumLineCount: 2
            truncationMode: TruncationMode.Fade
        }

        BackgroundItem {
            id: offlineRetry

            objectName: "offlineRetryButton"
            anchors.right: parent.right
            anchors.rightMargin: Theme.horizontalPageMargin
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(Theme.itemSizeMedium,
                            offlineRetryLabel.implicitWidth
                            + 2 * Theme.paddingMedium)
            height: Theme.itemSizeSmall
            visible: root.retryVisible
            onClicked: root.retryRequested()

            Label {
                id: offlineRetryLabel

                anchors.centerIn: parent
                text: qsTr("Retry")
                color: parent.highlighted ? Theme.highlightColor
                                          : Theme.primaryColor
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }
}
