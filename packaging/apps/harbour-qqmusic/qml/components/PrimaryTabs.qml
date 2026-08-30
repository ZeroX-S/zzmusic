import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: root

    property int currentIndex: 0
    property string objectNamePrefix: "primary"
    property string recommendedText: ""
    property string discoverText: ""
    property string topChartsText: ""
    readonly property int tabCount: 3
    signal recommendedClicked()
    signal discoverClicked()
    signal topChartsClicked()

    height: Theme.itemSizeMedium

    Row {
        id: tabRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.horizontalPageMargin
        anchors.rightMargin: Theme.horizontalPageMargin
        height: parent.height

        BackgroundItem {
            id: recommendedTab
            objectName: root.objectNamePrefix + "RecommendedTab"
            width: tabRow.width / root.tabCount
            height: tabRow.height
            onClicked: root.recommendedClicked()

            Label {
                anchors.centerIn: parent
                text: root.recommendedText
                color: root.currentIndex === 0 || recommendedTab.highlighted
                       ? Theme.highlightColor : Theme.secondaryColor
                font.pixelSize: Theme.fontSizeMedium
                font.weight: root.currentIndex === 0
                             ? Font.DemiBold : Font.Normal
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: Theme.paddingMedium
                anchors.rightMargin: Theme.paddingMedium
                height: Theme.paddingSmall / 2
                radius: height / 2
                color: Theme.highlightColor
                visible: root.currentIndex === 0
            }
        }

        BackgroundItem {
            id: discoverTab
            objectName: root.objectNamePrefix + "DiscoverTab"
            width: tabRow.width / root.tabCount
            height: tabRow.height
            onClicked: root.discoverClicked()

            Label {
                anchors.centerIn: parent
                text: root.discoverText
                color: root.currentIndex === 1 || discoverTab.highlighted
                       ? Theme.highlightColor : Theme.secondaryColor
                font.pixelSize: Theme.fontSizeMedium
                font.weight: root.currentIndex === 1
                             ? Font.DemiBold : Font.Normal
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: Theme.paddingMedium
                anchors.rightMargin: Theme.paddingMedium
                height: Theme.paddingSmall / 2
                radius: height / 2
                color: Theme.highlightColor
                visible: root.currentIndex === 1
            }
        }

        BackgroundItem {
            id: topChartsTab
            objectName: root.objectNamePrefix + "TopChartsTab"
            width: tabRow.width / root.tabCount
            height: tabRow.height
            onClicked: root.topChartsClicked()

            Label {
                anchors.centerIn: parent
                text: root.topChartsText
                color: root.currentIndex === 2 || topChartsTab.highlighted
                       ? Theme.highlightColor : Theme.secondaryColor
                font.pixelSize: Theme.fontSizeMedium
                font.weight: root.currentIndex === 2
                             ? Font.DemiBold : Font.Normal
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: Theme.paddingMedium
                anchors.rightMargin: Theme.paddingMedium
                height: Theme.paddingSmall / 2
                radius: height / 2
                color: Theme.highlightColor
                visible: root.currentIndex === 2
            }
        }
    }
}
