#include "qqmusicservice.h"

#include "core/qqidentifier.h"
#include "qqlibraryparser.h"
#include "qqlyricparser.h"
#include "qqnetworktransport.h"
#include "qqplaybackparser.h"
#include "qqpublicparser.h"
#include "qqrequestbuilder.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QSet>

namespace {

enum class PendingKind {
    Recommendations,
    DailyThirty,
    GuessYouLike,
    Categories,
    Playlists,
    Toplists,
    ToplistPeriod,
    ToplistDetail,
    SearchTracks,
    SearchPlaylists,
    Playlist,
    User,
    LibraryCreated,
    LibraryCollected,
    Lyrics,
    Playback
};

struct PersonalizedAuthentication
{
    QString uin;
    QByteArray key;

    bool isValid() const
    {
        return QqIdentifier::isNumericPlaylistId(uin) && !key.isEmpty();
    }
};

PersonalizedAuthentication personalizedAuthentication(
    QNetworkAccessManager *manager)
{
    PersonalizedAuthentication authentication;
    if (!manager || !manager->cookieJar()) {
        return authentication;
    }
    const QList<QNetworkCookie> cookies = manager->cookieJar()->cookiesForUrl(
        QqRequestBuilder::personalizedUrl());
    static const char *const identityNames[] = {
        "uin", "wxuin", "pt2gguin"
    };
    static const char *const keyNames[] = {
        "qm_keyst", "qqmusic_key", "p_skey", "skey"
    };
    for (uint nameIndex = 0;
         nameIndex < sizeof(identityNames) / sizeof(identityNames[0]);
         ++nameIndex) {
        for (int index = 0; index < cookies.size(); ++index) {
            if (cookies.at(index).name() != identityNames[nameIndex]) {
                continue;
            }
            QString value = QString::fromLatin1(cookies.at(index).value());
            while (value.startsWith(QLatin1Char('o'))) {
                value.remove(0, 1);
            }
            if (QqIdentifier::isNumericPlaylistId(value)) {
                authentication.uin = value;
                break;
            }
        }
        if (!authentication.uin.isEmpty()) {
            break;
        }
    }
    for (uint nameIndex = 0;
         nameIndex < sizeof(keyNames) / sizeof(keyNames[0]);
         ++nameIndex) {
        for (int index = 0; index < cookies.size(); ++index) {
            if (cookies.at(index).name() == keyNames[nameIndex]
                    && !cookies.at(index).value().isEmpty()) {
                authentication.key = cookies.at(index).value();
                break;
            }
        }
        if (!authentication.key.isEmpty()) {
            break;
        }
    }
    return authentication;
}

void appendUniqueTracks(QVector<Track> *target,
                        QSet<QString> *seen,
                        const QVector<Track> &items,
                        int maximum)
{
    if (!target || !seen) {
        return;
    }
    for (int index = 0;
         index < items.size() && target->size() < maximum;
         ++index) {
        const Track &track = items.at(index);
        if (track.mid.isEmpty() || seen->contains(track.mid)) {
            continue;
        }
        seen->insert(track.mid);
        target->append(track);
    }
}

}

struct QqMusicService::Pending
{
    quint64 operationId = 0;
    PendingKind kind = PendingKind::Recommendations;
    QString expectedId;
    QString period;
    bool containsCookies = false;
    QString accountUin;
    QByteArray authenticationKey;
    int personalizedPage = 0;
    QVector<Track> personalizedTracks;
    QSet<QString> personalizedTrackMids;
};

QqMusicService::QqMusicService(QObject *parent)
    : QObject(parent)
    , m_transport(new QqNetworkTransport(this))
{
    connect(m_transport, &QqNetworkTransport::completed,
            this, &QqMusicService::complete);
    connect(m_transport, &QqNetworkTransport::failed,
            this, &QqMusicService::transportFailed);
}

QqMusicService::QqMusicService(QNetworkAccessManager *manager,
                               QObject *parent)
    : QObject(parent)
    , m_transport(new QqNetworkTransport(manager, this))
{
    connect(m_transport, &QqNetworkTransport::completed,
            this, &QqMusicService::complete);
    connect(m_transport, &QqNetworkTransport::failed,
            this, &QqMusicService::transportFailed);
}

QqMusicService::~QqMusicService()
{
    m_transport->disconnect(this);
    qDeleteAll(m_pending);
    m_pending.clear();
}

QNetworkDiskCache *QqMusicService::configurePublicCache(
    const QString &directory, qint64 maximumBytes)
{
    return m_transport->configureDiskCache(directory, maximumBytes);
}

qint64 QqMusicService::publicCacheSize() const
{
    return m_transport->diskCacheSize();
}

void QqMusicService::setPublicCacheMaximumBytes(qint64 maximumBytes)
{
    m_transport->setDiskCacheMaximumBytes(maximumBytes);
}

bool QqMusicService::clearPublicCache()
{
    return m_transport->clearDiskCache();
}

bool QqMusicService::beginGet(Pending *pending,
                              const QUrl &url,
                              QqRequestKind requestKind,
                              bool containsCookies,
                              bool forceNetwork)
{
    if (!pending
            || pending->operationId == 0
            || m_pending.contains(pending->operationId)) {
        delete pending;
        return false;
    }
    const quint64 operationId = pending->operationId;
    m_pending.insert(operationId, pending);
    const bool started = m_transport->get(
        operationId,
        url,
        requestKind,
        containsCookies,
        forceNetwork);
    if (!started && m_pending.value(operationId) == pending) {
        m_pending.remove(operationId);
        delete pending;
    }
    return started;
}

bool QqMusicService::beginPost(Pending *pending,
                               const QUrl &url,
                               QqRequestKind requestKind,
                               const QByteArray &body,
                               bool containsCookies)
{
    if (!pending
            || pending->operationId == 0
            || m_pending.contains(pending->operationId)) {
        delete pending;
        return false;
    }
    const quint64 operationId = pending->operationId;
    m_pending.insert(operationId, pending);
    const bool started = m_transport->post(
        operationId,
        url,
        requestKind,
        body,
        containsCookies);
    if (!started && m_pending.value(operationId) == pending) {
        m_pending.remove(operationId);
        delete pending;
    }
    return started;
}

bool QqMusicService::requestRecommendations(quint64 operationId)
{
    Pending *pending = new Pending;
    pending->operationId = operationId;
    pending->kind = PendingKind::Recommendations;
    return beginGet(pending,
                    QqRequestBuilder::recommendationsUrl(),
                    QqRequestKind::PublicMetadata,
                    false,
                    true);
}

bool QqMusicService::requestDailyThirty(quint64 operationId)
{
    const PersonalizedAuthentication authentication =
        personalizedAuthentication(m_transport->networkAccessManager());
    if (!authentication.isValid()) {
        return false;
    }
    const QByteArray body = QqRequestBuilder::dailyThirtyBody(
        authentication.uin, authentication.key, 1);
    if (body.isEmpty()) {
        return false;
    }
    Pending *pending = new Pending;
    pending->operationId = operationId;
    pending->kind = PendingKind::DailyThirty;
    pending->containsCookies = true;
    pending->accountUin = authentication.uin;
    pending->authenticationKey = authentication.key;
    pending->personalizedPage = 1;
    return beginPost(pending,
                     QqRequestBuilder::personalizedUrl(),
                     QqRequestKind::AuthenticatedAccount,
                     body,
                     true);
}

bool QqMusicService::requestGuessYouLike(quint64 operationId)
{
    const PersonalizedAuthentication authentication =
        personalizedAuthentication(m_transport->networkAccessManager());
    if (!authentication.isValid()) {
        return false;
    }
    const QByteArray body = QqRequestBuilder::guessYouLikeBody(
        authentication.uin, authentication.key);
    if (body.isEmpty()) {
        return false;
    }
    Pending *pending = new Pending;
    pending->operationId = operationId;
    pending->kind = PendingKind::GuessYouLike;
    pending->containsCookies = true;
    pending->accountUin = authentication.uin;
    pending->authenticationKey = authentication.key;
    pending->personalizedPage = 1;
    return beginPost(pending,
                     QqRequestBuilder::personalizedUrl(),
                     QqRequestKind::AuthenticatedAccount,
                     body,
                     true);
}

bool QqMusicService::requestCategories(quint64 operationId)
{
    Pending *pending = new Pending;
    pending->operationId = operationId;
    pending->kind = PendingKind::Categories;
    return beginGet(pending,
                    QqRequestBuilder::categoriesUrl(),
                    QqRequestKind::PublicMetadata,
                    false);
}

bool QqMusicService::requestPlaylists(quint64 operationId,
                                      const QString &categoryId,
                                      int offset)
{
    Pending *pending = new Pending;
    pending->operationId = operationId;
    pending->kind = PendingKind::Playlists;
    return beginGet(pending,
                    QqRequestBuilder::playlistsUrl(categoryId, offset),
                    QqRequestKind::PublicMetadata,
                    false);
}

bool QqMusicService::requestToplists(quint64 operationId)
{
    Pending *pending = new Pending;
    pending->operationId = operationId;
    pending->kind = PendingKind::Toplists;
    return beginGet(pending,
                    QqRequestBuilder::toplistsUrl(),
                    QqRequestKind::PublicMetadata,
                    false);
}

bool QqMusicService::requestToplist(quint64 operationId,
                                    const QString &toplistId)
{
    bool converted = false;
    const int topId = toplistId.toInt(&converted, 10);
    if (!QqIdentifier::isNumericPlaylistId(toplistId)
            || !converted
            || topId <= 0) {
        return false;
    }

    Pending *pending = new Pending;
    pending->operationId = operationId;
    pending->kind = PendingKind::ToplistPeriod;
    pending->expectedId = toplistId;
    return beginGet(pending,
                    QqRequestBuilder::toplistPeriodUrl(),
                    QqRequestKind::PublicMetadata,
                    false);
}

bool QqMusicService::searchTracks(quint64 operationId,
                                  const QString &query,
                                  int page,
                                  const QString &accountUin)
{
    if (!accountUin.isEmpty()
            && !QqIdentifier::isNumericPlaylistId(accountUin)) {
        return false;
    }
    const bool accountBound = !accountUin.isEmpty();
    const QByteArray body = QqRequestBuilder::searchBody(
        query, SearchKind::Tracks, page, accountUin);
    if (body.isEmpty()) {
        return false;
    }
    Pending *pending = new Pending;
    pending->operationId = operationId;
    pending->kind = PendingKind::SearchTracks;
    pending->containsCookies = accountBound;
    pending->accountUin = accountUin;
    return beginPost(pending,
                     QUrl(QStringLiteral(
                         "https://u.y.qq.com/cgi-bin/musicu.fcg")),
                     accountBound ? QqRequestKind::AuthenticatedAccount
                                  : QqRequestKind::Search,
                     body,
                     accountBound);
}

bool QqMusicService::searchPlaylists(quint64 operationId,
                                     const QString &query,
                                     int page,
                                     const QString &accountUin)
{
    if (!accountUin.isEmpty()
            && !QqIdentifier::isNumericPlaylistId(accountUin)) {
        return false;
    }
    const bool accountBound = !accountUin.isEmpty();
    const QByteArray body = QqRequestBuilder::searchBody(
        query, SearchKind::Playlists, page, accountUin);
    if (body.isEmpty()) {
        return false;
    }
    Pending *pending = new Pending;
    pending->operationId = operationId;
    pending->kind = PendingKind::SearchPlaylists;
    pending->containsCookies = accountBound;
    pending->accountUin = accountUin;
    return beginPost(pending,
                     QUrl(QStringLiteral(
                         "https://u.y.qq.com/cgi-bin/musicu.fcg")),
                     accountBound ? QqRequestKind::AuthenticatedAccount
                                  : QqRequestKind::Search,
                     body,
                     accountBound);
}

bool QqMusicService::requestPlaylist(quint64 operationId,
                                     const QString &playlistId,
                                     bool containsCookies)
{
    Pending *pending = new Pending;
    pending->operationId = operationId;
    pending->kind = PendingKind::Playlist;
    pending->expectedId = playlistId;
    pending->containsCookies = containsCookies;
    return beginGet(pending,
                    QqRequestBuilder::playlistUrl(playlistId),
                    containsCookies ? QqRequestKind::AuthenticatedAccount
                                    : QqRequestKind::PublicMetadata,
                    containsCookies);
}

bool QqMusicService::requestUser(quint64 operationId, const QString &uin)
{
    Pending *pending = new Pending;
    pending->operationId = operationId;
    pending->kind = PendingKind::User;
    pending->expectedId = uin;
    pending->containsCookies = true;
    return beginGet(pending,
                    QqRequestBuilder::userUrl(uin),
                    QqRequestKind::AuthenticatedAccount,
                    true);
}

bool QqMusicService::requestCreatedLibrary(quint64 operationId,
                                           const QString &uin)
{
    Pending *pending = new Pending;
    pending->operationId = operationId;
    pending->kind = PendingKind::LibraryCreated;
    pending->expectedId = uin;
    pending->containsCookies = true;
    return beginGet(pending,
                    QqRequestBuilder::userCreatedUrl(uin),
                    QqRequestKind::AuthenticatedAccount,
                    true);
}

bool QqMusicService::requestCollectedLibrary(quint64 operationId,
                                             const QString &uin)
{
    Pending *pending = new Pending;
    pending->operationId = operationId;
    pending->kind = PendingKind::LibraryCollected;
    pending->expectedId = uin;
    pending->containsCookies = true;
    return beginGet(pending,
                    QqRequestBuilder::userCollectedUrl(uin),
                    QqRequestKind::AuthenticatedAccount,
                    true);
}

bool QqMusicService::requestLyrics(quint64 operationId,
                                   const QString &trackMid)
{
    Pending *pending = new Pending;
    pending->operationId = operationId;
    pending->kind = PendingKind::Lyrics;
    pending->expectedId = trackMid;
    return beginGet(pending,
                    QqRequestBuilder::lyricUrl(trackMid),
                    QqRequestKind::Lyric,
                    false);
}

bool QqMusicService::resolvePlayback(quint64 operationId,
                                     const QString &trackMid,
                                     const QString &accountUin)
{
    const QByteArray body = QqRequestBuilder::vkeyBody(trackMid, accountUin);
    if (body.isEmpty()) {
        return false;
    }
    const bool containsCookies = !accountUin.isEmpty();
    Pending *pending = new Pending;
    pending->operationId = operationId;
    pending->kind = PendingKind::Playback;
    pending->expectedId = trackMid;
    pending->containsCookies = containsCookies;
    return beginPost(pending,
                     QqRequestBuilder::vkeyUrl(),
                     QqRequestKind::Vkey,
                     body,
                     containsCookies);
}

bool QqMusicService::cancel(quint64 operationId)
{
    return m_pending.contains(operationId)
        && m_transport->cancel(operationId);
}

QqMusicService::Pending *QqMusicService::takePending(quint64 operationId)
{
    return m_pending.take(operationId);
}

void QqMusicService::emitInternalFailure(
    quint64 operationId, const QString &diagnosticCode)
{
    ServiceError error;
    error.kind = ErrorKind::Internal;
    error.operationId = operationId;
    error.diagnosticCode = diagnosticCode;
    emit failed(error);
}

void QqMusicService::complete(quint64 operationId, const QByteArray &body)
{
    Pending *pending = m_pending.value(operationId, 0);
    if (!pending) {
        return;
    }

    if (pending->kind == PendingKind::ToplistPeriod) {
        bool converted = false;
        const int topId = pending->expectedId.toInt(&converted, 10);
        const ServiceResult<QString> period = converted
            ? QqPublicParser::toplistPeriod(body, topId, operationId)
            : ServiceResult<QString>();
        if (!converted || !period.ok()) {
            takePending(operationId);
            delete pending;
            if (converted) {
                emit failed(period.error);
            } else {
                emitInternalFailure(
                    operationId,
                    QStringLiteral("qq.public.toplist.id.internal"));
            }
            return;
        }
        pending->period = period.value;
        pending->kind = PendingKind::ToplistDetail;
        const bool started = m_transport->get(
            operationId,
            QqRequestBuilder::toplistUrl(topId, pending->period, 100),
            QqRequestKind::PublicMetadata,
            false);
        if (!started && m_pending.value(operationId) == pending) {
            takePending(operationId);
            delete pending;
            emitInternalFailure(operationId,
                                QStringLiteral("qq.public.toplist.start"));
        }
        return;
    }

    if (pending->kind == PendingKind::DailyThirty
            || pending->kind == PendingKind::GuessYouLike) {
        const bool daily = pending->kind == PendingKind::DailyThirty;
        const ServiceResult<QVector<Track> > result = daily
            ? QqPublicParser::dailyThirtyTracks(body, operationId)
            : QqPublicParser::guessYouLikeTracks(body, operationId);
        if (!result.ok()) {
            takePending(operationId);
            delete pending;
            emit failed(result.error);
            return;
        }
        const int maximum = daily ? 30 : 20;
        appendUniqueTracks(&pending->personalizedTracks,
                           &pending->personalizedTrackMids,
                           result.value,
                           maximum);
        const bool mayContinue = pending->personalizedPage < 4
            && pending->personalizedTracks.size() < maximum
            && !result.value.isEmpty();
        if (mayContinue) {
            ++pending->personalizedPage;
            const QByteArray nextBody = daily
                ? QqRequestBuilder::dailyThirtyBody(
                      pending->accountUin,
                      pending->authenticationKey,
                      pending->personalizedPage)
                : QqRequestBuilder::guessYouLikeBody(
                      pending->accountUin,
                      pending->authenticationKey);
            const bool started = !nextBody.isEmpty() && m_transport->post(
                operationId,
                QqRequestBuilder::personalizedUrl(),
                QqRequestKind::AuthenticatedAccount,
                nextBody,
                true);
            if (!started && m_pending.value(operationId) == pending) {
                takePending(operationId);
                delete pending;
                emitInternalFailure(
                    operationId,
                    daily ? QStringLiteral("qq.personalized.daily30.start")
                          : QStringLiteral("qq.personalized.guess.start"));
            }
            return;
        }
        takePending(operationId);
        const QVector<Track> items = pending->personalizedTracks;
        delete pending;
        if (daily) {
            emit dailyThirtyReady(operationId, items);
        } else {
            emit guessYouLikeReady(operationId, items);
        }
        return;
    }

    pending = takePending(operationId);
    if (!pending) {
        return;
    }
    const PendingKind kind = pending->kind;
    const QString expectedId = pending->expectedId;
    const QString period = pending->period;
    const bool containsCookies = pending->containsCookies;
    delete pending;

    switch (kind) {
    case PendingKind::Recommendations: {
        const ServiceResult<QVector<PlaylistSummary> > result =
            QqPublicParser::recommendations(body, operationId);
        if (result.ok()) {
            emit recommendationsReady(operationId, result.value);
        } else {
            emit failed(result.error);
        }
        break;
    }
    case PendingKind::DailyThirty:
    case PendingKind::GuessYouLike:
        emitInternalFailure(
            operationId,
            QStringLiteral("qq.personalized.stage.invalid"));
        break;
    case PendingKind::Categories: {
        const ServiceResult<QVector<CategoryGroup> > result =
            QqPublicParser::categories(body, operationId);
        if (result.ok()) {
            emit categoriesReady(operationId, result.value);
        } else {
            emit failed(result.error);
        }
        break;
    }
    case PendingKind::Toplists: {
        const ServiceResult<QVector<PlaylistSummary> > result =
            QqPublicParser::toplists(body, operationId);
        if (result.ok()) {
            emit toplistsReady(operationId, result.value);
        } else {
            emit failed(result.error);
        }
        break;
    }
    case PendingKind::Playlists:
    case PendingKind::SearchPlaylists: {
        const ServiceResult<PlaylistPage> result =
            kind == PendingKind::Playlists
                ? QqPublicParser::playlists(body, operationId)
                : QqPublicParser::searchPlaylists(body, operationId);
        if (result.ok()) {
            emit playlistsReady(operationId,
                                result.value.items,
                                result.value.total);
        } else {
            emit failed(result.error);
        }
        break;
    }
    case PendingKind::SearchTracks: {
        const ServiceResult<TrackPage> result =
            QqPublicParser::searchTracks(body, operationId);
        if (result.ok()) {
            emit tracksReady(operationId,
                             result.value.items,
                             result.value.total);
        } else {
            emit failed(result.error);
        }
        break;
    }
    case PendingKind::Playlist: {
        const ServiceResult<PlaylistDetail> result =
            QqLibraryParser::playlist(body, expectedId, operationId);
        if (result.ok()) {
            emit playlistReady(operationId, result.value);
        } else {
            emit failed(result.error);
        }
        break;
    }
    case PendingKind::ToplistDetail: {
        bool converted = false;
        const int topId = expectedId.toInt(&converted, 10);
        if (!converted || topId <= 0) {
            emitInternalFailure(
                operationId,
                QStringLiteral("qq.public.toplist.id.internal"));
            break;
        }
        ServiceResult<PlaylistDetail> result =
            QqPublicParser::toplistDetail(body, topId, operationId);
        if (result.ok()) {
            result.value.period = period;
            emit playlistReady(operationId, result.value);
        } else {
            emit failed(result.error);
        }
        break;
    }
    case PendingKind::User: {
        const ServiceResult<UserProfile> result =
            QqLibraryParser::user(body, expectedId, operationId);
        if (result.ok()) {
            emit userReady(operationId, result.value);
        } else {
            emit failed(result.error);
        }
        break;
    }
    case PendingKind::LibraryCollected: {
        const ServiceResult<UserLibrary> result =
            QqLibraryParser::userCollected(body, operationId);
        if (result.ok()) {
            emit collectedLibraryReady(operationId,
                                       result.value.collected);
        } else {
            emit failed(result.error);
        }
        break;
    }
    case PendingKind::Lyrics: {
        const ServiceResult<LyricDocument> result =
            QqLyricParser::parse(body, operationId);
        if (result.ok()) {
            emit lyricsReady(operationId, result.value);
        } else {
            emit failed(result.error);
        }
        break;
    }
    case PendingKind::Playback: {
        ServiceResult<PlaybackLocation> result = QqPlaybackParser::parse(
            body, expectedId, QDateTime::currentDateTimeUtc(), operationId);
        if (result.ok()) {
            result.value.accountBound = containsCookies;
            emit playbackReady(operationId, result.value);
        } else {
            emit failed(result.error);
        }
        break;
    }
    case PendingKind::LibraryCreated: {
        const ServiceResult<UserLibrary> result =
            QqLibraryParser::userCreated(body, operationId);
        if (result.ok()) {
            emit createdLibraryReady(operationId,
                                     result.value.liked,
                                     result.value.created);
        } else {
            emit failed(result.error);
        }
        break;
    }
    case PendingKind::ToplistPeriod:
        emitInternalFailure(operationId,
                            QStringLiteral("qq.public.toplist.stage.invalid"));
        break;
    }
}

void QqMusicService::transportFailed(const ServiceError &error)
{
    Pending *pending = takePending(error.operationId);
    if (!pending) {
        return;
    }
    delete pending;
    emit failed(error);
}
