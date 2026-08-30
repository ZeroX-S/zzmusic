#include <QtTest>

#include "playback/platformaudiomonitor.h"

#include <QSignalSpy>

class PlatformAudioMonitorTest : public QObject
{
    Q_OBJECT

    static uint route(uint location)
    {
        return uint(PlatformAudioMonitor::RouteOutput)
            | uint(PlatformAudioMonitor::RouteActive)
            | location;
    }

    static uint snapshotRoute(uint location)
    {
        return uint(PlatformAudioMonitor::RouteOutput) | location;
    }

private slots:
    void initialSnapshotBuildsBaselineWithoutNotification()
    {
        PlatformAudioMonitor monitor(0, false);
        QSignalSpy removed(&monitor, SIGNAL(headsetRemoved()));

        monitor.applyActiveRoutesResult(
            monitor.m_generation, true,
            snapshotRoute(uint(PlatformAudioMonitor::RouteWired)));
        QCOMPARE(removed.count(), 0);

        monitor.audioRouteChanged(
            QStringLiteral("builtin"),
            route(uint(PlatformAudioMonitor::RouteBuiltin)));
        QCOMPARE(removed.count(), 1);

        monitor.audioRouteChanged(
            QStringLiteral("builtin"),
            route(uint(PlatformAudioMonitor::RouteBuiltin)));
        QCOMPARE(removed.count(), 1);
    }

    void classifiesRouteTransitions_data()
    {
        QTest::addColumn<uint>("previous");
        QTest::addColumn<uint>("current");
        QTest::addColumn<bool>("expectedRemoval");

        QTest::newRow("wired-to-builtin")
            << uint(PlatformAudioMonitor::RouteWired)
            << uint(PlatformAudioMonitor::RouteBuiltin) << true;
        QTest::newRow("wireless-to-builtin")
            << uint(PlatformAudioMonitor::RouteWireless)
            << uint(PlatformAudioMonitor::RouteBuiltin) << true;
        QTest::newRow("wired-to-combined-accessory")
            << uint(PlatformAudioMonitor::RouteWired)
            << uint(PlatformAudioMonitor::RouteBuiltin
                    | PlatformAudioMonitor::RouteWired) << false;
        QTest::newRow("combined-accessory-to-builtin")
            << uint(PlatformAudioMonitor::RouteBuiltin
                    | PlatformAudioMonitor::RouteWired)
            << uint(PlatformAudioMonitor::RouteBuiltin) << true;
        QTest::newRow("builtin-to-builtin")
            << uint(PlatformAudioMonitor::RouteBuiltin)
            << uint(PlatformAudioMonitor::RouteBuiltin) << false;
        QTest::newRow("wired-to-wireless")
            << uint(PlatformAudioMonitor::RouteWired)
            << uint(PlatformAudioMonitor::RouteWireless) << false;
        QTest::newRow("builtin-to-wired")
            << uint(PlatformAudioMonitor::RouteBuiltin)
            << uint(PlatformAudioMonitor::RouteWired) << false;
    }

    void classifiesRouteTransitions()
    {
        QFETCH(uint, previous);
        QFETCH(uint, current);
        QFETCH(bool, expectedRemoval);

        PlatformAudioMonitor monitor(0, false);
        QSignalSpy removed(&monitor, SIGNAL(headsetRemoved()));
        monitor.applyActiveRoutesResult(
            monitor.m_generation, true, route(previous));
        monitor.audioRouteChanged(QStringLiteral("next"), route(current));
        QCOMPARE(removed.count(), expectedRemoval ? 1 : 0);
    }

    void accessoryBitsTakePriorityInRawRouteTransitions()
    {
        PlatformAudioMonitor monitor(0, false);
        QSignalSpy removed(&monitor, SIGNAL(headsetRemoved()));

        monitor.applyActiveRoutesResult(monitor.m_generation, true, 9u);
        monitor.audioRouteChanged(QStringLiteral("combined"), route(13u));
        QCOMPARE(removed.count(), 0);

        monitor.audioRouteChanged(QStringLiteral("builtin"), route(5u));
        QCOMPARE(removed.count(), 1);
    }

    void ignoresInactiveAndInputRouteSignals()
    {
        PlatformAudioMonitor monitor(0, false);
        QSignalSpy removed(&monitor, SIGNAL(headsetRemoved()));
        monitor.applyActiveRoutesResult(
            monitor.m_generation, true,
            route(uint(PlatformAudioMonitor::RouteWired)));

        monitor.audioRouteChanged(
            QStringLiteral("builtin"),
            uint(PlatformAudioMonitor::RouteOutput
                 | PlatformAudioMonitor::RouteBuiltin));
        monitor.audioRouteChanged(
            QStringLiteral("microphone"),
            uint(PlatformAudioMonitor::RouteInput
                 | PlatformAudioMonitor::RouteBuiltin
                 | PlatformAudioMonitor::RouteActive));
        QCOMPARE(removed.count(), 0);

        monitor.audioRouteChanged(
            QStringLiteral("builtin"),
            route(uint(PlatformAudioMonitor::RouteBuiltin)));
        QCOMPARE(removed.count(), 1);
    }

    void serviceRestartAndErrorsRebuildBaseline()
    {
        PlatformAudioMonitor monitor(0, false);
        QSignalSpy removed(&monitor, SIGNAL(headsetRemoved()));
        const QString service = QStringLiteral(
            "org.nemomobile.Route.Manager");

        monitor.applyActiveRoutesResult(
            monitor.m_generation, true,
            route(uint(PlatformAudioMonitor::RouteWired)));
        monitor.routeServiceOwnerChanged(
            service, QStringLiteral(":1.10"), QString());
        monitor.audioRouteChanged(
            QStringLiteral("builtin"),
            route(uint(PlatformAudioMonitor::RouteBuiltin)));
        QCOMPARE(removed.count(), 0);

        monitor.audioRouteChanged(
            QStringLiteral("headphones"),
            route(uint(PlatformAudioMonitor::RouteWired)));
        monitor.routeServiceOwnerChanged(
            service, QStringLiteral(":1.10"), QStringLiteral(":1.11"));
        monitor.applyActiveRoutesResult(monitor.m_generation, false, 0);
        monitor.audioRouteChanged(
            QStringLiteral("builtin"),
            route(uint(PlatformAudioMonitor::RouteBuiltin)));
        QCOMPARE(removed.count(), 0);
    }

    void staleSnapshotCannotOverwriteNewerSignal()
    {
        PlatformAudioMonitor monitor(0, false);
        QSignalSpy removed(&monitor, SIGNAL(headsetRemoved()));
        const quint64 staleGeneration = monitor.m_generation;

        monitor.audioRouteChanged(
            QStringLiteral("headphones"),
            route(uint(PlatformAudioMonitor::RouteWired)));
        monitor.applyActiveRoutesResult(
            staleGeneration, true,
            route(uint(PlatformAudioMonitor::RouteBuiltin)));
        QCOMPARE(removed.count(), 0);

        monitor.audioRouteChanged(
            QStringLiteral("builtin"),
            route(uint(PlatformAudioMonitor::RouteBuiltin)));
        QCOMPARE(removed.count(), 1);
    }
};

QTEST_GUILESS_MAIN(PlatformAudioMonitorTest)
#include "tst_platformaudiomonitor.moc"
