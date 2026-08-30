#include <QtTest>

#include "playback/audioengine.h"

#include <QMediaPlayer>
#include <QSignalSpy>

class AudioEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void mapsBackendStatesAndStatuses()
    {
        AudioEngine engine(0, false);
        QSignalSpy loaded(&engine, SIGNAL(loaded()));
        QSignalSpy playing(&engine, SIGNAL(playing()));
        QSignalSpy paused(&engine, SIGNAL(paused()));
        QSignalSpy stalled(&engine, SIGNAL(stalled()));
        QSignalSpy ended(&engine, SIGNAL(ended()));
        QSignalSpy failed(&engine, SIGNAL(failed(ErrorKind)));

        QVERIFY(QMetaObject::invokeMethod(
            &engine, "mediaStatusChanged", Qt::DirectConnection,
            Q_ARG(int, int(QMediaPlayer::LoadedMedia))));
        QVERIFY(QMetaObject::invokeMethod(
            &engine, "playerStateChanged", Qt::DirectConnection,
            Q_ARG(int, int(QMediaPlayer::PlayingState))));
        QVERIFY(QMetaObject::invokeMethod(
            &engine, "playerStateChanged", Qt::DirectConnection,
            Q_ARG(int, int(QMediaPlayer::PausedState))));
        QVERIFY(QMetaObject::invokeMethod(
            &engine, "mediaStatusChanged", Qt::DirectConnection,
            Q_ARG(int, int(QMediaPlayer::StalledMedia))));
        QVERIFY(QMetaObject::invokeMethod(
            &engine, "mediaStatusChanged", Qt::DirectConnection,
            Q_ARG(int, int(QMediaPlayer::EndOfMedia))));
        QVERIFY(QMetaObject::invokeMethod(
            &engine, "mediaStatusChanged", Qt::DirectConnection,
            Q_ARG(int, int(QMediaPlayer::InvalidMedia))));

        QCOMPARE(loaded.count(), 1);
        QCOMPARE(playing.count(), 1);
        QCOMPARE(paused.count(), 1);
        QCOMPARE(stalled.count(), 1);
        QCOMPARE(ended.count(), 1);
        QCOMPARE(failed.count(), 1);
    }

    void rejectsUnsafeMediaAndForwardsInterruptions()
    {
        AudioEngine engine(0, false);
        QSignalSpy failed(&engine, SIGNAL(failed(ErrorKind)));
        QSignalSpy began(&engine, SIGNAL(interruptionBegan()));
        QSignalSpy ended(&engine, SIGNAL(interruptionEnded()));
        QSignalSpy headset(&engine, SIGNAL(headsetRemoved()));

        engine.load(QUrl(QStringLiteral("http://example.invalid/audio")));
        QCOMPARE(failed.count(), 1);
        engine.notifyInterruptionBegan();
        engine.notifyInterruptionEnded();
        engine.notifyHeadsetRemoved();
        QCOMPARE(began.count(), 1);
        QCOMPARE(ended.count(), 1);
        QCOMPARE(headset.count(), 1);
    }
};

QTEST_GUILESS_MAIN(AudioEngineTest)
#include "tst_audioengine.moc"
