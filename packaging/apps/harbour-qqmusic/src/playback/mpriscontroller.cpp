#include "mpriscontroller.h"

#include "playbackcontroller.h"

#include <QCryptographicHash>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>

namespace {

const char PlayerInterface[] = "org.mpris.MediaPlayer2.Player";

class MprisRootAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)

public:
    explicit MprisRootAdaptor(MprisController *controller)
        : QDBusAbstractAdaptor(controller), m_controller(controller) {}
    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return QStringLiteral("ZZ Music"); }
    QString desktopEntry() const { return QStringLiteral("harbour-qqmusic"); }
    QStringList supportedUriSchemes() const { return QStringList(); }
    QStringList supportedMimeTypes() const { return QStringList(); }

public slots:
    void Raise() { emit m_controller->raiseRequested(); }
    void Quit() { emit m_controller->quitRequested(); }

private:
    MprisController *m_controller;
};

class MprisPlayerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QString LoopStatus READ loopStatus)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle)
    Q_PROPERTY(double Rate READ rate)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ minimumRate)
    Q_PROPERTY(double MaximumRate READ maximumRate)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPause)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl)

public:
    explicit MprisPlayerAdaptor(MprisController *controller)
        : QDBusAbstractAdaptor(controller), m_controller(controller) {
        connect(controller, &MprisController::seeked,
                this, &MprisPlayerAdaptor::Seeked);
    }
    QString playbackStatus() const { return m_controller->playbackStatus(); }
    QString loopStatus() const { return m_controller->loopStatus(); }
    bool shuffle() const { return m_controller->shuffle(); }
    double rate() const { return 1.0; }
    QVariantMap metadata() const { return m_controller->metadata(); }
    double volume() const { return 1.0; }
    qlonglong position() const { return m_controller->position(); }
    double minimumRate() const { return 1.0; }
    double maximumRate() const { return 1.0; }
    bool canGoNext() const { return m_controller->canGoNext(); }
    bool canGoPrevious() const { return m_controller->canGoPrevious(); }
    bool canPlay() const { return m_controller->canPlay(); }
    bool canPause() const { return m_controller->canPause(); }
    bool canSeek() const { return m_controller->canSeek(); }
    bool canControl() const { return true; }

public slots:
    void Next() { m_controller->next(); }
    void Previous() { m_controller->previous(); }
    void Pause() { m_controller->pause(); }
    void PlayPause() { m_controller->playPause(); }
    void Stop() { m_controller->stop(); }
    void Play() { m_controller->play(); }
    void setShuffle(bool enabled) { m_controller->setShuffle(enabled); }
    void Seek(qlonglong offset) { m_controller->seek(offset); }
    void SetPosition(const QDBusObjectPath &trackId, qlonglong position) {
        m_controller->setPosition(trackId, position);
    }
    void OpenUri(const QString &) {}

signals:
    void Seeked(qlonglong Position);

private:
    MprisController *m_controller;
};

}

MprisController::MprisController(PlaybackController *playback,
                                 bool registerOnSessionBus,
                                 QObject *parent)
    : QObject(parent)
    , m_playback(playback)
    , m_registered(false)
{
    if (m_playback) {
        connect(m_playback, &PlaybackController::stateChanged,
                this, &MprisController::refresh);
        connect(m_playback, &PlaybackController::currentChanged,
                this, &MprisController::refresh);
        connect(m_playback, &PlaybackController::queueChanged,
                this, &MprisController::refresh);
        connect(m_playback, &PlaybackController::repeatModeChanged,
                this, &MprisController::refresh);
        connect(m_playback, &PlaybackController::durationChanged,
                this, &MprisController::refresh);
        connect(m_playback, &PlaybackController::seeked,
                this, &MprisController::playbackSeeked);
    }
    new MprisRootAdaptor(this);
    new MprisPlayerAdaptor(this);
    if (registerOnSessionBus) {
        QDBusConnection bus = QDBusConnection::sessionBus();
        m_registered = bus.registerService(serviceName())
            && bus.registerObject(objectPath(), this,
                                  QDBusConnection::ExportAdaptors);
        if (!m_registered) {
            bus.unregisterObject(objectPath());
            bus.unregisterService(serviceName());
        }
    }
    refresh();
}

MprisController::~MprisController()
{
    if (m_registered) {
        QDBusConnection::sessionBus().unregisterObject(objectPath());
        QDBusConnection::sessionBus().unregisterService(serviceName());
    }
}

QString MprisController::serviceName()
{
    return QStringLiteral("org.mpris.MediaPlayer2.harbour_qqmusic");
}

QString MprisController::objectPath()
{
    return QStringLiteral("/org/mpris/MediaPlayer2");
}

QVariantMap MprisController::metadata() const
{
    QVariantMap result;
    if (!m_playback || m_playback->currentIndex() < 0) {
        return result;
    }
    const Track track = m_playback->currentTrack();
    result.insert(QStringLiteral("mpris:trackid"),
                  QVariant::fromValue(currentTrackId()));
    result.insert(QStringLiteral("mpris:length"),
                  qlonglong(qMax<qint64>(0, track.durationMs) * 1000));
    if (!track.artworkUrl.isEmpty()) {
        result.insert(QStringLiteral("mpris:artUrl"),
                      track.artworkUrl.toString());
    }
    result.insert(QStringLiteral("xesam:title"), track.title);
    result.insert(QStringLiteral("xesam:artist"), QStringList() << track.artist);
    result.insert(QStringLiteral("xesam:album"), track.album);
    return result;
}

QString MprisController::playbackStatus() const
{
    if (!m_playback || m_playback->currentIndex() < 0) {
        return QStringLiteral("Stopped");
    }
    const int state = m_playback->state();
    if (state == PlaybackController::Playing
            || state == PlaybackController::Stalled) {
        return QStringLiteral("Playing");
    }
    if (state == PlaybackController::Paused
            || state == PlaybackController::Loading
            || state == PlaybackController::Resolving) {
        return QStringLiteral("Paused");
    }
    return QStringLiteral("Stopped");
}

QString MprisController::loopStatus() const
{
    if (!m_playback) {
        return QStringLiteral("None");
    }
    if (m_playback->repeatMode() == PlaybackController::RepeatOne) {
        return QStringLiteral("Track");
    }
    return m_playback->repeatMode() == PlaybackController::RepeatAll
        ? QStringLiteral("Playlist") : QStringLiteral("None");
}

bool MprisController::shuffle() const
{
    return m_playback
        && m_playback->repeatMode() == PlaybackController::Shuffle;
}

qlonglong MprisController::position() const
{
    return m_playback ? qlonglong(m_playback->positionMs() * 1000) : 0;
}

bool MprisController::canPlay() const
{
    return m_playback && m_playback->canPlay();
}

bool MprisController::canPause() const
{
    return m_playback && m_playback->canPause();
}

bool MprisController::canGoNext() const
{
    return m_playback && m_playback->canGoNext();
}

bool MprisController::canGoPrevious() const
{
    return m_playback && m_playback->canGoPrevious();
}

bool MprisController::canSeek() const
{
    return m_playback && m_playback->currentIndex() >= 0
        && m_playback->currentTrack().durationMs > 0;
}

bool MprisController::registered() const
{
    return m_registered;
}

QDBusObjectPath MprisController::currentTrackId() const
{
    const QByteArray mid = m_playback
        ? m_playback->currentTrack().mid.toUtf8() : QByteArray();
    const QString digest = QString::fromLatin1(
        QCryptographicHash::hash(mid, QCryptographicHash::Sha1).toHex());
    return QDBusObjectPath(QStringLiteral("/org/bonito/qqmusic/track_")
                           + digest);
}

void MprisController::next()
{
    if (m_playback) {
        m_playback->next();
    }
}

void MprisController::previous()
{
    if (m_playback) {
        m_playback->previous();
    }
}

void MprisController::pause()
{
    if (m_playback) {
        m_playback->pause();
    }
}

void MprisController::playPause()
{
    if (!m_playback) {
        return;
    }
    if (m_playback->canPause()) {
        m_playback->pause();
    } else {
        m_playback->play();
    }
}

void MprisController::stop()
{
    if (m_playback) {
        m_playback->stop();
    }
}

void MprisController::play()
{
    if (m_playback) {
        m_playback->play();
    }
}

void MprisController::setShuffle(bool enabled)
{
    if (!m_playback) {
        return;
    }
    if (enabled) {
        m_playback->setRepeatMode(PlaybackController::Shuffle);
    } else if (m_playback->repeatMode() == PlaybackController::Shuffle) {
        m_playback->setRepeatMode(PlaybackController::RepeatOff);
    }
}

void MprisController::seek(qlonglong offsetMicroseconds)
{
    if (canSeek()) {
        const qlonglong target = position() + offsetMicroseconds;
        m_playback->seek(qMax<qlonglong>(0, target) / 1000);
    }
}

void MprisController::setPosition(const QDBusObjectPath &trackId,
                                  qlonglong positionMicroseconds)
{
    if (canSeek() && trackId.path() == currentTrackId().path()) {
        m_playback->seek(qMax<qlonglong>(0, positionMicroseconds) / 1000);
    }
}

void MprisController::refresh()
{
    QVariantMap changed;
    const QVariantMap nextMetadata = metadata();
    const QString nextStatus = playbackStatus();
    const QString nextLoop = loopStatus();
    const bool nextShuffle = shuffle();
    QVariantMap capabilities;
    capabilities.insert(QStringLiteral("CanPlay"), canPlay());
    capabilities.insert(QStringLiteral("CanPause"), canPause());
    capabilities.insert(QStringLiteral("CanGoNext"), canGoNext());
    capabilities.insert(QStringLiteral("CanGoPrevious"), canGoPrevious());
    capabilities.insert(QStringLiteral("CanSeek"), canSeek());
    capabilities.insert(QStringLiteral("CanControl"), true);
    if (m_lastMetadata != nextMetadata) {
        m_lastMetadata = nextMetadata;
        changed.insert(QStringLiteral("Metadata"), nextMetadata);
    }
    if (m_lastPlaybackStatus != nextStatus) {
        m_lastPlaybackStatus = nextStatus;
        changed.insert(QStringLiteral("PlaybackStatus"), nextStatus);
    }
    if (m_lastLoopStatus != nextLoop) {
        m_lastLoopStatus = nextLoop;
        changed.insert(QStringLiteral("LoopStatus"), nextLoop);
    }
    if (m_lastShuffle != nextShuffle) {
        m_lastShuffle = nextShuffle;
        changed.insert(QStringLiteral("Shuffle"), nextShuffle);
    }
    for (auto it = capabilities.constBegin(); it != capabilities.constEnd(); ++it) {
        if (m_lastCapabilities.value(it.key()) != it.value()) {
            changed.insert(it.key(), it.value());
        }
    }
    m_lastCapabilities = capabilities;
    if (!changed.isEmpty()) {
        publish(QString::fromLatin1(PlayerInterface), changed);
    }
}

void MprisController::playbackSeeked(qint64 positionMs)
{
    emit seeked(qlonglong(qMax<qint64>(0, positionMs)) * 1000);
}

void MprisController::publish(const QString &interfaceName,
                              const QVariantMap &changed)
{
    emit propertiesPublished(interfaceName, changed);
    if (!m_registered) {
        return;
    }
    QDBusMessage message = QDBusMessage::createSignal(
        objectPath(), QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    message << interfaceName << changed << QStringList();
    QDBusConnection::sessionBus().send(message);
}

#include "mpriscontroller.moc"
