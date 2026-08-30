#ifndef QQMUSIC_MPRISCONTROLLER_H
#define QQMUSIC_MPRISCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QVariantMap>

class PlaybackController;
class QDBusObjectPath;

class MprisController : public QObject
{
    Q_OBJECT

public:
    explicit MprisController(PlaybackController *playback,
                             bool registerOnSessionBus = true,
                             QObject *parent = 0);
    ~MprisController();

    static QString serviceName();
    static QString objectPath();

    QVariantMap metadata() const;
    QString playbackStatus() const;
    QString loopStatus() const;
    bool shuffle() const;
    qlonglong position() const;
    bool canPlay() const;
    bool canPause() const;
    bool canGoNext() const;
    bool canGoPrevious() const;
    bool canSeek() const;
    bool registered() const;
    QDBusObjectPath currentTrackId() const;

public slots:
    void next();
    void previous();
    void pause();
    void playPause();
    void stop();
    void play();
    void setShuffle(bool enabled);
    void seek(qlonglong offsetMicroseconds);
    void setPosition(const QDBusObjectPath &trackId,
                     qlonglong positionMicroseconds);

signals:
    void raiseRequested();
    void quitRequested();
    void propertiesPublished(const QString &interfaceName,
                             const QVariantMap &changed);
    void seeked(qlonglong positionMicroseconds);

private slots:
    void refresh();
    void playbackSeeked(qint64 positionMs);

private:
    void publish(const QString &interfaceName,
                 const QVariantMap &changed);

    QPointer<PlaybackController> m_playback;
    QVariantMap m_lastMetadata;
    QString m_lastPlaybackStatus;
    QString m_lastLoopStatus;
    bool m_lastShuffle = false;
    QVariantMap m_lastCapabilities;
    bool m_registered;
};

#endif
