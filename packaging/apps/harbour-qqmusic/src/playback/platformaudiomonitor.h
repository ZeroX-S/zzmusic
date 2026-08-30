#ifndef QQMUSIC_PLATFORMAUDIOMONITOR_H
#define QQMUSIC_PLATFORMAUDIOMONITOR_H

#include <QDBusConnection>
#include <QObject>

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;
class PlatformAudioMonitorTest;

class PlatformAudioMonitor : public QObject
{
    Q_OBJECT

public:
    explicit PlatformAudioMonitor(QObject *parent = 0,
                                  bool monitorSystemBus = true);

signals:
    void headsetRemoved();

private slots:
    void audioRouteChanged(const QString &device, uint deviceType);
    void routeServiceOwnerChanged(const QString &serviceName,
                                  const QString &oldOwner,
                                  const QString &newOwner);
    void activeRoutesFinished(QDBusPendingCallWatcher *watcher);

private:
    friend class PlatformAudioMonitorTest;

    // org.nemomobile.Route.Manager InterfaceVersion 3 route type bits.
    enum RouteType {
        RouteOutput = 1u << 0,
        RouteInput = 1u << 1,
        RouteBuiltin = 1u << 2,
        RouteWired = 1u << 3,
        RouteWireless = 1u << 4,
        RouteActive = 1u << 27
    };

    static bool isActiveOutput(uint deviceType);
    static bool isAccessory(uint deviceType);
    static bool isBuiltinOutput(uint deviceType);
    void resetBaseline();
    void requestActiveRoutes();
    void applyActiveRoutesResult(quint64 generation, bool valid,
                                 uint outputDeviceType);
    void observeActiveOutput(uint deviceType, bool allowNotification);

    QDBusConnection m_bus;
    QDBusServiceWatcher *m_serviceWatcher;
    bool m_monitorSystemBus;
    bool m_haveActiveOutput;
    uint m_activeOutputType;
    quint64 m_generation;
};

#endif
