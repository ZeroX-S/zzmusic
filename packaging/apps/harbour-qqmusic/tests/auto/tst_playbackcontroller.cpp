#include <QtTest>

#include "playback/audioengine.h"
#include "playback/playbackcontroller.h"
#include "service/qqmusicservice.h"

#include <QSet>

namespace {

Track track(const QString &mid)
{
    Track value;
    value.mid = mid;
    value.title = QStringLiteral("Title ") + mid;
    value.artist = QStringLiteral("Artist");
    value.album = QStringLiteral("Album");
    value.durationMs = 180000;
    value.playable = true;
    return value;
}

QVector<Track> tracks(int count)
{
    QVector<Track> values;
    for (int index = 0; index < count; ++index) {
        values.append(track(QStringLiteral("mid-%1").arg(index)));
    }
    return values;
}

class FakeService : public QqMusicService
{
    Q_OBJECT

public:
    explicit FakeService(QObject *parent = 0)
        : QqMusicService(parent)
        , lastOperationId(0)
        , containsCookies(false)
        , resolveCount(0)
        , lastCancelledOperationId(0)
        , cancelCount(0)
        , emitCancellationSynchronously(false)
    {
    }

    bool resolvePlayback(quint64 operationId,
                         const QString &trackMid,
                         const QString &accountUin) override
    {
        lastOperationId = operationId;
        lastMid = trackMid;
        lastAccountUin = accountUin;
        containsCookies = !accountUin.isEmpty();
        ++resolveCount;
        return true;
    }

    bool cancel(quint64 operationId) override
    {
        lastCancelledOperationId = operationId;
        ++cancelCount;
        if (emitCancellationSynchronously) {
            ServiceError error;
            error.kind = ErrorKind::Cancelled;
            error.operationId = operationId;
            error.diagnosticCode = QStringLiteral("test.playback.cancelled");
            emit failed(error);
        }
        return true;
    }

    void succeed(bool accountBound = false)
    {
        succeedFor(lastOperationId, accountBound);
    }

    void succeedFor(quint64 operationId, bool accountBound = false)
    {
        succeedForQuality(operationId, QStringLiteral("128 kbps"),
                          accountBound);
    }

    void succeedForQuality(quint64 operationId, const QString &quality,
                           bool accountBound = false)
    {
        PlaybackLocation location;
        location.url = QUrl(QStringLiteral(
            "https://isure.stream.qqmusic.qq.com/sanitized.mp3"));
        location.bitrate = quality;
        location.expiresAt = QDateTime::currentDateTimeUtc().addSecs(300);
        location.accountBound = accountBound;
        emit playbackReady(operationId, location);
    }

    void failResolve(ErrorKind kind)
    {
        ServiceError error;
        error.kind = kind;
        error.operationId = lastOperationId;
        error.diagnosticCode = QStringLiteral("test.playback.failure");
        emit failed(error);
    }

    quint64 lastOperationId;
    QString lastMid;
    QString lastAccountUin;
    bool containsCookies;
    int resolveCount;
    quint64 lastCancelledOperationId;
    int cancelCount;
    bool emitCancellationSynchronously;
};

class FakeAudioEngine : public AudioEngine
{
    Q_OBJECT

public:
    explicit FakeAudioEngine(QObject *parent = 0)
        : AudioEngine(parent, false)
        , autoLoad(true)
        , resetPositionOnPause(false)
        , resetPositionAfterPlay(false)
        , emitPositionOnSeek(true)
        , playCount(0)
        , pauseCount(0)
        , stopCount(0)
        , seekPosition(-1)
    {
    }

    void load(const QUrl &url) override
    {
        loadedUrl = url;
        if (autoLoad) {
            emit loaded();
        }
    }

    void play() override
    {
        ++playCount;
        emit playing();
        if (resetPositionAfterPlay) {
            emit positionChanged(0);
        }
    }

    void pause() override
    {
        ++pauseCount;
        if (resetPositionOnPause) {
            emit positionChanged(0);
        }
        emit paused();
    }

    void stop() override
    {
        ++stopCount;
        emit stopped();
    }

    void seek(qint64 positionMs) override
    {
        seekPosition = positionMs;
        if (emitPositionOnSeek) {
            emit positionChanged(positionMs);
        }
    }

    void fail(ErrorKind kind)
    {
        emit failed(kind);
    }

    void publishMediaInfo(int sampleRateHz, int bitrateBps,
                          const QString &codec)
    {
        emit mediaInfoChanged(sampleRateHz, bitrateBps, codec);
    }

    void finishMedia()
    {
        emit ended();
    }

    void publishPosition(qint64 positionMs)
    {
        emit positionChanged(positionMs);
    }

    void publishStopped()
    {
        emit stopped();
    }

    bool autoLoad;
    bool resetPositionOnPause;
    bool resetPositionAfterPlay;
    bool emitPositionOnSeek;
    int playCount;
    int pauseCount;
    int stopCount;
    qint64 seekPosition;
    QUrl loadedUrl;
};

}

class PlaybackControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void replacesQueueAtSelectedTrackAndStartsPlayback()
    {
        FakeService service;
        FakeAudioEngine engine;
        PlaybackController controller(&service, &engine);
        const QVector<Track> values = tracks(3);
        controller.replaceQueue(values, 1);
        QCOMPARE(controller.currentTrack().mid, values.at(1).mid);
        QCOMPARE(controller.state(), int(PlaybackController::Resolving));
        QCOMPARE(service.lastMid, values.at(1).mid);
        service.succeed();
        QCOMPARE(controller.state(), int(PlaybackController::Playing));
        QCOMPARE(engine.playCount, 1);
        QVERIFY(engine.loadedUrl.isValid());
    }

    void supportsQueueEditingAndManualNavigation()
    {
        FakeService service;
        FakeAudioEngine engine;
        PlaybackController controller(&service, &engine);
        controller.replaceQueue(tracks(3), 1, false);
        controller.addNext(track(QStringLiteral("next")));
        controller.append(track(QStringLiteral("last")));
        QCOMPARE(controller.queue().size(), 5);
        QCOMPARE(controller.queue().at(2).toMap().value("mid").toString(),
                 QStringLiteral("next"));
        controller.next();
        QCOMPARE(controller.currentTrack().mid, QStringLiteral("next"));
        controller.previous();
        QCOMPARE(controller.currentTrack().mid, QStringLiteral("mid-1"));
        controller.remove(0);
        QCOMPARE(controller.currentIndex(), 0);
        controller.clear();
        QVERIFY(controller.queue().isEmpty());
        QCOMPARE(controller.currentIndex(), -1);
        QCOMPARE(controller.state(), int(PlaybackController::Idle));
    }

    void exposesAuthorizedAndRuntimeAudioDetails()
    {
        FakeService service;
        FakeAudioEngine engine;
        PlaybackController controller(&service, &engine);
        controller.replaceQueue(tracks(1));
        service.succeedForQuality(service.lastOperationId,
                                  QStringLiteral("FLAC lossless"));

        QCOMPARE(controller.audioFormat(), QStringLiteral("FLAC"));
        QCOMPARE(controller.audioSampleRateHz(), 0);
        QCOMPARE(controller.audioBitrateKbps(), 0);
        QVERIFY(controller.audioLossless());

        QSignalSpy detailsChanged(&controller, SIGNAL(audioDetailsChanged()));
        engine.publishMediaInfo(96000, 2350123,
                                QStringLiteral("audio/x-flac"));
        QCOMPARE(detailsChanged.count(), 1);
        QCOMPARE(controller.audioFormat(), QStringLiteral("FLAC"));
        QCOMPARE(controller.audioSampleRateHz(), 96000);
        QCOMPARE(controller.audioBitrateKbps(), 2350);
        QVERIFY(controller.audioLossless());

        controller.stop();
        QVERIFY(controller.audioFormat().isEmpty());
        QCOMPARE(controller.audioSampleRateHz(), 0);
        QCOMPARE(controller.audioBitrateKbps(), 0);
        QVERIFY(!controller.audioLossless());
    }

    void enforcesRepeatModesAtEndOfMedia()
    {
        FakeService service;
        FakeAudioEngine engine;
        PlaybackController controller(&service, &engine);
        controller.replaceQueue(tracks(2), 1);
        service.succeed();
        engine.finishMedia();
        QCOMPARE(controller.state(), int(PlaybackController::Idle));

        controller.setRepeatMode(PlaybackController::RepeatAll);
        controller.play();
        service.succeed();
        engine.finishMedia();
        QCOMPARE(controller.currentIndex(), 0);

        controller.setRepeatMode(PlaybackController::RepeatOne);
        service.succeed();
        const QString mid = controller.currentTrack().mid;
        engine.finishMedia();
        QCOMPARE(controller.currentTrack().mid, mid);
        QCOMPARE(controller.state(), int(PlaybackController::Resolving));
    }

    void shufflesWithoutRepeatingAndKeepsPreviousHistory()
    {
        FakeService service;
        FakeAudioEngine engine;
        PlaybackController controller(&service, &engine);
        controller.replaceQueue(tracks(5), 0, false);
        controller.setRepeatMode(PlaybackController::Shuffle);
        QCOMPARE(controller.repeatMode(), int(PlaybackController::Shuffle));
        QVERIFY(controller.canGoNext());
        QVERIFY(!controller.canGoPrevious());

        QStringList order;
        QSet<QString> unique;
        order.append(controller.currentTrack().mid);
        unique.insert(controller.currentTrack().mid);
        for (int index = 0; index < 4; ++index) {
            controller.next();
            order.append(controller.currentTrack().mid);
            unique.insert(controller.currentTrack().mid);
        }
        QCOMPARE(order.size(), 5);
        QCOMPARE(unique.size(), 5);
        QVERIFY(controller.canGoPrevious());

        controller.previous();
        QCOMPARE(controller.currentTrack().mid, order.at(3));
        controller.next();
        QCOMPARE(controller.currentTrack().mid, order.at(4));

        controller.setRepeatMode(4);
        QCOMPARE(controller.repeatMode(), int(PlaybackController::Shuffle));
    }

    void refreshesOnceThenAppliesBoundedSkipRule()
    {
        FakeService service;
        FakeAudioEngine engine;
        PlaybackController controller(&service, &engine);
        controller.setRepeatMode(PlaybackController::RepeatAll);
        controller.replaceQueue(tracks(5), 0);
        service.succeed();
        const int initialResolves = service.resolveCount;
        engine.fail(ErrorKind::DecodeFailed);
        QCOMPARE(service.resolveCount, initialResolves + 1);
        service.succeed();
        engine.fail(ErrorKind::DecodeFailed);
        QCOMPARE(controller.currentIndex(), 1);

        service.failResolve(ErrorKind::NotPlayable);
        service.failResolve(ErrorKind::NotPlayable);
        service.failResolve(ErrorKind::NotPlayable);
        service.failResolve(ErrorKind::NotPlayable);
        QCOMPARE(controller.state(), int(PlaybackController::Failed));
        QCOMPARE(controller.consecutiveSkips(), 3);
    }

    void restoresQueueAndHandlesInterruptionsAndLogout()
    {
        FakeService service;
        FakeAudioEngine engine;
        PlaybackController controller(&service, &engine);
        const QString accountUin = QStringLiteral("1234567890");
        controller.setAccountSessionUin(accountUin);
        controller.restoreQueue(tracks(3), QStringLiteral("mid-2"),
                                PlaybackController::RepeatAll, 42000);
        QCOMPARE(controller.currentIndex(), 2);
        QVERIFY(service.containsCookies);
        QCOMPARE(service.lastAccountUin, accountUin);
        service.succeed(true);
        QCOMPARE(engine.seekPosition, qint64(42000));
        QCOMPARE(controller.state(), int(PlaybackController::Paused));

        controller.play();
        QCOMPARE(controller.state(), int(PlaybackController::Playing));
        engine.notifyInterruptionBegan();
        QCOMPARE(controller.state(), int(PlaybackController::Paused));
        engine.notifyInterruptionEnded();
        QCOMPARE(controller.state(), int(PlaybackController::Playing));
        controller.sessionEnded();
        QCOMPARE(controller.state(), int(PlaybackController::Idle));
    }

    void logoutCancelsPendingAccountResolveAndIgnoresLateSuccess()
    {
        FakeService service;
        FakeAudioEngine engine;
        PlaybackController controller(&service, &engine);
        QSignalSpy errors(&controller, SIGNAL(errorChanged(ServiceError)));
        controller.setAccountSessionUin(QStringLiteral("1234567890"));
        controller.replaceQueue(tracks(1));
        const quint64 operationId = service.lastOperationId;
        QVERIFY(service.containsCookies);

        controller.sessionEnded();
        QCOMPARE(service.cancelCount, 1);
        QCOMPARE(service.lastCancelledOperationId, operationId);
        QCOMPARE(controller.state(), int(PlaybackController::Idle));
        QCOMPARE(engine.playCount, 0);
        QVERIFY(engine.loadedUrl.isEmpty());
        QCOMPARE(errors.count(), 0);

        service.succeedFor(operationId, true);
        QCOMPARE(controller.state(), int(PlaybackController::Idle));
        QCOMPARE(engine.playCount, 0);
        QVERIFY(engine.loadedUrl.isEmpty());
        QCOMPARE(errors.count(), 0);
    }

    void logoutKeepsPendingPublicResolve()
    {
        FakeService service;
        FakeAudioEngine engine;
        PlaybackController controller(&service, &engine);
        controller.replaceQueue(tracks(1));
        const quint64 operationId = service.lastOperationId;
        QVERIFY(!service.containsCookies);

        controller.sessionEnded();
        QCOMPARE(service.cancelCount, 0);
        QCOMPARE(controller.state(), int(PlaybackController::Resolving));

        service.succeedFor(operationId);
        QCOMPARE(controller.state(), int(PlaybackController::Playing));
        QCOMPARE(engine.playCount, 1);
    }

    void accountSessionRetriesFailedPublicResolve()
    {
        FakeService service;
        FakeAudioEngine engine;
        PlaybackController controller(&service, &engine);
        controller.replaceQueue(tracks(1));
        QCOMPARE(service.resolveCount, 1);
        QVERIFY(service.lastAccountUin.isEmpty());
        service.failResolve(ErrorKind::NotPlayable);
        QCOMPARE(controller.state(), int(PlaybackController::Failed));

        const QString accountUin = QStringLiteral("1234567890");
        controller.setAccountSessionUin(accountUin);
        QCOMPARE(service.resolveCount, 2);
        QCOMPARE(service.lastAccountUin, accountUin);
        QCOMPARE(controller.state(), int(PlaybackController::Resolving));
    }

    void stopIgnoresSynchronousCancellationFailure()
    {
        FakeService service;
        FakeAudioEngine engine;
        PlaybackController controller(&service, &engine);
        QSignalSpy errors(&controller, SIGNAL(errorChanged(ServiceError)));
        controller.replaceQueue(tracks(1));
        const quint64 operationId = service.lastOperationId;
        service.emitCancellationSynchronously = true;

        controller.stop();
        QCOMPARE(service.cancelCount, 1);
        QCOMPARE(service.lastCancelledOperationId, operationId);
        QCOMPARE(controller.state(), int(PlaybackController::Idle));
        QCOMPARE(errors.count(), 0);
    }

    void replacementResolveIgnoresSynchronousCancellationFailure()
    {
        FakeService service;
        FakeAudioEngine engine;
        PlaybackController controller(&service, &engine);
        QSignalSpy errors(&controller, SIGNAL(errorChanged(ServiceError)));
        controller.replaceQueue(tracks(2));
        const quint64 previousOperationId = service.lastOperationId;
        service.emitCancellationSynchronously = true;

        controller.next();
        const quint64 replacementOperationId = service.lastOperationId;
        QCOMPARE(service.cancelCount, 1);
        QCOMPARE(service.lastCancelledOperationId, previousOperationId);
        QVERIFY(replacementOperationId != previousOperationId);
        QCOMPARE(controller.state(), int(PlaybackController::Resolving));
        QCOMPARE(errors.count(), 0);

        service.succeedFor(replacementOperationId);
        QCOMPARE(controller.state(), int(PlaybackController::Playing));
        QCOMPARE(engine.playCount, 1);
    }

    void headsetRemovalPausesWithoutAutomaticResume()
    {
        FakeService service;
        FakeAudioEngine engine;
        PlaybackController controller(&service, &engine);
        controller.replaceQueue(tracks(1));
        service.succeed();
        QCOMPARE(controller.state(), int(PlaybackController::Playing));

        const int playCount = engine.playCount;
        engine.notifyHeadsetRemoved();
        QCOMPARE(controller.state(), int(PlaybackController::Paused));
        QCOMPARE(engine.pauseCount, 1);

        engine.notifyInterruptionEnded();
        QCOMPARE(controller.state(), int(PlaybackController::Paused));
        QCOMPARE(engine.playCount, playCount);
    }

    void userPauseSurvivesAnInterruptionCycle()
    {
        FakeService service;
        FakeAudioEngine engine;
        PlaybackController controller(&service, &engine);
        controller.replaceQueue(tracks(1));
        service.succeed();
        controller.pause();
        const int playCount = engine.playCount;

        engine.notifyInterruptionBegan();
        engine.notifyInterruptionEnded();
        QCOMPARE(controller.state(), int(PlaybackController::Paused));
        QCOMPARE(engine.playCount, playCount);
    }

    void preservesPositionAcrossSailfish51PauseReset()
    {
        FakeService service;
        FakeAudioEngine engine;
        PlaybackController controller(&service, &engine);
        controller.replaceQueue(tracks(1));
        service.succeed();
        engine.publishPosition(42000);
        QCOMPARE(controller.positionMs(), qint64(42000));

        engine.resetPositionOnPause = true;
        controller.pause();
        QCOMPARE(controller.state(), int(PlaybackController::Paused));
        QCOMPARE(controller.positionMs(), qint64(42000));

        engine.publishStopped();
        QCOMPARE(controller.state(), int(PlaybackController::Paused));
        QCOMPARE(controller.positionMs(), qint64(42000));

        engine.emitPositionOnSeek = false;
        engine.resetPositionAfterPlay = true;
        controller.play();
        QCOMPARE(controller.state(), int(PlaybackController::Playing));
        QCOMPARE(controller.positionMs(), qint64(42000));
        QCOMPARE(engine.seekPosition, qint64(42000));

        engine.publishStopped();
        QCOMPARE(controller.state(), int(PlaybackController::Playing));
        QCOMPARE(controller.positionMs(), qint64(42000));

        engine.publishPosition(42100);
        QCOMPARE(controller.positionMs(), qint64(42100));
        engine.publishStopped();
        QCOMPARE(controller.state(), int(PlaybackController::Idle));
    }
};

QTEST_GUILESS_MAIN(PlaybackControllerTest)
#include "tst_playbackcontroller.moc"
