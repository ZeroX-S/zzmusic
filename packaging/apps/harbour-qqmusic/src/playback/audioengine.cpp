#include "audioengine.h"

#include <QMediaContent>
#include <QMediaMetaData>
#include <QMediaPlayer>

AudioEngine::AudioEngine(QObject *parent, bool createBackend)
    : QObject(parent)
    , m_player(createBackend ? new QMediaPlayer(this) : 0)
{
    if (!m_player) {
        return;
    }
    connect(m_player, &QMediaPlayer::stateChanged,
            this, [this](QMediaPlayer::State state) {
        playerStateChanged(int(state));
    });
    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, [this](QMediaPlayer::MediaStatus status) {
        mediaStatusChanged(int(status));
    });
    connect(m_player,
            static_cast<void (QMediaPlayer::*)(QMediaPlayer::Error)>(
                &QMediaPlayer::error),
            this, [this](QMediaPlayer::Error error) {
        playerError(int(error));
    });
    connect(m_player, &QMediaPlayer::positionChanged,
            this, &AudioEngine::positionChanged);
    connect(m_player, &QMediaPlayer::durationChanged,
            this, &AudioEngine::durationChanged);
    connect(m_player, SIGNAL(metaDataChanged()),
            this, SLOT(updateMediaInfo()));
}

AudioEngine::~AudioEngine()
{
}

void AudioEngine::load(const QUrl &url)
{
    if (!m_player || !url.isValid() || url.scheme() != QStringLiteral("https")) {
        emit failed(ErrorKind::NotPlayable);
        return;
    }
    m_player->setMedia(QMediaContent(url));
}

void AudioEngine::play()
{
    if (m_player) {
        m_player->play();
    }
}

void AudioEngine::pause()
{
    if (m_player) {
        m_player->pause();
    }
}

void AudioEngine::stop()
{
    if (m_player) {
        m_player->stop();
    }
}

void AudioEngine::seek(qint64 positionMs)
{
    if (m_player) {
        m_player->setPosition(qMax<qint64>(0, positionMs));
    }
}

void AudioEngine::notifyInterruptionBegan()
{
    emit interruptionBegan();
}

void AudioEngine::notifyInterruptionEnded()
{
    emit interruptionEnded();
}

void AudioEngine::notifyHeadsetRemoved()
{
    emit headsetRemoved();
}

void AudioEngine::playerStateChanged(int state)
{
    switch (QMediaPlayer::State(state)) {
    case QMediaPlayer::PlayingState:
        emit playing();
        break;
    case QMediaPlayer::PausedState:
        emit paused();
        break;
    case QMediaPlayer::StoppedState:
        emit stopped();
        break;
    }
}

void AudioEngine::mediaStatusChanged(int status)
{
    switch (QMediaPlayer::MediaStatus(status)) {
    case QMediaPlayer::LoadedMedia:
    case QMediaPlayer::BufferedMedia:
        emit loaded();
        break;
    case QMediaPlayer::StalledMedia:
    case QMediaPlayer::BufferingMedia:
        emit stalled();
        break;
    case QMediaPlayer::EndOfMedia:
        emit ended();
        break;
    case QMediaPlayer::InvalidMedia:
        emit failed(ErrorKind::DecodeFailed);
        break;
    default:
        break;
    }
}

void AudioEngine::playerError(int error)
{
    const QMediaPlayer::Error value = QMediaPlayer::Error(error);
    emit failed(value == QMediaPlayer::NetworkError
                    ? ErrorKind::Offline
                    : ErrorKind::DecodeFailed);
}

void AudioEngine::updateMediaInfo()
{
    if (!m_player) {
        return;
    }
    const int sampleRateHz = m_player->metaData(
        QMediaMetaData::SampleRate).toInt();
    const int bitrateBps = m_player->metaData(
        QMediaMetaData::AudioBitRate).toInt();
    const QString codec = m_player->metaData(
        QMediaMetaData::AudioCodec).toString();
    if (sampleRateHz > 0 || bitrateBps > 0 || !codec.isEmpty()) {
        emit mediaInfoChanged(sampleRateHz, bitrateBps, codec);
    }
}
