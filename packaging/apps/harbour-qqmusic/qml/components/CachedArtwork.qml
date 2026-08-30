import QtQuick 2.0

Image {
    id: root

    property var controller
    property url remoteSource: ""
    property bool active: true
    property url resolvedSource: ""
    property var _requestedController: null
    property string _requestedRemoteSource: ""

    source: resolvedSource
    asynchronous: true
    cache: true

    onActiveChanged: {
        if (!active) {
            _requestedController = null
            _requestedRemoteSource = ""
        }
        refreshSource(false)
    }
    onControllerChanged: refreshSource()
    onRemoteSourceChanged: refreshSource()

    Connections {
        target: root.controller
        ignoreUnknownSignals: true

        onArtworkAvailable: {
            if (root.active
                    && String(remoteUrl) === String(root.remoteSource)) {
                root.resolvedSource = localUrl
            }
        }

        onArtworkCacheReset: root.refreshSource(true)
    }

    Component.onCompleted: refreshSource()

    function refreshSource(force) {
        if (!root.active || String(root.remoteSource).length === 0) {
            root._requestedController = null
            root._requestedRemoteSource = ""
            root.resolvedSource = ""
            return
        }
        if (!root.controller
                || typeof root.controller.artworkSource !== "function") {
            root.resolvedSource = root.remoteSource
            return
        }
        var remoteKey = String(root.remoteSource)
        if (!force
                && root._requestedController === root.controller
                && root._requestedRemoteSource === remoteKey) {
            return
        }
        root._requestedController = root.controller
        root._requestedRemoteSource = remoteKey
        root.resolvedSource = root.controller.artworkSource(root.remoteSource)
    }
}
