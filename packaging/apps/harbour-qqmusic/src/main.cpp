#include "applicationcontroller.h"
#include "login/nativeloginmanager.h"
#include "playback/audioengine.h"
#include "playback/mpriscontroller.h"
#include "playback/platformaudiomonitor.h"
#include "playback/playbackcontroller.h"
#include "service/qqmusicservice.h"
#include "session/qqcookiejar.h"
#include "session/secretidentifierfactory.h"
#include "session/sessionstore.h"
#include "storage/imagecache.h"
#include "storage/settingsstore.h"

#include <QDir>
#include <QGuiApplication>
#include <QNetworkAccessManager>
#include <QLocale>
#include <QStandardPaths>
#include <QQuickItem>
#include <QQuickView>
#include <QScopedPointer>
#include <QTranslator>
#include <QQmlContext>
#include <QVariant>

#include <sailfishapp.h>

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    registerApplicationControllerTypes();
    QTranslator applicationTranslator;
    if (QLocale::system().name().startsWith(QStringLiteral("zh_CN"))
            && applicationTranslator.load(QStringLiteral(
                "/usr/share/translations/harbour-qqmusic-zh_CN.qm"))) {
        app->installTranslator(&applicationTranslator);
    }
    QNetworkAccessManager networkManager;
    QqCookieJar *activeCookieJar = new QqCookieJar(&networkManager);
    networkManager.setCookieJar(activeCookieJar);
    QqMusicService service(&networkManager);
    SettingsStore settingsStore;
    const qint64 networkCacheBytes = qMax<qint64>(1, qMin(
        SettingsStore::networkCacheBytes(), settingsStore.cacheLimitBytes() / 4));
    const QString cacheRoot = QDir(QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation)).filePath(QStringLiteral("harbour-qqmusic"));
    service.configurePublicCache(
        QDir(cacheRoot).filePath(QStringLiteral("metadata")),
        networkCacheBytes);
    ImageCache imageCache(
        QDir(cacheRoot).filePath(QStringLiteral("artwork")),
        qMax<qint64>(1, settingsStore.cacheLimitBytes() - networkCacheBytes));
    SessionStore sessionStore(activeCookieJar);
    NativeLoginManager loginManager;
    SecretIdentifierFactory secretIdentifierFactory;
    AudioEngine audioEngine;
    PlatformAudioMonitor platformAudioMonitor;
    PlaybackController playbackController(&service, &audioEngine);
    MprisController mprisController(&playbackController);
    ApplicationController controller(&service);
    QObject::connect(&platformAudioMonitor,
                     &PlatformAudioMonitor::headsetRemoved,
                     &audioEngine, &AudioEngine::notifyHeadsetRemoved);
    controller.setSessionStore(&sessionStore);
    controller.setNativeLoginManager(&loginManager);
    controller.setPlaybackController(&playbackController);
    controller.setStorage(&settingsStore, &imageCache);
    QScopedPointer<QQuickView> view(SailfishApp::createView());
    QQuickView *viewObject = view.data();
    QObject::connect(&mprisController, &MprisController::quitRequested,
                     app.data(), &QCoreApplication::quit);
    QObject::connect(&mprisController, &MprisController::raiseRequested,
                     viewObject, [viewObject]() {
        viewObject->show();
        viewObject->requestActivate();
    });

    view->rootContext()->setContextProperty(
        QStringLiteral("appController"), &controller);
    view->setSource(SailfishApp::pathTo("qml/main.qml"));
    if (view->rootObject()) {
        QObject *bridge = view->rootObject()->findChild<QObject *>(
            QStringLiteral("qqSecretsStore"));
        if (bridge) {
            bridge->setProperty(
                "identifierFactory",
                QVariant::fromValue(
                    static_cast<QObject *>(&secretIdentifierFactory)));
        }
        if (sessionStore.bindBridge(bridge)) {
            sessionStore.load();
        }
    }
    view->show();

    return app->exec();
}
