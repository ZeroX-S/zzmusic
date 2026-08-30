import QtQuick 2.0
import Sailfish.Silica 1.0
import "cover" as Cover
import "internal" as Internal
import "pages" as Pages

ApplicationWindow {
    Internal.SecretsStore {
        id: secretsStore
    }

    Internal.AppNotifications {
        anchors.fill: parent
        controller: appController
    }

    allowedOrientations: Orientation.All

    initialPage: Component {
        Pages.HomePage { controller: appController }
    }

    cover: Component {
        Cover.AppCover { controller: appController }
    }
}
