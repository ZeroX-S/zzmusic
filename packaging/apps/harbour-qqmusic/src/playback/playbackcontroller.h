#ifndef QQMUSIC_PLAYBACKCONTROLLER_H
#define QQMUSIC_PLAYBACKCONTROLLER_H

#include "core/types.h"

#include <QObject>
#include <QPointer>
#include <QVariantList>

class AudioEngine;
class QqMusicService;

class PlaybackController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(int repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(QVariantList queue READ queue NOTIFY queueChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentChanged)
    Q_PROPERTY(QVariantMap currentTrackValue READ currentTrackValue NOTIFY currentChanged)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY durationChanged)
    Q_PROPERTY(int audioSampleRateHz READ audioSampleRateHz NOTIFY audioDetailsChanged)
    Q_PROPERTY(int audioBitrateKbps READ audioBitrateKbps NOTIFY audioDetailsChanged)
    Q_PROPERTY(QString audioFormat READ audioFormat NOTIFY audioDetailsChanged)
    Q_PROPERTY(bool audioLossless READ audioLossless NOTIFY audioDetailsChanged)
    Q_PROPERTY(bool canPlay READ canPlay NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY stateChanged)
    Q_PROPERTY(bool canPause READ canPause NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY capabilitiesChanged)

public:
    enum State {
        Idle,
        Resolving,
        Loading,
        Playing,
        Paused,
        Stalled,
        Failed
    };
    Q_ENUMS(State)

    enum Repeat {
        RepeatOff,
        RepeatAll,
        RepeatOne,
        Shuffle
    };
    Q_ENUMS(Repeat)

    PlaybackController(QqMusicService *service, AudioEngine *engine,
                       QObject *parent = 0);

    int state() const;
    int repeatMode() const;
    void setRepeatMode(int repeatMode);
    QVariantList queue() const;
    QVector<Track> queueTracks() const;
    int currentIndex() const;
    Track currentTrack() const;
    QVariantMap currentTrackValue() const;
    qint64 positionMs() const;
    qint64 durationMs() const;
    int audioSampleRateHz() const;
    int audioBitrateKbps() const;
    QString audioFormat() const;
    bool audioLossless() const;
    int consecutiveSkips() const;
    bool canPlay() const;
    bool isPlaying() const;
    bool canPause() const;
    bool canGoNext() const;
    bool canGoPrevious() const;
    bool currentLocationAccountBound() const;

    void replaceQueue(const QVector<Track> &tracks, int startIndex = 0,
                      bool autoplay = true);
    void restoreQueue(const QVector<Track> &tracks,
                      const QString &currentMid,
                      int repeatMode,
                      qint64 positionMs);
    void addNext(const Track &track);
    void append(const Track &track);

public slots:
    void play();
    void pause();
    void stop();
    void previous();
    void next();
    void retry();
    void seek(qint64 positionMs);
    void remove(int index);
    void clear();
    void setAccountSessionUin(const QString &uin);
    void sessionEnded();

signals:
    void stateChanged();
    void repeatModeChanged();
    void queueChanged();
    void currentChanged();
    void positionChanged();
    void seeked(qint64 positionMs);
    void durationChanged();
    void audioDetailsChanged();
    void capabilitiesChanged();
    void errorChanged(const ServiceError &error);

private slots:
    void playbackReady(quint64 operationId,
                       const PlaybackLocation &location);
    void serviceFailed(const ServiceError &error);
    void engineLoaded();
    void enginePlaying();
    void enginePaused();
    void engineStopped();
    void engineStalled();
    void engineEnded();
    void engineFailed(ErrorKind kind);
    void interruptionBegan();
    void interruptionEnded();
    void headsetRemoved();
    void updatePosition(qint64 positionMs);
    void updateDuration(qint64 durationMs);
    void updateMediaInfo(int sampleRateHz, int bitrateBps,
                         const QString &codec);

private:
    quint64 nextOperationId();
    void setState(State state);
    void resolveCurrent();
    void advance(bool manual, bool previous);
    void handleUnavailable(const ServiceError &error);
    void fail(const ServiceError &error);
    void setCurrentIndex(int index);
    void resetShuffleState();
    void rebuildShuffleUpcoming();
    int nextShuffleIndex(int upperExclusive);
    QVariantMap trackValue(const Track &track) const;
    void setAudioDetails(int sampleRateHz, int bitrateKbps,
                         const QString &format, bool lossless);
    void resetAudioDetails();
    void beginPausePositionGuard();
    void clearPausePositionGuard();

    QPointer<QqMusicService> m_service;
    QPointer<AudioEngine> m_engine;
    QVector<Track> m_queue;
    int m_currentIndex;
    State m_state;
    Repeat m_repeatMode;
    QVector<int> m_shuffleUpcoming;
    QVector<int> m_shuffleHistory;
    quint32 m_shuffleRandomState;
    quint64 m_nextOperationId;
    quint64 m_resolveOperationId;
    PlaybackLocation m_location;
    qint64 m_positionMs;
    qint64 m_durationMs;
    int m_audioSampleRateHz;
    int m_audioBitrateKbps;
    QString m_audioFormat;
    bool m_audioLossless;
    qint64 m_pendingSeekMs;
    int m_consecutiveSkips;
    bool m_autoplay;
    bool m_refreshedForCurrent;
    QString m_accountUin;
    bool m_resolveAccountBound;
    bool m_progressEvidenceLogged;
    bool m_userPaused;
    bool m_resumeAfterInterruption;
    bool m_pausePositionGuard;
    bool m_pauseBackendResetLogged;
    qint64 m_pausedPositionMs;
};

#endif
