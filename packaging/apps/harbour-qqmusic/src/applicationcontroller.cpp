#include "applicationcontroller.h"

#include "models/lyriclistmodel.h"
#include "models/playlistlistmodel.h"
#include "models/tracklistmodel.h"
#include "login/nativeloginmanager.h"
#include "playback/playbackcontroller.h"
#include "service/qqmusicservice.h"
#include "session/sessionstore.h"
#include "storage/imagecache.h"
#include "storage/settingsstore.h"

#include <QDebug>
#include <QTimer>
#include <QVariantMap>

namespace {

const int HomeRecommendationWindowSize = 8;
const QString DailyThirtyPlaylistId =
    QStringLiteral("qq-personal-daily30");
const QString GuessYouLikePlaylistId =
    QStringLiteral("qq-personal-guess-you-like");

QVector<PlaylistSummary> recommendationWindow(
    const QVector<PlaylistSummary> &items, quint64 refreshSequence,
    int *selectedOffset)
{
    if (selectedOffset) {
        *selectedOffset = 0;
    }
    if (items.size() < 2 || refreshSequence == 0) {
        return items;
    }

    const int visibleCount = qMin(HomeRecommendationWindowSize, items.size());
    const int stride = qMax(1, visibleCount / 2);
    const quint64 normalized = (refreshSequence - 1)
        % quint64(items.size());
    const int offset = int((normalized * quint64(stride))
                           % quint64(items.size()));
    QVector<PlaylistSummary> visible;
    visible.reserve(visibleCount);
    for (int index = 0; index < visibleCount; ++index) {
        visible.append(items.at((offset + index) % items.size()));
    }
    if (selectedOffset) {
        *selectedOffset = offset;
    }
    return visible;
}

}

void registerApplicationControllerTypes()
{
    qRegisterMetaType<PlaylistListModel *>("PlaylistListModel*");
    qRegisterMetaType<TrackListModel *>("TrackListModel*");
    qRegisterMetaType<LyricListModel *>("LyricListModel*");
    qRegisterMetaType<PlaybackController *>("PlaybackController*");
    qRegisterMetaType<SettingsStore *>("SettingsStore*");
    qRegisterMetaType<ImageCache *>("ImageCache*");
}

ApplicationController::ApplicationController(QObject *parent)
    : ApplicationController(new QqMusicService, parent)
{
    m_service->setParent(this);
}

ApplicationController::ApplicationController(QqMusicService *service,
                                             QObject *parent)
    : QObject(parent)
    , m_service(service ? service : new QqMusicService(this))
    , m_sessionStore(0)
    , m_loginManager(0)
    , m_playbackController(0)
    , m_settingsStore(0)
    , m_imageCache(0)
    , m_persistTimer(new QTimer(this))
    , m_queueRestored(false)
    , m_authenticationRemovalPending(false)
    , m_loginQrGeneration(0)
    , m_loginCancellable(false)
    , m_recommendations(new PlaylistListModel(this))
    , m_publicPlaylists(new PlaylistListModel(this))
    , m_toplists(new PlaylistListModel(this))
    , m_searchTracks(new TrackListModel(this))
    , m_searchPlaylists(new PlaylistListModel(this))
    , m_playlistTracks(new TrackListModel(this))
    , m_likedPlaylists(new PlaylistListModel(this))
    , m_createdPlaylists(new PlaylistListModel(this))
    , m_collectedPlaylists(new PlaylistListModel(this))
    , m_lyrics(new LyricListModel(this))
    , m_homeState(PageState::Empty)
    , m_discoverState(PageState::Empty)
    , m_searchState(PageState::Empty)
    , m_playlistState(PageState::Empty)
    , m_myMusicState(PageState::Empty)
    , m_createdPlaylistsState(PageState::Empty)
    , m_collectedPlaylistsState(PageState::Empty)
    , m_lyricState(PageState::Empty)
    , m_nextOperationId(99)
    , m_homeOperationId(0)
    , m_homeRefreshSequence(0)
    , m_discoverOperationId(0)
    , m_searchOperationId(0)
    , m_playlistOperationId(0)
    , m_myMusicOperationId(0)
    , m_collectedLibraryOperationId(0)
    , m_lyricOperationId(0)
    , m_loggedIn(false)
    , m_searchPage(0)
    , m_searchTotal(0)
    , m_searchAppend(false)
    , m_searchMode(TrackSearch)
    , m_homeStage(HomeStage::None)
    , m_discoverStage(DiscoverStage::None)
{
    m_persistTimer->setSingleShot(true);
    m_persistTimer->setInterval(1500);
    connect(m_persistTimer, &QTimer::timeout,
            this, &ApplicationController::persistQueue);
    connect(m_service, &QqMusicService::recommendationsReady,
            this, &ApplicationController::recommendationsReady);
    connect(m_service, &QqMusicService::dailyThirtyReady,
            this, &ApplicationController::dailyThirtyReady);
    connect(m_service, &QqMusicService::guessYouLikeReady,
            this, &ApplicationController::guessYouLikeReady);
    connect(m_service, &QqMusicService::categoriesReady,
            this, &ApplicationController::categoriesReady);
    connect(m_service, &QqMusicService::playlistsReady,
            this, &ApplicationController::playlistsReady);
    connect(m_service, &QqMusicService::toplistsReady,
            this, &ApplicationController::toplistsReady);
    connect(m_service, &QqMusicService::tracksReady,
            this, &ApplicationController::tracksReady);
    connect(m_service, &QqMusicService::playlistReady,
            this, &ApplicationController::playlistReady);
    connect(m_service, &QqMusicService::createdLibraryReady,
            this, &ApplicationController::createdLibraryReady);
    connect(m_service, &QqMusicService::collectedLibraryReady,
            this, &ApplicationController::collectedLibraryReady);
    connect(m_service, &QqMusicService::lyricsReady,
            this, &ApplicationController::lyricsReady);
    connect(m_service, &QqMusicService::failed,
            this, &ApplicationController::serviceFailed);
}

ApplicationController::~ApplicationController()
{
    if (m_persistTimer->isActive()) {
        m_persistTimer->stop();
        persistQueue();
    }
}

PlaylistListModel *ApplicationController::recommendations() const
{
    return m_recommendations;
}

PlaylistListModel *ApplicationController::publicPlaylists() const
{
    return m_publicPlaylists;
}

PlaylistListModel *ApplicationController::toplists() const
{
    return m_toplists;
}

TrackListModel *ApplicationController::searchTracks() const
{
    return m_searchTracks;
}

PlaylistListModel *ApplicationController::searchPlaylists() const
{
    return m_searchPlaylists;
}

TrackListModel *ApplicationController::playlistTracks() const
{
    return m_playlistTracks;
}

PlaylistListModel *ApplicationController::likedPlaylists() const
{
    return m_likedPlaylists;
}

PlaylistListModel *ApplicationController::createdPlaylists() const
{
    return m_createdPlaylists;
}

PlaylistListModel *ApplicationController::collectedPlaylists() const
{
    return m_collectedPlaylists;
}

LyricListModel *ApplicationController::lyrics() const
{
    return m_lyrics;
}

QVariantList ApplicationController::categoryGroups() const
{
    return m_categoryGroups;
}

QString ApplicationController::selectedCategoryId() const
{
    return m_selectedCategoryId;
}

int ApplicationController::homeState() const
{
    return int(m_homeState);
}

int ApplicationController::discoverState() const
{
    return int(m_discoverState);
}

int ApplicationController::searchState() const
{
    return int(m_searchState);
}

int ApplicationController::playlistState() const
{
    return int(m_playlistState);
}

int ApplicationController::myMusicState() const
{
    return int(m_myMusicState);
}

int ApplicationController::createdPlaylistsState() const
{
    return int(m_createdPlaylistsState);
}

int ApplicationController::collectedPlaylistsState() const
{
    return int(m_collectedPlaylistsState);
}

int ApplicationController::lyricState() const
{
    return int(m_lyricState);
}

quint64 ApplicationController::homeOperationId() const
{
    return m_homeOperationId;
}

quint64 ApplicationController::discoverOperationId() const
{
    return m_discoverOperationId;
}

quint64 ApplicationController::searchOperationId() const
{
    return m_searchOperationId;
}

quint64 ApplicationController::playlistOperationId() const
{
    return m_playlistOperationId;
}

quint64 ApplicationController::myMusicOperationId() const
{
    return m_myMusicOperationId;
}

quint64 ApplicationController::collectedLibraryOperationId() const
{
    return m_collectedLibraryOperationId;
}

int ApplicationController::searchMode() const
{
    return m_searchMode;
}

void ApplicationController::setSearchMode(int mode)
{
    if ((mode != TrackSearch && mode != PlaylistSearch)
            || m_searchMode == mode) {
        return;
    }
    if (m_service && m_searchOperationId != 0) {
        m_service->cancel(m_searchOperationId);
    }
    m_searchMode = mode;
    m_searchOperationId = 0;
    emit searchOperationIdChanged();
    m_searchTracks->clear();
    m_searchPlaylists->clear();
    m_searchPage = 0;
    m_searchTotal = 0;
    m_searchAppend = false;
    m_searchAccountUin.clear();
    setSearchState(PageState::Empty);
    emit searchModeChanged();
}

QString ApplicationController::playlistTitle() const
{
    return m_playlistInfo.title;
}

QString ApplicationController::playlistPeriod() const
{
    return m_playlistPeriod;
}

QString ApplicationController::playlistAuthor() const
{
    return m_playlistInfo.author;
}

QUrl ApplicationController::playlistArtwork() const
{
    return m_playlistInfo.artworkUrl;
}

int ApplicationController::playlistTrackCount() const
{
    return m_playlistInfo.trackCount;
}

QUrl ApplicationController::playlistSourceUrl() const
{
    return m_playlistInfo.sourceUrl;
}

QString ApplicationController::userUin() const
{
    return m_userUin;
}

void ApplicationController::setUserUin(const QString &uin)
{
    // A validated re-login refreshes credentials even when the identity is
    // unchanged. Invalidate operations still using the previous active jar.
    if (m_userUin == uin && m_loginStatus != QStringLiteral("validating")) {
        return;
    }
    const quint64 oldCreatedOperationId = m_myMusicOperationId;
    const quint64 oldCollectedOperationId = m_collectedLibraryOperationId;
    m_myMusicOperationId = 0;
    m_collectedLibraryOperationId = 0;
    emit myMusicOperationIdChanged();
    if (m_service && oldCreatedOperationId != 0) {
        m_service->cancel(oldCreatedOperationId);
    }
    if (m_service && oldCollectedOperationId != 0) {
        m_service->cancel(oldCollectedOperationId);
    }
    m_likedPlaylists->clear();
    m_createdPlaylists->clear();
    m_collectedPlaylists->clear();
    setCreatedPlaylistsState(PageState::Empty);
    setCollectedPlaylistsState(PageState::Empty);
    setMyMusicState(PageState::Empty);
    m_userUin = uin;
    emit userUinChanged();
}

bool ApplicationController::loggedIn() const
{
    return m_loggedIn;
}

QString ApplicationController::nickname() const
{
    return m_nickname;
}

QUrl ApplicationController::avatar() const
{
    return m_avatar;
}

PlaybackController *ApplicationController::playback() const
{
    return m_playbackController;
}

void ApplicationController::setPlaybackController(
    PlaybackController *playbackController)
{
    if (m_playbackController) {
        m_playbackController->disconnect(this);
    }
    m_playbackController = playbackController;
    if (m_playbackController) {
        m_playbackController->setAccountSessionUin(
            m_loggedIn ? m_userUin : QString());
        connect(m_playbackController, &PlaybackController::queueChanged,
                this, &ApplicationController::schedulePersistQueue);
        connect(m_playbackController, &PlaybackController::currentChanged,
                this, &ApplicationController::schedulePersistQueue);
        connect(m_playbackController, &PlaybackController::currentChanged,
                this, &ApplicationController::requestCurrentLyrics);
        connect(m_playbackController, &PlaybackController::repeatModeChanged,
                this, &ApplicationController::schedulePersistQueue);
        connect(m_playbackController, &PlaybackController::positionChanged,
                this, &ApplicationController::schedulePersistQueue);
        connect(m_playbackController, &PlaybackController::positionChanged,
                this, &ApplicationController::updateLyricPosition);
        connect(m_playbackController, &PlaybackController::errorChanged,
                this, &ApplicationController::playbackFailed);
        if (m_settingsStore && !m_queueRestored
                && m_settingsStore->resumeEnabled()) {
            bool ok = false;
            const QueueSnapshot snapshot = m_settingsStore->loadQueue(&ok);
            if (ok && !snapshot.tracks.isEmpty()) {
                m_playbackController->restoreQueue(
                    snapshot.tracks, snapshot.currentMid,
                    snapshot.repeatMode, snapshot.positionMs);
            }
            m_queueRestored = true;
        }
        requestCurrentLyrics();
    }
}

SettingsStore *ApplicationController::settings() const
{
    return m_settingsStore;
}

ImageCache *ApplicationController::imageCache() const
{
    return m_imageCache;
}

int ApplicationController::artworkRevision() const
{
    return m_imageCache ? m_imageCache->revision() : 0;
}

void ApplicationController::setStorage(SettingsStore *settingsStore,
                                       ImageCache *imageCache)
{
    const bool imageCacheChanged = m_imageCache != imageCache;
    if (m_settingsStore && m_settingsStore != settingsStore) {
        disconnect(m_settingsStore, 0, this, 0);
    }
    if (m_imageCache && m_imageCache != imageCache) {
        disconnect(m_imageCache, 0, this, 0);
    }
    m_settingsStore = settingsStore;
    m_imageCache = imageCache;
    if (m_settingsStore) {
        connect(m_settingsStore, &SettingsStore::cacheLimitBytesChanged,
                this, &ApplicationController::cacheLimitBytesChanged,
                Qt::UniqueConnection);
    }
    if (m_imageCache) {
        connect(m_imageCache, &ImageCache::usageChanged,
                this, &ApplicationController::cacheUsageChanged,
                Qt::UniqueConnection);
        connect(m_imageCache, &ImageCache::revisionChanged,
                this, &ApplicationController::artworkRevisionChanged,
                Qt::UniqueConnection);
        connect(m_imageCache, &ImageCache::artworkAvailable,
                this, &ApplicationController::artworkAvailable,
                Qt::UniqueConnection);
        connect(m_imageCache, &ImageCache::artworkForegroundAvailable,
                this, &ApplicationController::artworkForegroundAvailable,
                Qt::UniqueConnection);
        connect(m_imageCache, &ImageCache::cacheReset,
                this, &ApplicationController::artworkCacheReset,
                Qt::UniqueConnection);
    }
    if (imageCacheChanged) {
        emit artworkRevisionChanged();
    }
    if (m_playbackController && m_settingsStore && !m_queueRestored
            && m_settingsStore->resumeEnabled()) {
        bool ok = false;
        const QueueSnapshot snapshot = m_settingsStore->loadQueue(&ok);
        if (ok && !snapshot.tracks.isEmpty()) {
            m_playbackController->restoreQueue(
                snapshot.tracks, snapshot.currentMid,
                snapshot.repeatMode, snapshot.positionMs);
        }
        m_queueRestored = true;
    }
}

qint64 ApplicationController::cacheUsageBytes() const
{
    return (m_service ? m_service->publicCacheSize() : 0)
        + (m_imageCache ? m_imageCache->usageBytes() : 0);
}

qint64 ApplicationController::cacheLimitBytes() const
{
    return m_settingsStore ? m_settingsStore->cacheLimitBytes()
                           : SettingsStore::maximumCombinedCacheBytes();
}

bool ApplicationController::authenticationRemovalPending() const
{
    return m_authenticationRemovalPending;
}

QString ApplicationController::loginStatus() const
{
    return m_loginStatus;
}

QString ApplicationController::loginDiagnosticCode() const
{
    return m_loginDiagnosticCode;
}

QString ApplicationController::loginQrImage() const
{
    return m_loginQrImage;
}

int ApplicationController::loginQrGeneration() const
{
    return m_loginQrGeneration;
}

bool ApplicationController::loginCancellable() const
{
    return m_loginCancellable;
}

void ApplicationController::schedulePersistQueue()
{
    if (m_settingsStore && m_playbackController
            && !m_persistTimer->isActive()) {
        m_persistTimer->start();
    }
}

void ApplicationController::persistQueue()
{
    if (!m_settingsStore || !m_playbackController) {
        return;
    }
    QueueSnapshot snapshot;
    snapshot.tracks = m_playbackController->queueTracks();
    snapshot.currentMid = m_playbackController->currentTrack().mid;
    snapshot.repeatMode = m_playbackController->repeatMode();
    snapshot.positionMs = m_playbackController->positionMs();
    m_settingsStore->saveQueue(snapshot);
}

void ApplicationController::setSessionStore(SessionStore *sessionStore)
{
    if (m_sessionStore == sessionStore) {
        return;
    }
    if (m_sessionStore) {
        m_sessionStore->disconnect(this);
    }
    m_sessionStore = sessionStore;
    if (!m_sessionStore) {
        if (m_authenticationRemovalPending) {
            m_authenticationRemovalPending = false;
            emit authenticationRemovalPendingChanged();
        }
        clearSessionState(false);
        return;
    }
    connect(m_sessionStore, &SessionStore::sessionActivated,
            this, &ApplicationController::sessionActivated);
    connect(m_sessionStore, &SessionStore::sessionCleared,
            this, &ApplicationController::sessionCleared);
    connect(m_sessionStore, &SessionStore::sessionActionFailed,
            this, &ApplicationController::accountActionFailed);
    connect(m_sessionStore, &SessionStore::sessionFailed,
            this, [this](const ServiceError &error) {
        if (m_loginStatus == QStringLiteral("validating")) {
            setLoginPresentation(m_loginStatus, m_loginQrImage,
                                 m_loginQrGeneration, m_loginCancellable,
                                 error.diagnosticCode);
        }
    });
    connect(m_sessionStore, &SessionStore::candidateFailed,
            this, [this]() {
        if (m_loginStatus == QStringLiteral("validating")) {
            qInfo().noquote().nospace()
                << "qqmusic_login component=app event=candidate_failed attempt="
                << m_loginAttemptId;
            const QString diagnostic = m_loginDiagnosticCode.isEmpty()
                ? QStringLiteral("qq.session.candidate.failed")
                : m_loginDiagnosticCode;
            setLoginPresentation(QStringLiteral("error"), QString(), 0,
                                 false, diagnostic);
            emit accountActionFailed();
        }
    });
    connect(m_sessionStore, &SessionStore::removalPendingChanged,
            this, &ApplicationController::sessionRemovalPendingChanged);
    sessionRemovalPendingChanged();
    if (m_sessionStore->loggedIn()) {
        sessionActivated(m_sessionStore->profile());
    } else {
        clearSessionState(false);
    }
}

void ApplicationController::setNativeLoginManager(
    NativeLoginManager *loginManager)
{
    if (m_loginManager == loginManager) {
        return;
    }
    if (m_loginManager) {
        m_loginManager->disconnect(this);
    }
    m_loginManager = loginManager;
    setLoginPresentation(QString(), QString(), 0, false);
    m_loginAttemptId.clear();
    if (!m_loginManager) {
        return;
    }
    connect(m_loginManager, &NativeLoginManager::stateChanged,
            this, [this](NativeLoginManager::State state,
                         const QString &attemptId) {
        m_loginAttemptId = attemptId;
        switch (state) {
        case NativeLoginManager::Idle:
            setLoginPresentation(QString(), QString(), 0, false);
            break;
        case NativeLoginManager::Opening:
            setLoginPresentation(QStringLiteral("opening"),
                                 QString(), 0, true);
            break;
        case NativeLoginManager::QrReady:
            setLoginPresentation(QStringLiteral("qr"),
                                 m_loginQrImage,
                                 m_loginQrGeneration, true);
            break;
        case NativeLoginManager::Scanned:
        case NativeLoginManager::Exchanging:
            setLoginPresentation(QStringLiteral("scanned"),
                                 m_loginQrImage,
                                 m_loginQrGeneration, true);
            break;
        case NativeLoginManager::CandidateReady:
            setLoginPresentation(QStringLiteral("validating"),
                                 QString(), 0, false);
            break;
        case NativeLoginManager::Cancelled:
            setLoginPresentation(QStringLiteral("cancelled"),
                                 QString(), 0, false);
            break;
        case NativeLoginManager::TimedOut:
            setLoginPresentation(QStringLiteral("timeout"),
                                 QString(), 0, false);
            break;
        case NativeLoginManager::Failed:
            setLoginPresentation(QStringLiteral("error"),
                                 QString(), 0, false);
            break;
        }
    });
    connect(m_loginManager, &NativeLoginManager::qrReady,
            this, [this](const QString &attemptId, const QByteArray &png,
                         int, int, int generation) {
        if (!m_loginManager || attemptId != m_loginManager->attemptId()) {
            return;
        }
        const QString dataUri = QStringLiteral("data:image/png;base64,")
            + QString::fromLatin1(png.toBase64());
        setLoginPresentation(QStringLiteral("qr"), dataUri,
                             generation, true);
    });
    connect(m_loginManager, &NativeLoginManager::candidateReady,
            this, [this](const QString &attemptId,
                         const QVector<CookieRecord> &records) {
        if (!m_loginManager || attemptId != m_loginManager->attemptId()
                || !m_sessionStore) {
            setLoginPresentation(QStringLiteral("error"),
                                 QString(), 0, false);
            emit accountActionFailed();
            return;
        }
        m_loginAttemptId = attemptId;
        setLoginPresentation(QStringLiteral("validating"),
                             QString(), 0, false);
        qInfo().noquote().nospace()
            << "qqmusic_login component=app event=candidate_submitted attempt="
            << attemptId << " cookie_count=" << records.size();
        m_sessionStore->submitCandidate(records);
    });
    connect(m_loginManager, &NativeLoginManager::attemptFailed,
            this, [this](const QString &attemptId,
                         const ServiceError &error) {
        if (!m_loginManager || attemptId != m_loginManager->attemptId())
            return;
        qInfo().noquote().nospace()
            << "qqmusic_login component=app event=attempt_failed attempt="
            << attemptId << " diagnostic=" << error.diagnosticCode;
        setLoginPresentation(error.kind == ErrorKind::Timeout
                                 ? QStringLiteral("timeout")
                                 : QStringLiteral("error"), QString(), 0, false,
                             error.diagnosticCode);
        if (error.kind != ErrorKind::Timeout
                && error.kind != ErrorKind::Cancelled) {
            emit accountActionFailed();
        }
    });
}

quint64 ApplicationController::nextOperationId()
{
    ++m_nextOperationId;
    if (m_nextOperationId == 0) {
        ++m_nextOperationId;
    }
    return m_nextOperationId;
}

int ApplicationController::activeSearchCount() const
{
    return m_searchMode == PlaylistSearch
        ? m_searchPlaylists->rowCount()
        : m_searchTracks->rowCount();
}

bool ApplicationController::hasHomeContent() const
{
    return m_recommendations->rowCount() > 0;
}

bool ApplicationController::hasDiscoverContent() const
{
    if (m_publicPlaylists->rowCount() > 0
            || m_toplists->rowCount() > 0) {
        return true;
    }
    for (int index = 0; index < m_categoryGroups.size(); ++index) {
        const QVariantMap group = m_categoryGroups.at(index).toMap();
        if (!group.value(QStringLiteral("items")).toList().isEmpty()) {
            return true;
        }
    }
    return false;
}

void ApplicationController::requestDiscoverToplists()
{
    m_discoverStage = DiscoverStage::Toplists;
    if (!m_service || !m_service->requestToplists(m_discoverOperationId)) {
        m_discoverStage = DiscoverStage::None;
        setDiscoverState(PageState::Error);
    }
}

void ApplicationController::finishHome()
{
    m_homeStage = HomeStage::None;
    updateHomeRecommendations();
    setHomeState(hasHomeContent() ? PageState::Content : PageState::Empty);
}

void ApplicationController::requestHomeDailyThirty()
{
    m_homeStage = HomeStage::DailyThirty;
    if (!m_service
            || !m_service->requestDailyThirty(m_homeOperationId)) {
        m_dailyThirty = PlaylistDetail();
        requestHomeGuessYouLike();
    }
}

void ApplicationController::requestHomeGuessYouLike()
{
    m_homeStage = HomeStage::GuessYouLike;
    if (!m_service
            || !m_service->requestGuessYouLike(m_homeOperationId)) {
        m_guessYouLike = PlaylistDetail();
        requestHomePublicRecommendations();
    }
}

void ApplicationController::requestHomePublicRecommendations()
{
    m_homeStage = HomeStage::Recommendations;
    if (!m_service
            || !m_service->requestRecommendations(m_homeOperationId)) {
        m_homeStage = HomeStage::None;
        setHomeState(hasHomeContent()
            ? PageState::OfflineWithCache : PageState::Error);
    }
}

void ApplicationController::updateHomeRecommendations()
{
    QVector<PlaylistSummary> items;
    if (m_loggedIn && !m_dailyThirty.tracks.isEmpty()) {
        items.append(m_dailyThirty.info);
    }
    if (m_loggedIn && !m_guessYouLike.tracks.isEmpty()) {
        items.append(m_guessYouLike.info);
    }
    items += m_publicHomeRecommendations;
    m_recommendations->replace(items);
}

bool ApplicationController::openCachedPersonalized(
    const QString &playlistId)
{
    const PlaylistDetail *detail = 0;
    if (playlistId == DailyThirtyPlaylistId) {
        detail = &m_dailyThirty;
    } else if (playlistId == GuessYouLikePlaylistId) {
        detail = &m_guessYouLike;
    } else {
        return false;
    }

    m_playlistId = playlistId;
    m_playlistPeriod.clear();
    m_playlistOperationId = nextOperationId();
    emit playlistOperationIdChanged();
    m_playlistInfo = detail->info;
    m_playlistTracks->replace(detail->tracks);
    emit playlistInfoChanged();
    setPlaylistState(detail->tracks.isEmpty()
        ? PageState::Empty : PageState::Content);
    return true;
}

void ApplicationController::finishDiscover()
{
    m_discoverStage = DiscoverStage::None;
    setDiscoverState(hasDiscoverContent()
        ? PageState::Content : PageState::Empty);
}

void ApplicationController::setHomeState(PageState state)
{
    if (m_homeState != state) {
        m_homeState = state;
        emit homeStateChanged();
    }
}

void ApplicationController::setDiscoverState(PageState state)
{
    if (m_discoverState != state) {
        m_discoverState = state;
        emit discoverStateChanged();
    }
}

void ApplicationController::setSearchState(PageState state)
{
    if (m_searchState != state) {
        m_searchState = state;
        emit searchStateChanged();
    }
}

void ApplicationController::setPlaylistState(PageState state)
{
    if (m_playlistState != state) {
        m_playlistState = state;
        emit playlistStateChanged();
    }
}

void ApplicationController::setMyMusicState(PageState state)
{
    if (m_myMusicState != state) {
        m_myMusicState = state;
        emit myMusicStateChanged();
    }
}

void ApplicationController::setCreatedPlaylistsState(PageState state)
{
    if (m_createdPlaylistsState != state) {
        m_createdPlaylistsState = state;
        emit createdPlaylistsStateChanged();
    }
}

void ApplicationController::setCollectedPlaylistsState(PageState state)
{
    if (m_collectedPlaylistsState != state) {
        m_collectedPlaylistsState = state;
        emit collectedPlaylistsStateChanged();
    }
}

void ApplicationController::updateMyMusicState()
{
    const bool hasContent = m_likedPlaylists->rowCount() > 0
        || m_createdPlaylists->rowCount() > 0
        || m_collectedPlaylists->rowCount() > 0;
    if (m_createdPlaylistsState == PageState::OfflineWithCache
            || m_collectedPlaylistsState == PageState::OfflineWithCache) {
        setMyMusicState(PageState::OfflineWithCache);
    } else if (hasContent) {
        setMyMusicState(PageState::Content);
    } else if (m_createdPlaylistsState == PageState::Loading
               || m_collectedPlaylistsState == PageState::Loading) {
        setMyMusicState(PageState::Loading);
    } else if (m_createdPlaylistsState == PageState::Empty
               && m_collectedPlaylistsState == PageState::Empty) {
        setMyMusicState(PageState::Empty);
    } else {
        setMyMusicState(PageState::Error);
    }
}

void ApplicationController::setLyricState(PageState state)
{
    if (m_lyricState != state) {
        m_lyricState = state;
        emit lyricStateChanged();
    }
}

void ApplicationController::refreshHome()
{
    if (m_service && m_homeOperationId != 0
            && m_homeStage != HomeStage::None) {
        m_service->cancel(m_homeOperationId);
    }
    ++m_homeRefreshSequence;
    if (m_homeRefreshSequence == 0) {
        ++m_homeRefreshSequence;
    }
    m_homeOperationId = nextOperationId();
    emit homeOperationIdChanged();
    setHomeState(PageState::Loading);
    if (m_loggedIn) {
        requestHomeDailyThirty();
    } else {
        requestHomePublicRecommendations();
    }
}

void ApplicationController::refreshDiscover()
{
    if (m_service && m_discoverOperationId != 0
            && m_discoverStage != DiscoverStage::None) {
        m_service->cancel(m_discoverOperationId);
    }
    m_discoverOperationId = nextOperationId();
    emit discoverOperationIdChanged();
    m_discoverStage = DiscoverStage::Categories;
    setDiscoverState(PageState::Loading);
    if (!m_service
            || !m_service->requestCategories(m_discoverOperationId)) {
        m_discoverStage = DiscoverStage::None;
        setDiscoverState(PageState::Error);
    }
}

void ApplicationController::selectCategory(const QString &categoryId)
{
    bool knownCategory = false;
    for (int groupIndex = 0; groupIndex < m_categoryGroups.size(); ++groupIndex) {
        const QVariantList items = m_categoryGroups.at(groupIndex).toMap()
            .value(QStringLiteral("items")).toList();
        for (int itemIndex = 0; itemIndex < items.size(); ++itemIndex) {
            if (items.at(itemIndex).toMap()
                    .value(QStringLiteral("categoryId")).toString()
                    == categoryId) {
                knownCategory = true;
                break;
            }
        }
        if (knownCategory) {
            break;
        }
    }
    if (!knownCategory) {
        return;
    }
    if (m_service && m_discoverOperationId != 0
            && m_discoverStage != DiscoverStage::None) {
        m_service->cancel(m_discoverOperationId);
    }
    if (m_selectedCategoryId != categoryId) {
        m_publicPlaylists->clear();
        m_selectedCategoryId = categoryId;
        emit selectedCategoryChanged();
    } else {
        m_publicPlaylists->clear();
    }
    m_discoverOperationId = nextOperationId();
    emit discoverOperationIdChanged();
    m_discoverStage = DiscoverStage::FilteredPlaylists;
    setDiscoverState(PageState::Loading);
    if (!m_service
            || !m_service->requestPlaylists(
                m_discoverOperationId, categoryId, 0)) {
        m_discoverStage = DiscoverStage::None;
        setDiscoverState(PageState::Error);
    }
}

void ApplicationController::search(const QString &query)
{
    if (m_service && m_searchOperationId != 0) {
        m_service->cancel(m_searchOperationId);
    }
    m_searchQuery = query.trimmed();
    m_searchTracks->clear();
    m_searchPlaylists->clear();
    m_searchPage = 0;
    m_searchTotal = 0;
    m_searchAppend = false;
    m_searchAccountUin = m_loggedIn ? m_userUin : QString();
    if (m_searchQuery.isEmpty()) {
        m_searchOperationId = 0;
        m_searchAccountUin.clear();
        emit searchOperationIdChanged();
        setSearchState(PageState::Empty);
        return;
    }
    m_searchPage = 1;
    m_searchOperationId = nextOperationId();
    emit searchOperationIdChanged();
    setSearchState(PageState::Loading);
    const bool started = m_service
        && (m_searchMode == PlaylistSearch
            ? m_service->searchPlaylists(
                m_searchOperationId, m_searchQuery, m_searchPage,
                m_searchAccountUin)
            : m_service->searchTracks(
                m_searchOperationId, m_searchQuery, m_searchPage,
                m_searchAccountUin));
    if (!started) {
        setSearchState(PageState::Error);
    }
}

void ApplicationController::loadMoreSearch()
{
    if (!m_service
            || m_searchQuery.isEmpty()
            || activeSearchCount() == 0
            || m_searchState == PageState::Loading
            || (m_searchTotal > 0
                && activeSearchCount() >= m_searchTotal)) {
        return;
    }
    ++m_searchPage;
    m_searchAppend = true;
    m_searchOperationId = nextOperationId();
    emit searchOperationIdChanged();
    setSearchState(PageState::Loading);
    const bool started = m_searchMode == PlaylistSearch
        ? m_service->searchPlaylists(
            m_searchOperationId, m_searchQuery, m_searchPage,
            m_searchAccountUin)
        : m_service->searchTracks(
            m_searchOperationId, m_searchQuery, m_searchPage,
            m_searchAccountUin);
    if (!started) {
        --m_searchPage;
        m_searchAppend = false;
        setSearchState(PageState::Error);
    }
}

void ApplicationController::openPlaylist(const QString &playlistId)
{
    if (m_service && m_playlistOperationId != 0) {
        m_service->cancel(m_playlistOperationId);
    }
    if (openCachedPersonalized(playlistId)) {
        return;
    }
    m_playlistId = playlistId;
    m_playlistTracks->clear();
    m_playlistInfo = PlaylistSummary();
    m_playlistPeriod.clear();
    m_playlistInfo.id = playlistId;
    emit playlistInfoChanged();
    m_playlistOperationId = nextOperationId();
    emit playlistOperationIdChanged();
    setPlaylistState(PageState::Loading);
    if (!m_service
            || !m_service->requestPlaylist(
                m_playlistOperationId, playlistId, false)) {
        setPlaylistState(PageState::Error);
    }
}

void ApplicationController::openToplist(const QString &toplistId)
{
    if (m_service && m_playlistOperationId != 0) {
        m_service->cancel(m_playlistOperationId);
    }
    m_playlistId = toplistId;
    m_playlistTracks->clear();
    m_playlistInfo = PlaylistSummary();
    m_playlistPeriod.clear();
    m_playlistInfo.id = toplistId;
    emit playlistInfoChanged();
    m_playlistOperationId = nextOperationId();
    emit playlistOperationIdChanged();
    setPlaylistState(PageState::Loading);
    if (!m_service
            || !m_service->requestToplist(m_playlistOperationId, toplistId)) {
        setPlaylistState(PageState::Error);
    }
}

bool ApplicationController::playPlaylistAt(int index)
{
    if (!m_playbackController) {
        return false;
    }
    const QVector<Track> tracks = m_playlistTracks->items();
    if (index < 0 || index >= tracks.size()) {
        return false;
    }
    m_playbackController->replaceQueue(tracks, index, true);
    return true;
}

bool ApplicationController::playSearchTrack(int index)
{
    if (!m_playbackController) {
        return false;
    }
    Track track;
    if (!m_searchTracks->itemAt(index, &track)) {
        return false;
    }
    m_playbackController->replaceQueue(QVector<Track>() << track, 0, true);
    return true;
}

bool ApplicationController::playQueueIndex(int index)
{
    if (!m_playbackController) {
        return false;
    }
    const QVector<Track> tracks = m_playbackController->queueTracks();
    if (index < 0 || index >= tracks.size()) {
        return false;
    }
    m_playbackController->replaceQueue(tracks, index, true);
    return true;
}

bool ApplicationController::playNextPlaylistTrack(int index)
{
    Track track;
    if (!m_playbackController || !m_playlistTracks->itemAt(index, &track)) {
        return false;
    }
    m_playbackController->addNext(track);
    return true;
}

bool ApplicationController::appendPlaylistTrack(int index)
{
    Track track;
    if (!m_playbackController || !m_playlistTracks->itemAt(index, &track)) {
        return false;
    }
    m_playbackController->append(track);
    return true;
}

bool ApplicationController::playNextSearchTrack(int index)
{
    Track track;
    if (!m_playbackController || !m_searchTracks->itemAt(index, &track)) {
        return false;
    }
    m_playbackController->addNext(track);
    return true;
}

bool ApplicationController::appendSearchTrack(int index)
{
    Track track;
    if (!m_playbackController || !m_searchTracks->itemAt(index, &track)) {
        return false;
    }
    m_playbackController->append(track);
    return true;
}

bool ApplicationController::clearCache()
{
    const bool metadataCleared = !m_service || m_service->clearPublicCache();
    const bool artworkCleared = !m_imageCache || m_imageCache->clear();
    if (!m_imageCache) {
        emit cacheUsageChanged();
    }
    return metadataCleared && artworkCleared;
}

QUrl ApplicationController::artworkSource(const QUrl &url)
{
    return m_imageCache ? m_imageCache->source(url) : url;
}

bool ApplicationController::requestArtworkForeground(const QUrl &url)
{
    return m_imageCache && m_imageCache->requestForeground(url);
}

void ApplicationController::setCacheLimitBytes(qint64 bytes)
{
    if (!m_settingsStore) {
        return;
    }
    m_settingsStore->setCacheLimitBytes(bytes);
    const qint64 limit = m_settingsStore->cacheLimitBytes();
    const qint64 networkBytes = qMax<qint64>(1, qMin(
        SettingsStore::networkCacheBytes(), limit / 4));
    if (m_service) {
        m_service->setPublicCacheMaximumBytes(networkBytes);
    }
    if (m_imageCache) {
        m_imageCache->setMaximumBytes(qMax<qint64>(1,
                                                  limit - networkBytes));
    } else {
        emit cacheUsageChanged();
    }
}

void ApplicationController::refreshMyMusic()
{
    if (m_userUin.isEmpty()) {
        const quint64 oldCreatedOperationId = m_myMusicOperationId;
        const quint64 oldCollectedOperationId = m_collectedLibraryOperationId;
        m_myMusicOperationId = 0;
        m_collectedLibraryOperationId = 0;
        emit myMusicOperationIdChanged();
        if (m_service && oldCreatedOperationId != 0) {
            m_service->cancel(oldCreatedOperationId);
        }
        if (m_service && oldCollectedOperationId != 0) {
            m_service->cancel(oldCollectedOperationId);
        }
        setCreatedPlaylistsState(PageState::Error);
        setCollectedPlaylistsState(PageState::Error);
        setMyMusicState(PageState::Error);
        return;
    }
    if (m_service && m_myMusicOperationId != 0) {
        m_service->cancel(m_myMusicOperationId);
    }
    if (m_service && m_collectedLibraryOperationId != 0) {
        m_service->cancel(m_collectedLibraryOperationId);
    }
    m_myMusicOperationId = nextOperationId();
    m_collectedLibraryOperationId = nextOperationId();
    emit myMusicOperationIdChanged();
    setCreatedPlaylistsState(PageState::Loading);
    setCollectedPlaylistsState(PageState::Loading);
    updateMyMusicState();
    if (!m_service || !m_service->requestCreatedLibrary(
            m_myMusicOperationId, m_userUin)) {
        setCreatedPlaylistsState(PageState::Error);
    }
    if (!m_service || !m_service->requestCollectedLibrary(
            m_collectedLibraryOperationId, m_userUin)) {
        setCollectedPlaylistsState(PageState::Error);
    }
    updateMyMusicState();
}

void ApplicationController::startLogin()
{
    if (!m_loginManager || m_loginManager->active()
            || m_loginStatus == QStringLiteral("validating")) {
        if (!m_loginManager) {
            setLoginPresentation(QStringLiteral("error"),
                                 QString(), 0, false);
            emit accountActionFailed();
        }
        return;
    }
    setLoginPresentation(QStringLiteral("opening"), QString(), 0, true);
    m_loginManager->start();
    m_loginAttemptId = m_loginManager->attemptId();
    qInfo().noquote().nospace()
        << "qqmusic_login component=app event=start_requested attempt="
        << m_loginAttemptId;
}

void ApplicationController::cancelLogin()
{
    if (m_loginManager && m_loginCancellable
            && m_loginStatus != QStringLiteral("validating")) {
        qInfo().noquote().nospace()
            << "qqmusic_login component=app event=cancel_requested attempt="
            << m_loginAttemptId;
        m_loginManager->cancel();
    }
}

void ApplicationController::logout()
{
    if (m_loginManager && m_loginManager->active()) {
        m_loginManager->cancel();
    }
    if (m_sessionStore) {
        m_sessionStore->logout();
        return;
    }
    sessionCleared();
}

void ApplicationController::confirmExpiredSessionRemoval()
{
    if (m_sessionStore) {
        m_sessionStore->confirmExpiredSessionRemoval();
    }
}

void ApplicationController::sessionActivated(const UserProfile &profile)
{
    const bool candidateActivation = m_loginStatus == QStringLiteral("validating");
    qInfo().noquote()
        << "qqmusic-session app status=activated";
    setUserUin(profile.uin);
    const bool changed = !m_loggedIn
        || m_nickname != profile.nickname
        || m_avatar != profile.avatarUrl;
    m_loggedIn = true;
    m_nickname = profile.nickname;
    m_avatar = profile.avatarUrl;
    if (m_playbackController) {
        m_playbackController->setAccountSessionUin(profile.uin);
    }
    if (changed) {
        emit sessionChanged();
    }
    if (m_loginStatus == QStringLiteral("validating")) {
        qInfo().noquote().nospace()
            << "qqmusic_login component=app event=session_activated attempt="
            << m_loginAttemptId;
        setLoginPresentation(QStringLiteral("success"), QString(), 0, false);
    }
    refreshHome();
    if (!m_searchQuery.isEmpty()
            && (candidateActivation || m_searchAccountUin != profile.uin)) {
        search(m_searchQuery);
    }
}

void ApplicationController::sessionRemovalPendingChanged()
{
    const bool pending = m_sessionStore && m_sessionStore->removalPending();
    if (m_authenticationRemovalPending != pending) {
        m_authenticationRemovalPending = pending;
        emit authenticationRemovalPendingChanged();
    }
}

void ApplicationController::sessionCleared()
{
    if (m_playbackController) {
        m_playbackController->sessionEnded();
    }
    clearSessionState(true);
}

void ApplicationController::clearSessionState(bool notifyLogout)
{
    const bool refreshAnonymousSearch = !m_searchQuery.isEmpty()
        && !m_searchAccountUin.isEmpty();
    if (!m_loginManager || !m_loginManager->active()) {
        setLoginPresentation(QString(), QString(), 0, false);
        m_loginAttemptId.clear();
    }
    setUserUin(QString());
    m_likedPlaylists->clear();
    m_createdPlaylists->clear();
    m_collectedPlaylists->clear();
    m_dailyThirty = PlaylistDetail();
    m_guessYouLike = PlaylistDetail();
    updateHomeRecommendations();
    m_myMusicOperationId = 0;
    m_collectedLibraryOperationId = 0;
    emit myMusicOperationIdChanged();
    setCreatedPlaylistsState(PageState::Empty);
    setCollectedPlaylistsState(PageState::Empty);
    setMyMusicState(PageState::Empty);
    const bool changed = m_loggedIn || !m_nickname.isEmpty() || !m_avatar.isEmpty();
    m_loggedIn = false;
    m_nickname.clear();
    m_avatar = QUrl();
    if (changed) {
        emit sessionChanged();
    }
    if (refreshAnonymousSearch) {
        search(m_searchQuery);
    }
    if (notifyLogout) {
        emit logoutRequested();
    }
}

void ApplicationController::setLoginPresentation(
    const QString &status,
    const QString &qrImage,
    int generation,
    bool cancellable,
    const QString &diagnosticCode)
{
    if (m_loginStatus == status
            && m_loginDiagnosticCode == diagnosticCode
            && m_loginQrImage == qrImage
            && m_loginQrGeneration == generation
            && m_loginCancellable == cancellable) {
        return;
    }
    m_loginStatus = status;
    m_loginDiagnosticCode = diagnosticCode;
    m_loginQrImage = qrImage;
    m_loginQrGeneration = generation;
    m_loginCancellable = cancellable;
    emit loginChanged();
}

void ApplicationController::recommendationsReady(
    quint64 operationId, const QVector<PlaylistSummary> &items)
{
    if (operationId != m_homeOperationId) {
        return;
    }
    if (m_homeStage != HomeStage::Recommendations) {
        return;
    }
    int selectedOffset = 0;
    const QVector<PlaylistSummary> visible = recommendationWindow(
        items, m_homeRefreshSequence, &selectedOffset);
    m_publicHomeRecommendations = visible;
    qInfo().noquote() << QStringLiteral(
        "qqmusic_home component=controller event=recommendations_applied"
        " revision=%1 offset=%2 visible=%3 source=%4")
        .arg(m_homeRefreshSequence)
        .arg(selectedOffset)
        .arg(visible.size())
        .arg(items.size());
    finishHome();
}

void ApplicationController::dailyThirtyReady(
    quint64 operationId, const QVector<Track> &items)
{
    if (operationId != m_homeOperationId
            || m_homeStage != HomeStage::DailyThirty) {
        return;
    }
    m_dailyThirty = PlaylistDetail();
    m_dailyThirty.info.id = DailyThirtyPlaylistId;
    m_dailyThirty.info.title = tr("Daily 30");
    m_dailyThirty.info.author = tr("For you");
    m_dailyThirty.info.trackCount = items.size();
    m_dailyThirty.tracks = items;
    if (!items.isEmpty()) {
        m_dailyThirty.info.artworkUrl = items.first().artworkUrl;
    }
    qInfo().noquote().nospace()
        << "qqmusic_home component=controller event=daily30_applied count="
        << items.size();
    requestHomeGuessYouLike();
}

void ApplicationController::guessYouLikeReady(
    quint64 operationId, const QVector<Track> &items)
{
    if (operationId != m_homeOperationId
            || m_homeStage != HomeStage::GuessYouLike) {
        return;
    }
    m_guessYouLike = PlaylistDetail();
    m_guessYouLike.info.id = GuessYouLikePlaylistId;
    m_guessYouLike.info.title = tr("Guess You Like");
    m_guessYouLike.info.author = tr("Personalized");
    m_guessYouLike.info.trackCount = items.size();
    m_guessYouLike.tracks = items;
    if (!items.isEmpty()) {
        m_guessYouLike.info.artworkUrl = items.first().artworkUrl;
    }
    qInfo().noquote().nospace()
        << "qqmusic_home component=controller event=guess_applied count="
        << items.size();
    requestHomePublicRecommendations();
}

void ApplicationController::categoriesReady(
    quint64 operationId, const QVector<CategoryGroup> &groups)
{
    if (operationId != m_discoverOperationId
            || m_discoverStage != DiscoverStage::Categories) {
        return;
    }
    QVariantList categoryGroups;
    QString defaultCategoryId;
    bool selectedCategoryAvailable = false;
    for (int groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
        const CategoryGroup &group = groups.at(groupIndex);
        QVariantList items;
        for (int itemIndex = 0; itemIndex < group.items.size(); ++itemIndex) {
            const Category &category = group.items.at(itemIndex);
            QVariantMap item;
            item.insert(QStringLiteral("categoryId"), category.id);
            item.insert(QStringLiteral("title"), category.title);
            items.append(item);
            if (defaultCategoryId.isEmpty()) {
                defaultCategoryId = category.id;
            }
            if (category.id == m_selectedCategoryId) {
                selectedCategoryAvailable = true;
            }
        }
        QVariantMap value;
        value.insert(QStringLiteral("title"), group.title);
        value.insert(QStringLiteral("items"), items);
        if (!items.isEmpty()) {
            categoryGroups.append(value);
        }
    }
    if (m_categoryGroups != categoryGroups) {
        m_categoryGroups = categoryGroups;
        emit categoryGroupsChanged();
    }
    if (defaultCategoryId.isEmpty()) {
        if (!m_selectedCategoryId.isEmpty()) {
            m_selectedCategoryId.clear();
            emit selectedCategoryChanged();
        }
        m_publicPlaylists->clear();
        requestDiscoverToplists();
        return;
    }
    const QString categoryId = selectedCategoryAvailable
        ? m_selectedCategoryId : defaultCategoryId;
    if (m_selectedCategoryId != categoryId) {
        m_selectedCategoryId = categoryId;
        emit selectedCategoryChanged();
    }
    m_discoverStage = DiscoverStage::Playlists;
    if (!m_service || !m_service->requestPlaylists(
            m_discoverOperationId, categoryId, 0)) {
        m_discoverStage = DiscoverStage::None;
        setDiscoverState(PageState::Error);
    }
}

void ApplicationController::playlistsReady(
    quint64 operationId,
    const QVector<PlaylistSummary> &items,
    qint64 total)
{
    if (operationId == m_discoverOperationId
            && m_discoverStage == DiscoverStage::Playlists) {
        m_publicPlaylists->replace(items);
        requestDiscoverToplists();
        return;
    }
    if (operationId == m_discoverOperationId
            && m_discoverStage == DiscoverStage::FilteredPlaylists) {
        m_publicPlaylists->replace(items);
        finishDiscover();
        return;
    }
    if (operationId != m_searchOperationId
            || m_searchMode != PlaylistSearch) {
        return;
    }
    if (m_searchAppend) {
        m_searchPlaylists->append(items);
    } else {
        m_searchPlaylists->replace(items);
    }
    m_searchAppend = false;
    m_searchTotal = total;
    setSearchState(m_searchPlaylists->rowCount() == 0
        ? PageState::Empty
        : PageState::Content);
}

void ApplicationController::toplistsReady(
    quint64 operationId, const QVector<PlaylistSummary> &items)
{
    if (operationId != m_discoverOperationId
            || m_discoverStage != DiscoverStage::Toplists) {
        return;
    }
    m_toplists->replace(items);
    finishDiscover();
}

void ApplicationController::tracksReady(quint64 operationId,
                                        const QVector<Track> &items,
                                        qint64 total)
{
    if (operationId != m_searchOperationId
            || m_searchMode != TrackSearch) {
        return;
    }
    if (m_searchAppend) {
        m_searchTracks->append(items);
    } else {
        m_searchTracks->replace(items);
    }
    m_searchAppend = false;
    m_searchTotal = total;
    setSearchState(m_searchTracks->rowCount() == 0
        ? PageState::Empty
        : PageState::Content);
}

void ApplicationController::playlistReady(
    quint64 operationId, const PlaylistDetail &playlist)
{
    if (operationId != m_playlistOperationId) {
        return;
    }
    m_playlistInfo = playlist.info;
    m_playlistPeriod = playlist.period;
    emit playlistInfoChanged();
    m_playlistTracks->replace(playlist.tracks);
    setPlaylistState(playlist.tracks.isEmpty()
        ? PageState::Empty
        : PageState::Content);
}

void ApplicationController::createdLibraryReady(
    quint64 operationId,
    const QVector<PlaylistSummary> &liked,
    const QVector<PlaylistSummary> &created)
{
    if (operationId != m_myMusicOperationId) {
        return;
    }
    m_likedPlaylists->replace(liked);
    m_createdPlaylists->replace(created);
    setCreatedPlaylistsState(liked.isEmpty() && created.isEmpty()
        ? PageState::Empty : PageState::Content);
    updateMyMusicState();
}

void ApplicationController::collectedLibraryReady(
    quint64 operationId,
    const QVector<PlaylistSummary> &collected)
{
    if (operationId != m_collectedLibraryOperationId) {
        return;
    }
    m_collectedPlaylists->replace(collected);
    setCollectedPlaylistsState(collected.isEmpty()
        ? PageState::Empty : PageState::Content);
    updateMyMusicState();
}

void ApplicationController::lyricsReady(
    quint64 operationId, const LyricDocument &lyrics)
{
    if (operationId != m_lyricOperationId) {
        return;
    }
    m_lyricOperationId = 0;
    m_lyrics->replace(lyrics);
    setLyricState(lyrics.lines.isEmpty()
        ? PageState::Empty : PageState::Content);
    updateLyricPosition();
}

void ApplicationController::requestCurrentLyrics()
{
    if (m_service && m_lyricOperationId != 0) {
        m_service->cancel(m_lyricOperationId);
    }
    m_lyricOperationId = 0;
    m_lyrics->clear();
    const QString mid = m_playbackController
        ? m_playbackController->currentTrack().mid : QString();
    if (mid.isEmpty() || !m_service) {
        setLyricState(PageState::Empty);
        return;
    }
    m_lyricOperationId = nextOperationId();
    setLyricState(PageState::Loading);
    if (!m_service->requestLyrics(m_lyricOperationId, mid)) {
        m_lyricOperationId = 0;
        setLyricState(PageState::Error);
    }
}

void ApplicationController::updateLyricPosition()
{
    if (m_playbackController) {
        m_lyrics->setPosition(m_playbackController->positionMs());
    }
}

void ApplicationController::playbackFailed(const ServiceError &error)
{
    emit terminalPlaybackFailure(
        int(error.kind), m_playbackController
        ? m_playbackController->currentTrack().title : QString());
}

void ApplicationController::serviceFailed(const ServiceError &error)
{
    qWarning() << "QQ Music service failure: kind=" << int(error.kind)
               << "operation=" << error.operationId;
    // This service still owns requests made with the previous active jar.
    // Candidate authentication uses SessionStore's separate service/jar; an
    // old request must not cancel that validation or its Secrets commit.
    const bool currentAccountRequest = error.operationId != 0
        && !m_userUin.isEmpty()
        && (error.operationId == m_myMusicOperationId
            || error.operationId == m_collectedLibraryOperationId
            || (error.operationId == m_searchOperationId
                && m_searchAccountUin == m_userUin)
            || (error.operationId == m_homeOperationId
                && (m_homeStage == HomeStage::DailyThirty
                    || m_homeStage == HomeStage::GuessYouLike)));
    if (error.kind == ErrorKind::AuthenticationExpired && m_sessionStore
            && currentAccountRequest
            && m_loginStatus != QStringLiteral("validating")) {
        const bool firstExpiry = !m_authenticationRemovalPending;
        m_sessionStore->markAuthenticationExpired();
        if (firstExpiry) {
            emit authenticationExpired();
        }
    } else if (error.kind == ErrorKind::AuthenticationExpired
               && currentAccountRequest
               && m_loginStatus != QStringLiteral("validating")
               && !m_authenticationRemovalPending) {
        m_authenticationRemovalPending = true;
        emit authenticationRemovalPendingChanged();
        emit authenticationExpired();
    }
    if (error.operationId == m_lyricOperationId) {
        m_lyricOperationId = 0;
        m_lyrics->clear();
        setLyricState(PageState::Error);
        return;
    }
    if (error.operationId == m_homeOperationId) {
        if (error.kind != ErrorKind::AuthenticationExpired
                && m_homeStage == HomeStage::DailyThirty) {
            m_dailyThirty = PlaylistDetail();
            requestHomeGuessYouLike();
            return;
        }
        if (error.kind != ErrorKind::AuthenticationExpired
                && m_homeStage == HomeStage::GuessYouLike) {
            m_guessYouLike = PlaylistDetail();
            requestHomePublicRecommendations();
            return;
        }
        m_homeStage = HomeStage::None;
        setHomeState(error.kind == ErrorKind::Offline
                         && hasHomeContent()
                     ? PageState::OfflineWithCache
                     : PageState::Error);
    } else if (error.operationId == m_discoverOperationId) {
        m_discoverStage = DiscoverStage::None;
        setDiscoverState(error.kind == ErrorKind::Offline
                             && hasDiscoverContent()
                         ? PageState::OfflineWithCache
                         : PageState::Error);
    } else if (error.operationId == m_searchOperationId) {
        if (m_searchAppend && m_searchPage > 1) {
            --m_searchPage;
        }
        m_searchAppend = false;
        setSearchState(error.kind == ErrorKind::Offline
                           && activeSearchCount() > 0
                       ? PageState::OfflineWithCache
                       : PageState::Error);
    } else if (error.operationId == m_playlistOperationId) {
        setPlaylistState(error.kind == ErrorKind::Offline
                             && m_playlistTracks->rowCount() > 0
                         ? PageState::OfflineWithCache
                         : PageState::Error);
    } else if (error.operationId == m_myMusicOperationId) {
        const bool hasCache = m_likedPlaylists->rowCount() > 0
            || m_createdPlaylists->rowCount() > 0;
        setCreatedPlaylistsState(error.kind == ErrorKind::Offline && hasCache
            ? PageState::OfflineWithCache : PageState::Error);
        updateMyMusicState();
    } else if (error.operationId == m_collectedLibraryOperationId) {
        const bool hasCache = m_collectedPlaylists->rowCount() > 0;
        setCollectedPlaylistsState(error.kind == ErrorKind::Offline && hasCache
            ? PageState::OfflineWithCache : PageState::Error);
        updateMyMusicState();
    }
}
