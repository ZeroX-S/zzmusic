import QtQuick 2.0
import Sailfish.Silica 1.0

SilicaGridView {
    id: grid

    property var controller
    property int columns: 2
    property real tilePadding: Theme.paddingSmall
    property real viewportContentY: 0
    property real viewportHeight: height
    property real viewportOffsetY: y
    property real artworkPrefetchMargin: Math.max(viewportHeight,
                                                   Theme.itemSizeHuge * 2)
    readonly property real artworkExtent: Math.max(0, cellWidth
                                                    - 2 * tilePadding)
    readonly property real textExtent: Theme.itemSizeMedium
                                       + Theme.paddingMedium
    readonly property bool stableCellGeometry: cellWidth > 0
                                               && artworkExtent > 0
                                               && cellHeight
                                                  === artworkExtent
                                                     + textExtent
    signal playlistClicked(string playlistId)

    interactive: false
    clip: true
    cellWidth: columns > 0 ? width / columns : width
    cellHeight: artworkExtent + textExtent
    height: Math.ceil(count / Math.max(1, columns)) * cellHeight

    delegate: BackgroundItem {
        id: playlistTile
        objectName: "playlistTile_" + playlistTile.playlistIdentifier
        readonly property string playlistIdentifier: model
                                                     && model.playlistId !== undefined
                                                     ? String(model.playlistId)
                                                     : ""
        width: grid.cellWidth
        height: grid.cellHeight
        enabled: playlistIdentifier.length > 0
        onClicked: grid.playlistClicked(playlistIdentifier)

        CachedArtwork {
            id: artworkImage
            objectName: "playlistArtwork_" + playlistTile.playlistIdentifier
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: grid.tilePadding
            height: width
            controller: grid.controller
            remoteSource: artwork
            active: grid.delegateArtworkActive(playlistTile.y,
                                                playlistTile.height)
            fillMode: Image.PreserveAspectCrop
            sourceSize.width: Math.max(1, Math.round(width))
            sourceSize.height: Math.max(1, Math.round(height))

            Rectangle {
                anchors.fill: parent
                visible: artworkImage.status !== Image.Ready
                color: Theme.rgba(Theme.highlightBackgroundColor, 0.18)

                Image {
                    anchors.centerIn: parent
                    source: "image://theme/icon-m-music"
                    opacity: Theme.opacityLow
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    running: artworkImage.active
                             && artwork && artwork.toString().length > 0
                             && artworkImage.status !== Image.Ready
                             && artworkImage.status !== Image.Error
                    visible: running
                    size: BusyIndicatorSize.Small
                }
            }
        }

        Label {
            id: titleLabel
            anchors.top: artworkImage.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: grid.tilePadding
            text: title
            color: parent.highlighted ? Theme.highlightColor
                                      : Theme.primaryColor
            font.pixelSize: Theme.fontSizeSmall
            font.weight: Font.DemiBold
            height: Theme.itemSizeMedium
            wrapMode: Text.Wrap
            maximumLineCount: 2
            truncationMode: TruncationMode.Fade
        }

        Label {
            objectName: "playlistMeta_" + playlistTile.playlistIdentifier
            anchors.top: titleLabel.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: grid.tilePadding
            anchors.rightMargin: grid.tilePadding
            text: grid.playlistMeta(trackCount, author)
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeExtraSmall
            truncationMode: TruncationMode.Fade
        }
    }

    function delegateArtworkActive(delegateY, delegateHeight) {
        if (viewportHeight <= 0)
            return true
        var itemTop = viewportOffsetY + delegateY
        var visibleTop = viewportContentY - artworkPrefetchMargin
        var visibleBottom = viewportContentY + viewportHeight
                            + artworkPrefetchMargin
        return itemTop + delegateHeight >= visibleTop
                && itemTop <= visibleBottom
    }

    function playlistMeta(count, playlistAuthor) {
        var countText = count > 0 ? qsTr("%1 tracks").arg(count) : ""
        var authorText = playlistAuthor || qsTr("QQ Music")
        return countText.length > 0 ? countText + " · " + authorText
                                    : authorText
    }
}
