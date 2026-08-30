#include "platformaudiomonitor.h"

#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QVariant>

namespace {

const char routeService[] = "org.nemomobile.Route.Manager";
const char routePath[] = "/org/nemomobile/Route/Manager";
const char routeInterface[] = "org.nemomobile.Route.Manager";
const char routeChangedSignal[] = "AudioRouteChanged";
const char activeRoutesMethod[] = "ActiveRoutes";
const char generationProperty[] = "routeGeneration";

}

PlatformAudioMonitor::PlatformAudioMonitor(QObject *parent,
                                           bool monitorSystemBus)
    : QObject(parent)
    , m_bus(monitorSystemBus
                ? QDBusConnection::systemBus()
                : QDBusConnection(QString()))
    , m_serviceWatcher(0)
    , m_monitorSystemBus(monitorSystemBus)
    , m_haveActiveOutput(false)
    , m_activeOutputType(0)
    , m_generation(0)
{
    if (!m_monitorSystemBus || !m_bus.isConnected()) {
        return;
    }

    m_serviceWatcher = new QDBusServiceWatcher(
        QString::fromLatin1(routeService), m_bus,
        QDBusServiceWatcher::WatchForOwnerChange,
        this);
    connect(m_serviceWatcher,
            SIGNAL(serviceOwnerChanged(QString,QString,QString)),
            this, SLOT(routeServiceOwnerChanged(QString,QString,QString)));

    m_bus.connect(QString::fromLatin1(routeService),
                  QString::fromLatin1(routePath),
                  QString::fromLatin1(routeInterface),
                  QString::fromLatin1(routeChangedSignal),
                  this, SLOT(audioRouteChanged(QString,uint)));

    resetBaseline();
    requestActiveRoutes();
}

bool PlatformAudioMonitor::isActiveOutput(uint deviceType)
{
    return (deviceType & uint(RouteOutput))
        && (deviceType & uint(RouteActive));
}

bool PlatformAudioMonitor::isAccessory(uint deviceType)
{
    return deviceType & (uint(RouteWired) | uint(RouteWireless));
}

bool PlatformAudioMonitor::isBuiltinOutput(uint deviceType)
{
    return isActiveOutput(deviceType)
        && (deviceType & uint(RouteBuiltin))
        && !isAccessory(deviceType);
}

void PlatformAudioMonitor::resetBaseline()
{
    ++m_generation;
    m_haveActiveOutput = false;
    m_activeOutputType = 0;
}

void PlatformAudioMonitor::requestActiveRoutes()
{
    if (!m_monitorSystemBus || !m_bus.isConnected()) {
        return;
    }

    const QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(routeService),
        QString::fromLatin1(routePath),
        QString::fromLatin1(routeInterface),
        QString::fromLatin1(activeRoutesMethod));
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(
        m_bus.asyncCall(message), this);
    watcher->setProperty(generationProperty,
                         QVariant::fromValue<qulonglong>(m_generation));
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(activeRoutesFinished(QDBusPendingCallWatcher*)));
}

void PlatformAudioMonitor::applyActiveRoutesResult(
    quint64 generation, bool valid, uint outputDeviceType)
{
    if (generation != m_generation) {
        return;
    }

    m_haveActiveOutput = false;
    m_activeOutputType = 0;
    if (valid && (outputDeviceType & uint(RouteOutput))) {
        observeActiveOutput(outputDeviceType | uint(RouteActive), false);
    }
}

void PlatformAudioMonitor::observeActiveOutput(uint deviceType,
                                               bool allowNotification)
{
    if (!isActiveOutput(deviceType)) {
        return;
    }

    const bool removed = allowNotification
        && m_haveActiveOutput
        && isAccessory(m_activeOutputType)
        && isBuiltinOutput(deviceType);
    m_haveActiveOutput = true;
    m_activeOutputType = deviceType;
    if (removed) {
        emit headsetRemoved();
    }
}

void PlatformAudioMonitor::audioRouteChanged(const QString &device,
                                             uint deviceType)
{
    Q_UNUSED(device)
    if (!isActiveOutput(deviceType)) {
        return;
    }

    ++m_generation;
    observeActiveOutput(deviceType, true);
}

void PlatformAudioMonitor::routeServiceOwnerChanged(
    const QString &serviceName, const QString &oldOwner,
    const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    if (serviceName != QString::fromLatin1(routeService)) {
        return;
    }

    resetBaseline();
    if (!newOwner.isEmpty()) {
        requestActiveRoutes();
    }
}

void PlatformAudioMonitor::activeRoutesFinished(
    QDBusPendingCallWatcher *watcher)
{
    const quint64 generation = watcher->property(
        generationProperty).toULongLong();
    const QDBusPendingReply<QString, uint, QString, uint> reply = *watcher;
    applyActiveRoutesResult(generation, !reply.isError(),
                            reply.isError() ? 0 : reply.argumentAt<1>());
    watcher->deleteLater();
}
