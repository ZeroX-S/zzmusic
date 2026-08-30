#include <QtTest>

#include "playback/audioengine.h"
#include "playback/mpriscontroller.h"
#include "playback/playbackcontroller.h"

#include <QDBusObjectPath>
#include <QSignalSpy>

namespace {

class FakeAudioEngine : public AudioEngine
{
public:
    FakeAudioEngine() : AudioEngine(0, false), pauseCount(0), seekMs(-1) {}
    void pause() override { ++pauseCount; emit paused(); }
    void seek(qint64 value) override { seekMs = value; emit positionChanged(value); }
    void advance(qint64 value) { emit positionChanged(value); }
    int pauseCount;
    qint64 seekMs;
};

Track track(const QString &mid, const QString &title)
{
    Track value;
    value.mid = mid;
    value.title = title;
    value.artist = QStringLiteral("Artist");
    value.album = QStringLiteral("Album");
    value.artworkUrl = QUrl(QStringLiteral(
        "https://y.gtimg.cn/music/photo_new/cover.jpg"));
    value.durationMs = 180000;
    return value;
}

}

class MprisControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void publishesStableMetadataAndCapabilities()
    {
        FakeAudioEngine engine;
        PlaybackController playback(0, &engine);
        QVector<Track> queue;
        queue << track(QStringLiteral("001AAA"), QStringLiteral("Song"))
              << track(QStringLiteral("002BBB"), QStringLiteral("Next"));
        playback.replaceQueue(queue, 0, false);
        MprisController mpris(&playback, false);

        const QVariantMap metadata = mpris.metadata();
        QCOMPARE(metadata.value(QStringLiteral("xesam:title")).toString(),
                 QStringLiteral("Song"));
        QCOMPARE(metadata.value(QStringLiteral("xesam:artist")).toStringList(),
                 QStringList() << QStringLiteral("Artist"));
        QCOMPARE(metadata.value(QStringLiteral("xesam:album")).toString(),
                 QStringLiteral("Album"));
        QCOMPARE(metadata.value(QStringLiteral("mpris:length")).toLongLong(),
                 qlonglong(180000000));
        const QDBusObjectPath firstId = metadata.value(
            QStringLiteral("mpris:trackid")).value<QDBusObjectPath>();
        QCOMPARE(firstId.path(), mpris.currentTrackId().path());
        QVERIFY(firstId.path().startsWith(
            QStringLiteral("/org/bonito/qqmusic/track_")));
        QVERIFY(mpris.canGoNext());
        QVERIFY(!mpris.canGoPrevious());
        QVERIFY(mpris.canSeek());
    }

    void mapsPlaybackStateAndTransportMethods()
    {
        FakeAudioEngine engine;
        PlaybackController playback(0, &engine);
        playback.replaceQueue(QVector<Track>()
            << track(QStringLiteral("001AAA"), QStringLiteral("Song")),
            0, false);
        MprisController mpris(&playback, false);
        QSignalSpy seeked(&mpris, SIGNAL(seeked(qlonglong)));
        QCOMPARE(mpris.playbackStatus(), QStringLiteral("Stopped"));
        QVERIFY(QMetaObject::invokeMethod(
            &playback, "enginePlaying", Qt::DirectConnection));
        QCOMPARE(mpris.playbackStatus(), QStringLiteral("Playing"));
        mpris.playPause();
        QCOMPARE(engine.pauseCount, 1);
        QCOMPARE(mpris.playbackStatus(), QStringLiteral("Paused"));

        const QDBusObjectPath id = mpris.currentTrackId();
        mpris.setPosition(id, 42000000);
        QCOMPARE(engine.seekMs, qint64(42000));
        QCOMPARE(seeked.count(), 1);
        QCOMPARE(seeked.at(0).at(0).toLongLong(), qlonglong(42000000));
        mpris.seek(3000000);
        QCOMPARE(engine.seekMs, qint64(45000));
        QCOMPARE(seeked.count(), 2);
        QCOMPARE(seeked.at(1).at(0).toLongLong(), qlonglong(45000000));
        mpris.setPosition(QDBusObjectPath(QStringLiteral("/wrong")), 1);
        QCOMPARE(engine.seekMs, qint64(45000));
        QCOMPARE(seeked.count(), 2);
    }

    void emitsSeekedOnlyForDiscontinuousPositionChanges()
    {
        FakeAudioEngine engine;
        PlaybackController playback(0, &engine);
        playback.replaceQueue(QVector<Track>()
            << track(QStringLiteral("001AAA"), QStringLiteral("Song")),
            0, false);
        MprisController mpris(&playback, false);
        QSignalSpy published(&mpris,
                             SIGNAL(propertiesPublished(QString,QVariantMap)));
        QSignalSpy seeked(&mpris, SIGNAL(seeked(qlonglong)));
        for (int index = 0; index < 20; ++index) {
            engine.advance(index * 1000);
        }
        QCOMPARE(published.count(), 0);
        QCOMPARE(seeked.count(), 0);

        playback.seek(42000);
        QCOMPARE(seeked.count(), 1);
        QCOMPARE(seeked.at(0).at(0).toLongLong(), qlonglong(42000000));
        QCOMPARE(published.count(), 0);
    }

    void publishesRestoredPositionAsSeeked()
    {
        FakeAudioEngine engine;
        PlaybackController playback(0, &engine);
        MprisController mpris(&playback, false);
        QSignalSpy seeked(&mpris, SIGNAL(seeked(qlonglong)));

        playback.restoreQueue(
            QVector<Track>()
                << track(QStringLiteral("001AAA"), QStringLiteral("Song")),
            QStringLiteral("001AAA"), PlaybackController::RepeatOff, 42000);
        QVERIFY(QMetaObject::invokeMethod(
            &playback, "engineLoaded", Qt::DirectConnection));

        QCOMPARE(engine.seekMs, qint64(42000));
        QCOMPARE(seeked.count(), 1);
        QCOMPARE(seeked.at(0).at(0).toLongLong(), qlonglong(42000000));
    }

    void exposesShuffleIndependentlyFromLoopStatus()
    {
        FakeAudioEngine engine;
        PlaybackController playback(0, &engine);
        playback.replaceQueue(QVector<Track>()
            << track(QStringLiteral("001AAA"), QStringLiteral("Song"))
            << track(QStringLiteral("002BBB"), QStringLiteral("Next")),
            0, false);
        MprisController mpris(&playback, false);
        QSignalSpy published(&mpris,
                             SIGNAL(propertiesPublished(QString,QVariantMap)));

        mpris.setShuffle(true);
        QVERIFY(mpris.shuffle());
        QCOMPARE(mpris.loopStatus(), QStringLiteral("None"));
        QCOMPARE(playback.repeatMode(), int(PlaybackController::Shuffle));
        QVERIFY(published.count() > 0);
        bool publishedShuffle = false;
        for (int index = 0; index < published.count(); ++index) {
            const QVariantMap changed = published.at(index).at(1).toMap();
            if (changed.value(QStringLiteral("Shuffle")).toBool()) {
                publishedShuffle = true;
                break;
            }
        }
        QVERIFY(publishedShuffle);

        mpris.setShuffle(false);
        QVERIFY(!mpris.shuffle());
        QCOMPARE(playback.repeatMode(), int(PlaybackController::RepeatOff));
    }
};

QTEST_GUILESS_MAIN(MprisControllerTest)
#include "tst_mpriscontroller.moc"
