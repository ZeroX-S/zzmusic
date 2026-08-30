import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: root

    visible: false

    property int _defaultPageOrientations: Orientation.Portrait
    property int allowedOrientations: Orientation.All
    property int defaultPageCutoutMode: CutoutMode.AvoidLandscapeCutout
    property int deviceOrientation: Orientation.Portrait
    property bool _backgroundVisible: true
    property bool _dimmingActive: false
    property bool _rotating: false
    property color _backgroundColor: "black"
    property Item contentItem: root
    property Item indicatorParentItem: root
    property Item _rotatingItem: root
    property Item _touchBlockerItem: root

    function _selectOrientation(allowed, current) {
        return Orientation.None
    }
}
