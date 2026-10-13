#include "playbackcontroller.h"

#include "audioengine.h"
#include "core/qqidentifier.h"
#include "service/qqmusicservice.h"

#include <QDateTime>
#include <QDebug>

namespace {

const qint64 PauseResumePositionToleranceMs = 1500;

QString qualityCode(const QString &bitrate)
{
    if (bitrate == QLatin1String("FLAC lossless")) {
        return QStringLiteral("lossless_flac");
    }
    if (bitrate == QLatin1String("320 kbps")) {
        return QStringLiteral("high_320k");
    }
    if (bitrate == QLatin1String("128 kbps")) {
        return QStringLiteral("standard_128k");
    }
    return QStringLiteral("unknown");
}

QString audioFormatForQuality(const QString &quality)
{
    if (quality == QLatin1String("FLAC lossless")) {
        return QStringLiteral("FLAC");
    }
    if (quality == QLatin1String("320 kbps")
            || quality == QLatin1String("128 kbps")) {
        return QStringLiteral("MP3");
    }
    return QString();
}

int nominalBitrateForQuality(const QString &quality)
{
    if (quality == QLatin1String("320 kbps")) {
        return 320;
    }
    if (quality == QLatin1String("128 kbps")) {
        return 128;
    }
    return 0;
}

QString normalizedAudioFormat(const QString &codec)
{
    const QString value = codec.trimmed().toLower();
    if (value.contains(QStringLiteral("flac"))) {
        return QStringLiteral("FLAC");
    }
    if (value.contains(QStringLiteral("mp3"))
            || value.contains(QStringLiteral("mpeg"))) {
        return QStringLiteral("MP3");
    }
    if (value.contains(QStringLiteral("alac"))) {
        return QStringLiteral("ALAC");
    }
    if (value.contains(QStringLiteral("aac"))) {
        return QStringLiteral("AAC");
    }
    if (value.contains(QStringLiteral("opus"))) {
        return QStringLiteral("Opus");
    }
    if (value.contains(QStringLiteral("vorbis"))
            || value.contains(QStringLiteral("ogg"))) {
        return QStringLiteral("Ogg Vorbis");
    }
    return QString();
}

}

PlaybackController::PlaybackController(QqMusicService *service,
                                       AudioEngine *engine,
                                       QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_engine(engine)
    , m_currentIndex(-1)
    , m_state(Idle)
    , m_repeatMode(RepeatOff)
    , m_shuffleRandomState(0)
    , m_nextOperationId(quint64(1) << 63)
    , m_resolveOperationId(0)
    , m_positionMs(0)
    , m_durationMs(0)
    , m_audioSampleRateHz(0)
    , m_audioBitrateKbps(0)
    , m_audioLossless(false)
    , m_pendingSeekMs(0)
    , m_consecutiveSkips(0)
    , m_autoplay(true)
    , m_refreshedForCurrent(false)
    , m_resolveAccountBound(false)
    , m_progressEvidenceLogged(false)
    , m_userPaused(false)
    , m_resumeAfterInterruption(false)
    , m_pausePositionGuard(false)
    , m_pauseBackendResetLogged(false)
    , m_pauseBackendNeedsReload(false)
    , m_pausedPositionMs(0)
    , m_pausedDurationMs(0)
{
    m_shuffleRandomState = quint32(QDateTime::currentMSecsSinceEpoch())
        ^ quint32(quintptr(this));
    if (m_shuffleRandomState == 0) {
        m_shuffleRandomState = 0x9e3779b9U;
    }
    if (m_service) {
        connect(m_service, &QqMusicService::playbackReady,
                this, &PlaybackController::playbackReady);
        connect(m_service, &QqMusicService::failed,
                this, &PlaybackController::serviceFailed);
    }
    if (m_engine) {
        connect(m_engine, &AudioEngine::loaded,
                this, &PlaybackController::engineLoaded);
        connect(m_engine, &AudioEngine::playing,
                this, &PlaybackController::enginePlaying);
        connect(m_engine, &AudioEngine::paused,
                this, &PlaybackController::enginePaused);
        connect(m_engine, &AudioEngine::stopped,
                this, &PlaybackController::engineStopped);
        connect(m_engine, &AudioEngine::stalled,
                this, &PlaybackController::engineStalled);
        connect(m_engine, &AudioEngine::ended,
                this, &PlaybackController::engineEnded);
        connect(m_engine, &AudioEngine::failed,
                this, &PlaybackController::engineFailed);
        connect(m_engine, &AudioEngine::positionChanged,
                this, &PlaybackController::updatePosition);
        connect(m_engine, &AudioEngine::durationChanged,
                this, &PlaybackController::updateDuration);
        connect(m_engine, &AudioEngine::mediaInfoChanged,
                this, &PlaybackController::updateMediaInfo);
        connect(m_engine, &AudioEngine::interruptionBegan,
                this, &PlaybackController::interruptionBegan);
        connect(m_engine, &AudioEngine::interruptionEnded,
                this, &PlaybackController::interruptionEnded);
        connect(m_engine, &AudioEngine::headsetRemoved,
                this, &PlaybackController::headsetRemoved);
    }
}

int PlaybackController::state() const
{
    return int(m_state);
}

int PlaybackController::repeatMode() const
{
    return int(m_repeatMode);
}

void PlaybackController::setRepeatMode(int repeatMode)
{
    if (repeatMode < int(RepeatOff) || repeatMode > int(Shuffle)
            || m_repeatMode == Repeat(repeatMode)) {
        return;
    }
    m_repeatMode = Repeat(repeatMode);
    resetShuffleState();
    emit repeatModeChanged();
    emit capabilitiesChanged();
}

QVariantList PlaybackController::queue() const
{
    QVariantList result;
    for (const Track &track : m_queue) {
        result.append(trackValue(track));
    }
    return result;
}

QVector<Track> PlaybackController::queueTracks() const
{
    return m_queue;
}

int PlaybackController::currentIndex() const
{
    return m_currentIndex;
}

Track PlaybackController::currentTrack() const
{
    return m_currentIndex >= 0 && m_currentIndex < m_queue.size()
        ? m_queue.at(m_currentIndex) : Track();
}

QVariantMap PlaybackController::currentTrackValue() const
{
    return trackValue(currentTrack());
}

qint64 PlaybackController::positionMs() const
{
    return m_positionMs;
}

qint64 PlaybackController::durationMs() const
{
    return m_durationMs;
}

int PlaybackController::audioSampleRateHz() const
{
    return m_audioSampleRateHz;
}

int PlaybackController::audioBitrateKbps() const
{
    return m_audioBitrateKbps;
}

QString PlaybackController::audioFormat() const
{
    return m_audioFormat;
}

bool PlaybackController::audioLossless() const
{
    return m_audioLossless;
}

int PlaybackController::consecutiveSkips() const
{
    return m_consecutiveSkips;
}

bool PlaybackController::canPlay() const
{
    return !m_queue.isEmpty() && m_state != Resolving && m_state != Loading;
}

bool PlaybackController::isPlaying() const
{
    return m_state == Playing;
}

bool PlaybackController::canPause() const
{
    return m_state == Playing || m_state == Stalled;
}

bool PlaybackController::canGoNext() const
{
    if (m_repeatMode == Shuffle) {
        return m_queue.size() > 1;
    }
    return !m_queue.isEmpty()
        && (m_currentIndex + 1 < m_queue.size()
            || m_repeatMode == RepeatAll);
}

bool PlaybackController::canGoPrevious() const
{
    if (m_repeatMode == Shuffle) {
        return !m_shuffleHistory.isEmpty();
    }
    return !m_queue.isEmpty()
        && (m_currentIndex > 0 || m_repeatMode == RepeatAll);
}

bool PlaybackController::currentLocationAccountBound() const
{
    return m_location.accountBound;
}

void PlaybackController::replaceQueue(const QVector<Track> &tracks,
                                      int startIndex,
                                      bool autoplay)
{
    const int previousIndex = m_currentIndex;
    const QVariantMap previousTrack = currentTrackValue();
    clearPausePositionGuard();
    if (m_engine) {
        m_engine->stop();
    }
    m_queue = tracks;
    m_autoplay = autoplay;
    m_consecutiveSkips = 0;
    m_pendingSeekMs = 0;
    m_refreshedForCurrent = false;
    m_progressEvidenceLogged = false;
    m_location = PlaybackLocation();
    resetAudioDetails();
    m_positionMs = 0;
    m_durationMs = 0;
    emit queueChanged();
    emit positionChanged();
    emit durationChanged();
    if (m_queue.isEmpty()) {
        resetShuffleState();
        setCurrentIndex(-1);
        setState(Idle);
        return;
    }
    setCurrentIndex(qBound(0, startIndex, m_queue.size() - 1));
    resetShuffleState();
    if (previousIndex == m_currentIndex
            && previousTrack != currentTrackValue()) {
        emit currentChanged();
    }
    resolveCurrent();
}

void PlaybackController::restoreQueue(const QVector<Track> &tracks,
                                      const QString &currentMid,
                                      int repeatMode,
                                      qint64 positionMs)
{
    int index = 0;
    for (int candidate = 0; candidate < tracks.size(); ++candidate) {
        if (tracks.at(candidate).mid == currentMid) {
            index = candidate;
            break;
        }
    }
    setRepeatMode(repeatMode);
    replaceQueue(tracks, index, false);
    m_pendingSeekMs = qMax<qint64>(0, positionMs);
}

void PlaybackController::addNext(const Track &track)
{
    if (track.mid.isEmpty()) {
        return;
    }
    const int insertAt = m_currentIndex < 0
        ? 0 : qMin(m_currentIndex + 1, m_queue.size());
    m_queue.insert(insertAt, track);
    if (m_currentIndex < 0) {
        setCurrentIndex(0);
    }
    resetShuffleState();
    emit queueChanged();
    emit capabilitiesChanged();
}

void PlaybackController::append(const Track &track)
{
    if (track.mid.isEmpty()) {
        return;
    }
    m_queue.append(track);
    if (m_currentIndex < 0) {
        setCurrentIndex(0);
    }
    resetShuffleState();
    emit queueChanged();
    emit capabilitiesChanged();
}

void PlaybackController::play()
{
    m_userPaused = false;
    m_resumeAfterInterruption = false;
    m_autoplay = true;
    if (m_queue.isEmpty()) {
        return;
    }
    if (m_currentIndex < 0) {
        setCurrentIndex(0);
    }
    if (m_state == Paused && m_location.url.isValid()
            && m_location.expiresAt > QDateTime::currentDateTimeUtc()) {
        if (m_engine) {
            if (m_pausePositionGuard && m_pauseBackendNeedsReload) {
                m_pendingSeekMs = m_pausedPositionMs;
                m_pauseBackendNeedsReload = false;
                setState(Loading);
                m_engine->load(m_location.url);
                return;
            }
            if (m_pausePositionGuard && m_pausedPositionMs > 0) {
                m_engine->seek(m_pausedPositionMs);
            }
            m_engine->play();
        }
        return;
    }
    if (m_state == Resolving || m_state == Loading) {
        return;
    }
    resolveCurrent();
}

void PlaybackController::pause()
{
    m_userPaused = true;
    m_resumeAfterInterruption = false;
    m_autoplay = false;
    beginPausePositionGuard();
    if (m_engine) {
        m_engine->pause();
    }
    setState(Paused);
}

void PlaybackController::stop()
{
    const quint64 resolveOperationId = m_resolveOperationId;
    m_resolveOperationId = 0;
    m_resolveAccountBound = false;
    if (m_service && resolveOperationId != 0) {
        m_service->cancel(resolveOperationId);
    }
    clearPausePositionGuard();
    if (m_engine) {
        m_engine->stop();
    }
    m_location = PlaybackLocation();
    resetAudioDetails();
    m_positionMs = 0;
    m_pendingSeekMs = 0;
    m_resumeAfterInterruption = false;
    emit positionChanged();
    setState(Idle);
}

void PlaybackController::previous()
{
    if (m_positionMs > 5000 && m_engine) {
        seek(0);
        return;
    }
    advance(true, true);
}

void PlaybackController::next()
{
    advance(true, false);
}

void PlaybackController::retry()
{
    if (m_queue.isEmpty()) {
        return;
    }
    m_consecutiveSkips = 0;
    m_refreshedForCurrent = false;
    m_autoplay = true;
    resolveCurrent();
}

void PlaybackController::seek(qint64 positionMs)
{
    const qint64 bounded = m_durationMs > 0
        ? qBound<qint64>(0, positionMs, m_durationMs)
        : qMax<qint64>(0, positionMs);
    if (m_pausePositionGuard) {
        m_pausedPositionMs = bounded;
    }
    if (m_engine) {
        m_engine->seek(bounded);
    }
    updatePosition(bounded);
    if (m_currentIndex >= 0 && m_currentIndex < m_queue.size()) {
        emit seeked(bounded);
    }
}

void PlaybackController::remove(int index)
{
    if (index < 0 || index >= m_queue.size()) {
        return;
    }
    const bool removingCurrent = index == m_currentIndex;
    m_queue.remove(index);
    if (m_queue.isEmpty()) {
        clear();
        return;
    }
    if (index < m_currentIndex) {
        setCurrentIndex(m_currentIndex - 1);
    } else if (removingCurrent) {
        setCurrentIndex(qMin(index, m_queue.size() - 1));
        m_refreshedForCurrent = false;
        resolveCurrent();
    }
    resetShuffleState();
    emit queueChanged();
    emit capabilitiesChanged();
}

void PlaybackController::clear()
{
    stop();
    m_queue.clear();
    resetShuffleState();
    setCurrentIndex(-1);
    emit queueChanged();
    emit capabilitiesChanged();
}

void PlaybackController::setAccountSessionUin(const QString &uin)
{
    const QString validated = QqIdentifier::isNumericPlaylistId(uin)
        ? uin : QString();
    if (m_accountUin == validated) {
        return;
    }
    const bool becameAvailable = m_accountUin.isEmpty()
        && !validated.isEmpty();
    m_accountUin = validated;
    if (becameAvailable && !m_queue.isEmpty()
            && (m_state == Failed
                || (m_state == Resolving && !m_resolveAccountBound))) {
        m_consecutiveSkips = 0;
        m_refreshedForCurrent = false;
        m_autoplay = true;
        resolveCurrent();
    }
}

void PlaybackController::sessionEnded()
{
    const bool stopAccountPlayback = m_location.accountBound
        || (m_resolveOperationId != 0 && m_resolveAccountBound);
    m_accountUin.clear();
    if (stopAccountPlayback) {
        stop();
    }
}

void PlaybackController::playbackReady(
    quint64 operationId, const PlaybackLocation &location)
{
    if (operationId != m_resolveOperationId || m_currentIndex < 0) {
        return;
    }
    m_resolveOperationId = 0;
    m_resolveAccountBound = false;
    if (!location.url.isValid()
            || location.url.scheme() != QStringLiteral("https")
            || !location.expiresAt.isValid()
            || location.expiresAt <= QDateTime::currentDateTimeUtc()) {
        ServiceError error;
        error.kind = ErrorKind::NotPlayable;
        error.diagnosticCode = QStringLiteral("qq.playback.location.invalid");
        handleUnavailable(error);
        return;
    }
    m_location = location;
    setAudioDetails(0, nominalBitrateForQuality(location.bitrate),
                    audioFormatForQuality(location.bitrate),
                    location.bitrate == QLatin1String("FLAC lossless"));
    m_progressEvidenceLogged = false;
    qInfo().noquote().nospace()
        << "qqmusic-playback authorization=ready quality="
        << qualityCode(location.bitrate)
        << " account_bound=" << int(location.accountBound);
    setState(Loading);
    if (m_engine) {
        m_engine->load(location.url);
    } else {
        ServiceError error;
        error.kind = ErrorKind::Internal;
        error.diagnosticCode = QStringLiteral("qq.playback.engine.missing");
        fail(error);
    }
}

void PlaybackController::serviceFailed(const ServiceError &error)
{
    if (error.operationId != m_resolveOperationId) {
        return;
    }
    m_resolveOperationId = 0;
    m_resolveAccountBound = false;
    if (error.kind == ErrorKind::NotPlayable) {
        handleUnavailable(error);
    } else {
        fail(error);
    }
}

void PlaybackController::engineLoaded()
{
    if (m_pendingSeekMs > 0 && m_engine) {
        const qint64 restoredPositionMs = m_pendingSeekMs;
        m_engine->seek(restoredPositionMs);
        updatePosition(restoredPositionMs);
        m_pendingSeekMs = 0;
        emit seeked(restoredPositionMs);
    }
    if (m_autoplay && m_engine) {
        m_engine->play();
    } else {
        setState(Paused);
    }
}

void PlaybackController::enginePlaying()
{
    m_consecutiveSkips = 0;
    m_userPaused = false;
    setState(Playing);
    if (m_pausePositionGuard) {
        const qint64 resumePositionMs = m_pausedPositionMs;
        if (resumePositionMs > 0 && m_engine) {
            m_engine->seek(resumePositionMs);
        } else {
            clearPausePositionGuard();
        }
    }
    qInfo().noquote().nospace()
        << "qqmusic-playback engine=playing quality="
        << qualityCode(m_location.bitrate)
        << " account_bound=" << int(m_location.accountBound);
}

void PlaybackController::enginePaused()
{
    if (m_pausePositionGuard && !m_userPaused && m_state == Playing) {
        if (!m_pauseBackendResetLogged) {
            m_pauseBackendResetLogged = true;
            qInfo().noquote()
                << "zzmusic_playback component=controller"
                << "event=pause_backend_reset_ignored signal=paused";
        }
        return;
    }
    setState(Paused);
}

void PlaybackController::engineStopped()
{
    if (m_pausePositionGuard) {
        if (m_userPaused || !m_autoplay || m_state == Paused) {
            m_pauseBackendNeedsReload = true;
        }
        if (!m_pauseBackendResetLogged) {
            m_pauseBackendResetLogged = true;
            qInfo().noquote()
                << "zzmusic_playback component=controller"
                << "event=pause_backend_reset_ignored signal=stopped";
        }
        if (m_state != Playing) {
            setState(Paused);
        }
        return;
    }
    if (m_state != Resolving && m_state != Loading && m_state != Failed) {
        setState(Idle);
    }
}

void PlaybackController::engineStalled()
{
    if (m_state == Playing || m_state == Loading) {
        setState(Stalled);
    }
}

void PlaybackController::engineEnded()
{
    if (m_repeatMode == RepeatOne) {
        m_refreshedForCurrent = false;
        resolveCurrent();
    } else {
        advance(false, false);
    }
}

void PlaybackController::engineFailed(ErrorKind kind)
{
    if ((kind == ErrorKind::DecodeFailed || kind == ErrorKind::Offline)
            && !m_refreshedForCurrent) {
        m_refreshedForCurrent = true;
        resolveCurrent();
        return;
    }
    ServiceError error;
    error.kind = kind == ErrorKind::None ? ErrorKind::DecodeFailed : kind;
    error.diagnosticCode = QStringLiteral("qq.playback.engine.failed");
    handleUnavailable(error);
}

void PlaybackController::interruptionBegan()
{
    if (m_state == Playing || m_state == Loading || m_state == Resolving) {
        m_resumeAfterInterruption = !m_userPaused;
        m_autoplay = false;
        beginPausePositionGuard();
        if (m_engine) {
            m_engine->pause();
        }
        setState(Paused);
    }
}

void PlaybackController::interruptionEnded()
{
    if (m_resumeAfterInterruption && !m_userPaused) {
        m_resumeAfterInterruption = false;
        play();
    }
}

void PlaybackController::headsetRemoved()
{
    interruptionBegan();
    m_resumeAfterInterruption = false;
}

void PlaybackController::updatePosition(qint64 positionMs)
{
    const qint64 bounded = qMax<qint64>(0, positionMs);
    const qint64 resumeFloor = qMax<qint64>(
        0, m_pausedPositionMs - PauseResumePositionToleranceMs);
    const bool pausedResetNoise = m_pausePositionGuard
        && m_pausedPositionMs > 0
        && ((m_state == Playing && bounded < resumeFloor)
            || (m_state != Playing && bounded != m_pausedPositionMs));
    if (pausedResetNoise) {
        if (!m_pauseBackendResetLogged) {
            m_pauseBackendResetLogged = true;
            qInfo().noquote()
                << "zzmusic_playback component=controller"
                << "event=pause_backend_reset_ignored signal=position";
        }
        return;
    }
    if (m_pausePositionGuard && m_state == Playing
            && (m_pausedPositionMs == 0 || bounded >= resumeFloor)) {
        clearPausePositionGuard();
    }
    if (m_positionMs != bounded) {
        m_positionMs = bounded;
        emit positionChanged();
    }
    if (!m_progressEvidenceLogged && m_state == Playing
            && m_positionMs >= 3000 && m_location.url.isValid()) {
        m_progressEvidenceLogged = true;
        qInfo().noquote().nospace()
            << "qqmusic-playback progress=ready quality="
            << qualityCode(m_location.bitrate)
            << " account_bound=" << int(m_location.accountBound)
            << " position_gt_3s=1";
    }
}

void PlaybackController::updateDuration(qint64 durationMs)
{
    const qint64 bounded = qMax<qint64>(0, durationMs);
    if (m_pausePositionGuard && bounded == 0 && m_pausedDurationMs > 0) {
        if (m_userPaused || !m_autoplay || m_state == Paused) {
            m_pauseBackendNeedsReload = true;
        }
        if (!m_pauseBackendResetLogged) {
            m_pauseBackendResetLogged = true;
            qInfo().noquote()
                << "zzmusic_playback component=controller"
                << "event=pause_backend_reset_ignored signal=duration";
        }
        return;
    }
    if (m_durationMs != bounded) {
        m_durationMs = bounded;
        emit durationChanged();
    }
}

void PlaybackController::updateMediaInfo(int sampleRateHz, int bitrateBps,
                                         const QString &codec)
{
    const int safeSampleRate = sampleRateHz >= 8000 && sampleRateHz <= 768000
        ? sampleRateHz : 0;
    const int safeBitrateBps = bitrateBps >= 8000
            && bitrateBps <= 100000000
        ? bitrateBps : 0;
    const QString detectedFormat = normalizedAudioFormat(codec);
    if (safeSampleRate == 0 && safeBitrateBps == 0
            && detectedFormat.isEmpty()) {
        return;
    }
    const QString format = detectedFormat.isEmpty()
        ? m_audioFormat : detectedFormat;
    const bool lossless = detectedFormat.isEmpty()
        ? m_audioLossless
        : detectedFormat == QLatin1String("FLAC")
            || detectedFormat == QLatin1String("ALAC");
    setAudioDetails(safeSampleRate > 0
                        ? safeSampleRate : m_audioSampleRateHz,
                    safeBitrateBps > 0
                        ? (safeBitrateBps + 500) / 1000
                        : m_audioBitrateKbps,
                    format, lossless);
}

quint64 PlaybackController::nextOperationId()
{
    ++m_nextOperationId;
    if (m_nextOperationId == 0) {
        m_nextOperationId = (quint64(1) << 63) + 1;
    }
    return m_nextOperationId;
}

void PlaybackController::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged();
        emit capabilitiesChanged();
    }
}

void PlaybackController::resolveCurrent()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_queue.size()) {
        setState(Idle);
        return;
    }
    const quint64 previousOperationId = m_resolveOperationId;
    m_resolveOperationId = 0;
    m_resolveAccountBound = false;
    if (m_service && previousOperationId != 0) {
        m_service->cancel(previousOperationId);
    }
    clearPausePositionGuard();
    m_location = PlaybackLocation();
    resetAudioDetails();
    m_progressEvidenceLogged = false;
    m_positionMs = 0;
    emit positionChanged();
    m_resolveOperationId = nextOperationId();
    m_resolveAccountBound = !m_accountUin.isEmpty();
    setState(Resolving);
    if (!m_service || !m_service->resolvePlayback(
            m_resolveOperationId,
            m_queue.at(m_currentIndex).mid,
            m_accountUin)) {
        m_resolveOperationId = 0;
        m_resolveAccountBound = false;
        ServiceError error;
        error.kind = ErrorKind::Internal;
        error.diagnosticCode = QStringLiteral("qq.playback.resolve.start");
        fail(error);
    }
}

void PlaybackController::setAudioDetails(int sampleRateHz, int bitrateKbps,
                                         const QString &format, bool lossless)
{
    const int boundedSampleRate = qMax(0, sampleRateHz);
    const int boundedBitrate = qMax(0, bitrateKbps);
    if (m_audioSampleRateHz == boundedSampleRate
            && m_audioBitrateKbps == boundedBitrate
            && m_audioFormat == format
            && m_audioLossless == lossless) {
        return;
    }
    m_audioSampleRateHz = boundedSampleRate;
    m_audioBitrateKbps = boundedBitrate;
    m_audioFormat = format;
    m_audioLossless = lossless;
    emit audioDetailsChanged();
}

void PlaybackController::resetAudioDetails()
{
    setAudioDetails(0, 0, QString(), false);
}

void PlaybackController::beginPausePositionGuard()
{
    m_pausePositionGuard = true;
    m_pauseBackendResetLogged = false;
    m_pauseBackendNeedsReload = false;
    m_pausedPositionMs = m_positionMs;
    m_pausedDurationMs = m_durationMs;
}

void PlaybackController::clearPausePositionGuard()
{
    m_pausePositionGuard = false;
    m_pauseBackendResetLogged = false;
    m_pauseBackendNeedsReload = false;
    m_pausedPositionMs = 0;
    m_pausedDurationMs = 0;
}

void PlaybackController::advance(bool manual, bool previousDirection)
{
    if (m_queue.isEmpty()) {
        return;
    }
    if (manual) {
        m_consecutiveSkips = 0;
    }
    int nextIndex = -1;
    if (m_repeatMode == Shuffle) {
        if (previousDirection) {
            if (m_shuffleHistory.isEmpty()) {
                return;
            }
            if (m_currentIndex >= 0
                    && !m_shuffleUpcoming.contains(m_currentIndex)) {
                m_shuffleUpcoming.append(m_currentIndex);
            }
            nextIndex = m_shuffleHistory.takeLast();
        } else {
            if (m_queue.size() < 2) {
                stop();
                return;
            }
            if (m_shuffleUpcoming.isEmpty()) {
                rebuildShuffleUpcoming();
            }
            if (m_shuffleUpcoming.isEmpty()) {
                stop();
                return;
            }
            if (m_currentIndex >= 0) {
                if (m_shuffleHistory.size() >= 256) {
                    m_shuffleHistory.remove(0);
                }
                m_shuffleHistory.append(m_currentIndex);
            }
            nextIndex = m_shuffleUpcoming.takeLast();
        }
    } else {
        nextIndex = m_currentIndex + (previousDirection ? -1 : 1);
    }
    if (nextIndex < 0 || nextIndex >= m_queue.size()) {
        if (m_repeatMode == RepeatAll) {
            nextIndex = previousDirection ? m_queue.size() - 1 : 0;
        } else {
            stop();
            return;
        }
    }
    setCurrentIndex(nextIndex);
    m_refreshedForCurrent = false;
    m_pendingSeekMs = 0;
    m_autoplay = true;
    resolveCurrent();
}

void PlaybackController::resetShuffleState()
{
    m_shuffleHistory.clear();
    rebuildShuffleUpcoming();
}

void PlaybackController::rebuildShuffleUpcoming()
{
    m_shuffleUpcoming.clear();
    if (m_repeatMode != Shuffle || m_queue.size() < 2) {
        return;
    }
    for (int index = 0; index < m_queue.size(); ++index) {
        if (index != m_currentIndex) {
            m_shuffleUpcoming.append(index);
        }
    }
    for (int index = m_shuffleUpcoming.size() - 1; index > 0; --index) {
        const int other = nextShuffleIndex(index + 1);
        qSwap(m_shuffleUpcoming[index], m_shuffleUpcoming[other]);
    }
}

int PlaybackController::nextShuffleIndex(int upperExclusive)
{
    if (upperExclusive <= 1) {
        return 0;
    }
    quint32 value = m_shuffleRandomState;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    m_shuffleRandomState = value;
    return int(value % quint32(upperExclusive));
}

void PlaybackController::handleUnavailable(const ServiceError &error)
{
    if (m_consecutiveSkips >= 3) {
        fail(error);
        return;
    }
    ++m_consecutiveSkips;
    if (m_currentIndex + 1 >= m_queue.size()
            && m_repeatMode != RepeatAll
            && m_repeatMode != Shuffle) {
        fail(error);
        return;
    }
    advance(false, false);
}

void PlaybackController::fail(const ServiceError &error)
{
    m_resolveOperationId = 0;
    m_resolveAccountBound = false;
    clearPausePositionGuard();
    if (m_engine) {
        m_engine->stop();
    }
    setState(Failed);
    emit errorChanged(error);
}

void PlaybackController::setCurrentIndex(int index)
{
    if (m_currentIndex != index) {
        m_currentIndex = index;
        emit currentChanged();
        emit capabilitiesChanged();
    }
}

QVariantMap PlaybackController::trackValue(const Track &track) const
{
    QVariantMap value;
    value.insert(QStringLiteral("mid"), track.mid);
    value.insert(QStringLiteral("title"), track.title);
    value.insert(QStringLiteral("artist"), track.artist);
    value.insert(QStringLiteral("album"), track.album);
    value.insert(QStringLiteral("artworkUrl"), track.artworkUrl);
    value.insert(QStringLiteral("sourceUrl"), track.sourceUrl);
    value.insert(QStringLiteral("durationMs"), track.durationMs);
    value.insert(QStringLiteral("playable"), track.playable);
    return value;
}
