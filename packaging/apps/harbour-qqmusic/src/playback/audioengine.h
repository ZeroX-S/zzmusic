#ifndef QQMUSIC_AUDIOENGINE_H
#define QQMUSIC_AUDIOENGINE_H

#include "core/types.h"

#include <QObject>
#include <QUrl>

class QMediaPlayer;

class AudioEngine : public QObject
{
    Q_OBJECT

public:
    explicit AudioEngine(QObject *parent = 0, bool createBackend = true);
    ~AudioEngine();

public slots:
    virtual void load(const QUrl &url);
    virtual void play();
    virtual void pause();
    virtual void stop();
    virtual void seek(qint64 positionMs);
    virtual void notifyInterruptionBegan();
    virtual void notifyInterruptionEnded();
    virtual void notifyHeadsetRemoved();

signals:
    void loaded();
    void playing();
    void paused();
    void stopped();
    void stalled();
    void ended();
    void failed(ErrorKind kind);
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void mediaInfoChanged(int sampleRateHz, int bitrateBps,
                          const QString &codec);
    void interruptionBegan();
    void interruptionEnded();
    void headsetRemoved();

private slots:
    void playerStateChanged(int state);
    void mediaStatusChanged(int status);
    void playerError(int error);
    void updateMediaInfo();

private:
    QMediaPlayer *m_player;
};

#endif
