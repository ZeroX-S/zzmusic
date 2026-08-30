#include <QtTest>

#include "service/qqmusicservice.h"
#include "service/qqrequestbuilder.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTimer>

static QByteArray fixture(const QString &name)
{
    const QByteArray path = (QStringLiteral("../fixtures/") + name).toUtf8();
    QFile file(QFINDTESTDATA(path.constData()));
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

static QByteArray filteredPlaylistsPayload()
{
    return QByteArrayLiteral(
        "{\"data\":{\"sum\":31,\"list\":[{"
        "\"dissid\":\"9451963602\","
        "\"dissname\":\"Filtered\","
        "\"imgurl\":\"https://y.gtimg.cn/music/photo_new/playlist.jpg\"}]}}"
    );
}

static QJsonObject personalizedTrack(const QString &mid,
                                     const QString &title)
{
    QJsonObject singer;
    singer.insert(QStringLiteral("mid"), QStringLiteral("001ARTISTMID01"));
    singer.insert(QStringLiteral("name"), QStringLiteral("Artist"));
    QJsonArray singers;
    singers.append(singer);
    QJsonObject album;
    album.insert(QStringLiteral("mid"), QStringLiteral("001ALBUMMID001"));
    album.insert(QStringLiteral("name"), QStringLiteral("Album"));
    QJsonObject track;
    track.insert(QStringLiteral("mid"), mid);
    track.insert(QStringLiteral("name"), title);
    track.insert(QStringLiteral("singer"), singers);
    track.insert(QStringLiteral("album"), album);
    track.insert(QStringLiteral("switch"), 2);
    return track;
}

static QByteArray dailyPayload(const QString &mid)
{
    QJsonObject item;
    item.insert(QStringLiteral("Track"),
                personalizedTrack(mid, QStringLiteral("Daily")));
    QJsonArray items;
    items.append(item);
    QJsonObject data;
    data.insert(QStringLiteral("VecSongs"), items);
    QJsonObject radar;
    radar.insert(QStringLiteral("data"), data);
    QJsonObject root;
    root.insert(QStringLiteral("radar"), radar);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

static QByteArray guessPayload(const QString &mid)
{
    QJsonArray items;
    items.append(personalizedTrack(mid, QStringLiteral("Guess")));
    QJsonObject data;
    data.insert(QStringLiteral("tracks"), items);
    QJsonObject radio;
    radio.insert(QStringLiteral("data"), data);
    QJsonObject root;
    root.insert(QStringLiteral("radio"), radio);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

class ServiceReply : public QNetworkReply
{
    Q_OBJECT

public:
    ServiceReply(const QNetworkRequest &request,
                 QNetworkAccessManager::Operation operation,
                 const QByteArray &body,
                 QNetworkReply::NetworkError error,
                 bool automaticDelivery,
                 QObject *parent)
        : QNetworkReply(parent)
        , m_body(body)
        , m_error(error)
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(operation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        if (automaticDelivery) {
            QTimer::singleShot(0, this, SLOT(deliver()));
        }
    }

    void abort() override
    {
        if (!isFinished()) {
            setError(QNetworkReply::OperationCanceledError,
                     QStringLiteral("cancelled"));
            setFinished(true);
            emit finished();
        }
    }

    qint64 bytesAvailable() const override
    {
        return m_available ? m_body.size() - m_offset : 0;
    }

protected:
    qint64 readData(char *data, qint64 maximum) override
    {
        if (!m_available || m_offset >= m_body.size()) {
            return -1;
        }
        const qint64 count = qMin(maximum, m_body.size() - m_offset);
        memcpy(data, m_body.constData() + m_offset, size_t(count));
        m_offset += count;
        return count;
    }

private slots:
    void deliver()
    {
        if (isFinished()) {
            return;
        }
        if (m_error != QNetworkReply::NoError) {
            setError(m_error, QStringLiteral("fixture network error"));
            setFinished(true);
            emit finished();
            return;
        }
        m_available = true;
        emit readyRead();
        setFinished(true);
        emit finished();
    }

private:
    QByteArray m_body;
    QNetworkReply::NetworkError m_error;
    qint64 m_offset = 0;
    bool m_available = false;
};

class ServiceNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT

public:
    QList<QByteArray> responses;
    QList<QNetworkRequest> requests;
    QList<Operation> operations;
    QList<QByteArray> bodies;
    QList<QNetworkReply::NetworkError> errors;
    QList<bool> automaticDeliveries;

protected:
    QNetworkReply *createRequest(Operation operation,
                                 const QNetworkRequest &request,
                                 QIODevice *outgoingData) override
    {
        requests.append(request);
        operations.append(operation);
        bodies.append(outgoingData ? outgoingData->readAll() : QByteArray());
        const QByteArray body = responses.isEmpty()
            ? QByteArrayLiteral("{}")
            : responses.takeFirst();
        const QNetworkReply::NetworkError error = errors.isEmpty()
            ? QNetworkReply::NoError
            : errors.takeFirst();
        const bool automaticDelivery = automaticDeliveries.isEmpty()
            ? true
            : automaticDeliveries.takeFirst();
        return new ServiceReply(request, operation, body, error,
                                automaticDelivery, this);
    }
};

class QqMusicServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void emitsAuthenticatedPersonalizedTrackSurfaces()
    {
        ServiceNetworkAccessManager manager;
        QList<QNetworkCookie> cookies;
        QNetworkCookie identity(QByteArrayLiteral("uin"),
                                QByteArrayLiteral("o1234567890"));
        identity.setSecure(true);
        QNetworkCookie key(QByteArrayLiteral("qm_keyst"),
                           QByteArrayLiteral("sanitized-music-key"));
        key.setSecure(true);
        cookies << identity << key;
        QVERIFY(manager.cookieJar()->setCookiesFromUrl(
            cookies, QqRequestBuilder::personalizedUrl()));

        manager.responses
            << dailyPayload(QStringLiteral("001DAILYTRK001"))
            << dailyPayload(QStringLiteral("001DAILYTRK002"))
            << dailyPayload(QStringLiteral("001DAILYTRK003"))
            << dailyPayload(QStringLiteral("001DAILYTRK004"))
            << guessPayload(QStringLiteral("001GUESSLIK001"))
            << guessPayload(QStringLiteral("001GUESSLIK002"))
            << guessPayload(QStringLiteral("001GUESSLIK003"))
            << guessPayload(QStringLiteral("001GUESSLIK004"));
        QqMusicService service(&manager);
        QSignalSpy daily(
            &service, SIGNAL(dailyThirtyReady(quint64,QVector<Track>)));
        QSignalSpy guess(
            &service, SIGNAL(guessYouLikeReady(quint64,QVector<Track>)));
        QSignalSpy failed(&service, SIGNAL(failed(ServiceError)));

        QVERIFY(service.requestDailyThirty(7));
        QTRY_COMPARE(daily.count(), 1);
        QCOMPARE(daily.at(0).at(0).toULongLong(), quint64(7));
        QCOMPARE(daily.at(0).at(1).value<QVector<Track> >().size(), 4);
        QCOMPARE(manager.requests.size(), 4);
        for (int index = 0; index < 4; ++index) {
            QCOMPARE(manager.operations.at(index),
                     QNetworkAccessManager::PostOperation);
            QCOMPARE(manager.requests.at(index).attribute(
                         QNetworkRequest::CookieLoadControlAttribute).toInt(),
                     int(QNetworkRequest::Automatic));
            QCOMPARE(manager.requests.at(index).rawHeader("Content-Type"),
                     QByteArrayLiteral("application/json"));
            const QJsonObject body = QJsonDocument::fromJson(
                manager.bodies.at(index)).object();
            QCOMPARE(body.value(QStringLiteral("radar")).toObject()
                         .value(QStringLiteral("param")).toObject()
                         .value(QStringLiteral("Page")).toInt(), index + 1);
        }

        QVERIFY(service.requestGuessYouLike(8));
        QTRY_COMPARE(guess.count(), 1);
        QCOMPARE(guess.at(0).at(0).toULongLong(), quint64(8));
        QCOMPARE(guess.at(0).at(1).value<QVector<Track> >().size(), 4);
        QCOMPARE(manager.requests.size(), 8);
        QCOMPARE(failed.count(), 0);
    }

    void emitsTypedPublicAndSearchResults()
    {
        ServiceNetworkAccessManager manager;
        manager.responses << fixture("recommendations.json")
                          << fixture("search-tracks.json");
        QqMusicService service(&manager);
        QSignalSpy recommendations(
            &service,
            SIGNAL(recommendationsReady(quint64,QVector<PlaylistSummary>)));
        QSignalSpy tracks(
            &service,
            SIGNAL(tracksReady(quint64,QVector<Track>,qint64)));
        QSignalSpy failed(&service, SIGNAL(failed(ServiceError)));

        QVERIFY(service.requestRecommendations(10));
        QTRY_COMPARE(recommendations.count(), 1);
        QCOMPARE(recommendations.at(0).at(0).toULongLong(), quint64(10));
        QCOMPARE(recommendations.at(0).at(1)
                     .value<QVector<PlaylistSummary> >().size(), 1);
        QCOMPARE(manager.requests.at(0).attribute(
                     QNetworkRequest::CacheLoadControlAttribute).toInt(),
                 int(QNetworkRequest::AlwaysNetwork));
        QCOMPARE(manager.requests.at(0).attribute(
                     QNetworkRequest::CacheSaveControlAttribute).toBool(),
                 false);

        QVERIFY(service.searchTracks(11, QStringLiteral("query"), 2));
        QTRY_COMPARE(tracks.count(), 1);
        QCOMPARE(tracks.at(0).at(0).toULongLong(), quint64(11));
        QVERIFY(!tracks.at(0).at(1).value<QVector<Track> >().isEmpty());
        QCOMPARE(manager.operations.at(1), QNetworkAccessManager::PostOperation);
        QVERIFY(!manager.bodies.at(1).isEmpty());
        QCOMPARE(failed.count(), 0);
    }

    void bindsAuthenticatedSearchCookiesAndRequestIdentity()
    {
        ServiceNetworkAccessManager manager;
        QNetworkCookie identity(QByteArrayLiteral("uin"),
                                QByteArrayLiteral("o1234567890"));
        identity.setSecure(true);
        QVERIFY(manager.cookieJar()->setCookiesFromUrl(
            QList<QNetworkCookie>() << identity,
            QUrl(QStringLiteral("https://u.y.qq.com/"))));
        manager.responses << fixture("search-tracks.json");
        QqMusicService service(&manager);
        QSignalSpy tracks(
            &service,
            SIGNAL(tracksReady(quint64,QVector<Track>,qint64)));
        QSignalSpy failed(&service, SIGNAL(failed(ServiceError)));

        QVERIFY(service.searchTracks(
            12, QStringLiteral("query"), 1,
            QStringLiteral("1234567890")));
        QTRY_COMPARE(tracks.count(), 1);
        QCOMPARE(manager.requests.size(), 1);
        QCOMPARE(manager.requests.first().attribute(
                     QNetworkRequest::CookieLoadControlAttribute).toInt(),
                 int(QNetworkRequest::Automatic));
        QCOMPARE(manager.requests.first().attribute(
                     QNetworkRequest::CookieSaveControlAttribute).toInt(),
                 int(QNetworkRequest::Automatic));
        const QJsonObject requestBody = QJsonDocument::fromJson(
            manager.bodies.first()).object();
        QCOMPARE(requestBody.value(QStringLiteral("comm")).toObject()
                     .value(QStringLiteral("uin")).toString(),
                 QStringLiteral("1234567890"));
        QCOMPARE(failed.count(), 0);
    }

    void loadsUserLibraryHalvesIndependently()
    {
        ServiceNetworkAccessManager manager;
        manager.responses << fixture("user-created.json")
                          << fixture("user-collected.json");
        QqMusicService service(&manager);
        QSignalSpy created(
            &service,
            SIGNAL(createdLibraryReady(quint64,QVector<PlaylistSummary>,QVector<PlaylistSummary>)));
        QSignalSpy collected(
            &service,
            SIGNAL(collectedLibraryReady(quint64,QVector<PlaylistSummary>)));
        QSignalSpy failed(&service, SIGNAL(failed(ServiceError)));

        QVERIFY(service.requestCreatedLibrary(20, QStringLiteral("10001")));
        QVERIFY(service.requestCollectedLibrary(21, QStringLiteral("10001")));
        QTRY_COMPARE(created.count(), 1);
        QTRY_COMPARE(collected.count(), 1);
        QCOMPARE(created.at(0).at(0).toULongLong(), quint64(20));
        QCOMPARE(created.at(0).at(1)
                     .value<QVector<PlaylistSummary> >().size(), 1);
        QCOMPARE(created.at(0).at(2)
                     .value<QVector<PlaylistSummary> >().size(), 1);
        QCOMPARE(collected.at(0).at(0).toULongLong(), quint64(21));
        QCOMPARE(collected.at(0).at(1)
                     .value<QVector<PlaylistSummary> >().size(), 1);
        QCOMPARE(manager.requests.count(), 2);
        for (int index = 0; index < manager.requests.size(); ++index) {
            QCOMPARE(manager.requests.at(index).attribute(
                         QNetworkRequest::CookieLoadControlAttribute).toInt(),
                     int(QNetworkRequest::Automatic));
        }
        QCOMPARE(failed.count(), 0);
    }

    void mapsExpiredAccountLibraryApplicationCodes()
    {
        ServiceNetworkAccessManager manager;
        manager.responses << QByteArrayLiteral("{\"code\":5000}")
                          << QByteArrayLiteral("{\"code\":4000}");
        QqMusicService service(&manager);
        QSignalSpy created(
            &service,
            SIGNAL(createdLibraryReady(quint64,QVector<PlaylistSummary>,QVector<PlaylistSummary>)));
        QSignalSpy collected(
            &service,
            SIGNAL(collectedLibraryReady(quint64,QVector<PlaylistSummary>)));
        QSignalSpy failed(&service, SIGNAL(failed(ServiceError)));

        QVERIFY(service.requestCreatedLibrary(22, QStringLiteral("10001")));
        QVERIFY(service.requestCollectedLibrary(23, QStringLiteral("10001")));
        QTRY_COMPARE(failed.count(), 2);
        QCOMPARE(created.count(), 0);
        QCOMPARE(collected.count(), 0);

        const ServiceError createdError =
            failed.at(0).at(0).value<ServiceError>();
        QCOMPARE(createdError.operationId, quint64(22));
        QCOMPARE(createdError.kind, ErrorKind::AuthenticationExpired);
        QCOMPARE(createdError.diagnosticCode,
                 QStringLiteral("qq.library.created.code.5000"));

        const ServiceError collectedError =
            failed.at(1).at(0).value<ServiceError>();
        QCOMPARE(collectedError.operationId, quint64(23));
        QCOMPARE(collectedError.kind, ErrorKind::AuthenticationExpired);
        QCOMPARE(collectedError.diagnosticCode,
                 QStringLiteral("qq.library.collected.code.4000"));
    }

    void emitsPlaylistLyricAndPlaybackResults()
    {
        ServiceNetworkAccessManager manager;
        manager.responses << fixture("playlist.json")
                          << fixture("lyrics.json")
                          << fixture("vkey.json");
        QqMusicService service(&manager);
        QSignalSpy playlists(&service,
                             SIGNAL(playlistReady(quint64,PlaylistDetail)));
        QSignalSpy lyrics(&service,
                          SIGNAL(lyricsReady(quint64,LyricDocument)));
        QSignalSpy playback(&service,
                            SIGNAL(playbackReady(quint64,PlaybackLocation)));

        QVERIFY(service.requestPlaylist(30, QStringLiteral("9451963602")));
        QTRY_COMPARE(playlists.count(), 1);
        QVERIFY(service.requestLyrics(31, QStringLiteral("001SANITIZED01")));
        QTRY_COMPARE(lyrics.count(), 1);
        const QString playbackMid = QStringLiteral("001SANITIZED01");
        const QString accountUin = QStringLiteral("1234567890");
        QVERIFY(service.resolvePlayback(32, playbackMid, accountUin));
        QTRY_COMPARE(playback.count(), 1);
        QCOMPARE(manager.bodies.last(),
                 QqRequestBuilder::vkeyBody(playbackMid, accountUin));
        QCOMPARE(playback.at(0).at(1)
                     .value<PlaybackLocation>().bitrate,
                 QStringLiteral("128 kbps"));
        QCOMPARE(playback.at(0).at(1)
                     .value<PlaybackLocation>().accountBound,
                 true);
    }

    void rejectsPlaybackResponseBoundToAnotherTrack()
    {
        ServiceNetworkAccessManager manager;
        manager.responses << QByteArrayLiteral(
            "{\"req_1\":{\"data\":{\"midurlinfo\":[{"
            "\"filename\":\"M500002SANITIZED02002SANITIZED02.mp3\","
            "\"purl\":\"M500002SANITIZED02.mp3?vkey=not-a-live-token\""
            "}],\"sip\":[\"https://ws.stream.qqmusic.qq.com/\"]}}}");
        QqMusicService service(&manager);
        QSignalSpy playback(&service,
                            SIGNAL(playbackReady(quint64,PlaybackLocation)));
        QSignalSpy failed(&service, SIGNAL(failed(ServiceError)));
        const QString requestedMid = QStringLiteral("001SANITIZED01");

        QVERIFY(service.resolvePlayback(33, requestedMid));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(playback.count(), 0);
        QCOMPARE(manager.bodies.count(), 1);
        QCOMPARE(manager.bodies.first(),
                 QqRequestBuilder::vkeyBody(requestedMid));

        const ServiceError error = failed.at(0).at(0).value<ServiceError>();
        QCOMPARE(error.operationId, quint64(33));
        QCOMPARE(error.kind, ErrorKind::SchemaChanged);
        QCOMPARE(error.diagnosticCode,
                 QStringLiteral("qq.playback.filename.invalid"));
        QCOMPARE(failed.count(), 1);
        QCOMPARE(playback.count(), 0);
    }

    void preservesOperationIdOnSchemaFailure()
    {
        ServiceNetworkAccessManager manager;
        manager.responses << QByteArrayLiteral("{}");
        QqMusicService service(&manager);
        QSignalSpy failed(&service, SIGNAL(failed(ServiceError)));

        QVERIFY(service.requestRecommendations(40));
        QTRY_COMPARE(failed.count(), 1);
        const ServiceError error = failed.at(0).at(0).value<ServiceError>();
        QCOMPARE(error.operationId, quint64(40));
        QCOMPARE(error.kind, ErrorKind::SchemaChanged);
    }

    void emitsRemainingTypedReadOnlyResults()
    {
        ServiceNetworkAccessManager manager;
        manager.responses << fixture("categories.json")
                          << filteredPlaylistsPayload()
                          << fixture("search-playlists.json")
                          << fixture("user.json");
        QqMusicService service(&manager);
        QSignalSpy categories(
            &service,
            SIGNAL(categoriesReady(quint64,QVector<CategoryGroup>)));
        QSignalSpy playlists(
            &service,
            SIGNAL(playlistsReady(quint64,QVector<PlaylistSummary>,qint64)));
        QSignalSpy user(&service,
                        SIGNAL(userReady(quint64,UserProfile)));
        QSignalSpy failed(&service, SIGNAL(failed(ServiceError)));

        QVERIFY(service.requestCategories(41));
        QTRY_COMPARE(categories.count(), 1);
        QCOMPARE(categories.at(0).at(0).toULongLong(), quint64(41));

        QVERIFY(service.requestPlaylists(42, QStringLiteral("165"), 0));
        QTRY_COMPARE(playlists.count(), 1);
        QCOMPARE(playlists.at(0).at(0).toULongLong(), quint64(42));
        QCOMPARE(playlists.at(0).at(2).toLongLong(), qint64(31));

        QVERIFY(service.searchPlaylists(43, QStringLiteral("query"), 1));
        QTRY_COMPARE(playlists.count(), 2);
        QCOMPARE(playlists.at(1).at(0).toULongLong(), quint64(43));

        QVERIFY(service.requestUser(44, QStringLiteral("1000123456")));
        QTRY_COMPARE(user.count(), 1);
        QCOMPARE(user.at(0).at(0).toULongLong(), quint64(44));
        QCOMPARE(user.at(0).at(1).value<UserProfile>().uin,
                 QStringLiteral("1000123456"));
        QCOMPARE(failed.count(), 0);
    }

    void loadsToplistSummariesAndLatestDetailAsTypedOperations()
    {
        ServiceNetworkAccessManager manager;
        manager.responses << fixture("toplists.json")
                          << fixture("toplist-period.html")
                          << fixture("toplist.json");
        QqMusicService service(&manager);
        QSignalSpy toplists(
            &service,
            SIGNAL(toplistsReady(quint64,QVector<PlaylistSummary>)));
        QSignalSpy playlist(
            &service,
            SIGNAL(playlistReady(quint64,PlaylistDetail)));
        QSignalSpy failed(&service, SIGNAL(failed(ServiceError)));

        QVERIFY(service.requestToplists(45));
        QTRY_COMPARE(toplists.count(), 1);
        QCOMPARE(toplists.at(0).at(0).toULongLong(), quint64(45));
        QCOMPARE(toplists.at(0).at(1)
                     .value<QVector<PlaylistSummary> >().size(), 1);
        QCOMPARE(manager.requests.count(), 1);
        QCOMPARE(manager.requests.at(0).url(),
                 QqRequestBuilder::toplistsUrl());

        QVERIFY(service.requestToplist(46, QStringLiteral("26")));
        QTRY_COMPARE(playlist.count(), 1);
        QCOMPARE(playlist.at(0).at(0).toULongLong(), quint64(46));
        QCOMPARE(playlist.at(0).at(1).value<PlaylistDetail>().info.id,
                 QStringLiteral("26"));
        QCOMPARE(playlist.at(0).at(1).value<PlaylistDetail>().period,
                 QStringLiteral("2026-07-24"));
        QCOMPARE(manager.requests.count(), 3);
        QCOMPARE(manager.requests.at(1).url(),
                 QqRequestBuilder::toplistPeriodUrl());
        QCOMPARE(manager.requests.at(2).url(),
                 QqRequestBuilder::toplistUrl(
                     26, QStringLiteral("2026-07-24"), 100));
        QCOMPARE(failed.count(), 0);

        QVERIFY(!service.requestToplist(47, QStringLiteral("0")));
        QVERIFY(!service.requestToplist(48, QStringLiteral("+26")));
        QVERIFY(!service.requestToplist(
            49, QStringLiteral("2147483648")));
        QCOMPARE(manager.requests.count(), 3);
    }

    void releasesToplistOperationAfterPeriodParseFailure()
    {
        ServiceNetworkAccessManager manager;
        manager.responses << QByteArrayLiteral("<html></html>")
                          << fixture("toplist-period.html")
                          << fixture("toplist.json");
        QqMusicService service(&manager);
        QSignalSpy playlist(
            &service,
            SIGNAL(playlistReady(quint64,PlaylistDetail)));
        QSignalSpy failed(&service, SIGNAL(failed(ServiceError)));

        QVERIFY(service.requestToplist(47, QStringLiteral("26")));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(playlist.count(), 0);
        QCOMPARE(manager.requests.count(), 1);
        const ServiceError parseError =
            failed.at(0).at(0).value<ServiceError>();
        QCOMPARE(parseError.operationId, quint64(47));
        QCOMPARE(parseError.kind, ErrorKind::SchemaChanged);
        QCOMPARE(parseError.diagnosticCode,
                 QStringLiteral("qq.public.toplistPeriod.missing"));

        QVERIFY(service.requestToplist(47, QStringLiteral("26")));
        QTRY_COMPARE(playlist.count(), 1);
        QCOMPARE(playlist.at(0).at(0).toULongLong(), quint64(47));
        QCOMPARE(manager.requests.count(), 3);
        QCOMPARE(failed.count(), 1);
    }

    void releasesToplistOperationAfterSecondStageNetworkFailure()
    {
        ServiceNetworkAccessManager manager;
        manager.responses << fixture("toplist-period.html")
                          << QByteArray()
                          << fixture("toplist-period.html")
                          << fixture("toplist.json");
        manager.errors << QNetworkReply::NoError
                       << QNetworkReply::TimeoutError
                       << QNetworkReply::NoError
                       << QNetworkReply::NoError;
        QqMusicService service(&manager);
        QSignalSpy playlist(
            &service,
            SIGNAL(playlistReady(quint64,PlaylistDetail)));
        QSignalSpy failed(&service, SIGNAL(failed(ServiceError)));

        QVERIFY(service.requestToplist(48, QStringLiteral("26")));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(playlist.count(), 0);
        QCOMPARE(manager.requests.count(), 2);
        const ServiceError networkError =
            failed.at(0).at(0).value<ServiceError>();
        QCOMPARE(networkError.operationId, quint64(48));
        QCOMPARE(networkError.kind, ErrorKind::Offline);

        QVERIFY(service.requestToplist(48, QStringLiteral("26")));
        QTRY_COMPARE(playlist.count(), 1);
        QCOMPARE(playlist.at(0).at(0).toULongLong(), quint64(48));
        QCOMPARE(manager.requests.count(), 4);
        QCOMPARE(failed.count(), 1);
    }

    void rejectsDuplicateAndReusesCancelledToplistDuringSecondStage()
    {
        ServiceNetworkAccessManager manager;
        manager.responses << fixture("toplist-period.html")
                          << fixture("toplist.json")
                          << fixture("toplist-period.html")
                          << fixture("toplist.json");
        manager.automaticDeliveries << true << false << true << true;
        QqMusicService service(&manager);
        QSignalSpy playlist(
            &service,
            SIGNAL(playlistReady(quint64,PlaylistDetail)));
        QSignalSpy failed(&service, SIGNAL(failed(ServiceError)));

        QVERIFY(service.requestToplist(49, QStringLiteral("26")));
        QTRY_COMPARE(manager.requests.count(), 2);
        QVERIFY(!service.requestRecommendations(49));
        QCOMPARE(manager.requests.count(), 2);

        QVERIFY(service.cancel(49));
        QCOMPARE(failed.count(), 1);
        QCOMPARE(playlist.count(), 0);
        const ServiceError cancelled =
            failed.at(0).at(0).value<ServiceError>();
        QCOMPARE(cancelled.operationId, quint64(49));
        QCOMPARE(cancelled.kind, ErrorKind::Cancelled);
        QVERIFY(!service.cancel(49));

        QVERIFY(service.requestToplist(49, QStringLiteral("26")));
        QTRY_COMPARE(playlist.count(), 1);
        QCOMPARE(playlist.at(0).at(0).toULongLong(), quint64(49));
        QCOMPARE(manager.requests.count(), 4);
        QCOMPARE(failed.count(), 1);
    }

    void rejectsDuplicateOperationsAndSupportsCancellation()
    {
        ServiceNetworkAccessManager manager;
        manager.responses << fixture("recommendations.json")
                          << fixture("recommendations.json");
        QqMusicService service(&manager);
        QSignalSpy recommendations(
            &service,
            SIGNAL(recommendationsReady(quint64,QVector<PlaylistSummary>)));
        QSignalSpy failed(&service, SIGNAL(failed(ServiceError)));

        QVERIFY(!service.requestRecommendations(0));
        QCOMPARE(manager.requests.count(), 0);
        QVERIFY(service.requestRecommendations(50));
        QVERIFY(!service.searchTracks(50, QStringLiteral("duplicate"), 1));
        QCOMPARE(manager.requests.count(), 1);

        QVERIFY(service.cancel(50));
        QCOMPARE(failed.count(), 1);
        const ServiceError cancelled =
            failed.at(0).at(0).value<ServiceError>();
        QCOMPARE(cancelled.operationId, quint64(50));
        QCOMPARE(cancelled.kind, ErrorKind::Cancelled);
        QVERIFY(!service.cancel(50));

        QVERIFY(service.requestRecommendations(50));
        QTRY_COMPARE(recommendations.count(), 1);
        QCOMPARE(recommendations.at(0).at(0).toULongLong(), quint64(50));
    }

    void preservesCreatedLibraryWhenCollectedRequestFails()
    {
        ServiceNetworkAccessManager manager;
        manager.responses << fixture("user-created.json") << QByteArray();
        manager.errors << QNetworkReply::NoError
                       << QNetworkReply::TimeoutError;
        QqMusicService service(&manager);
        QSignalSpy created(
            &service,
            SIGNAL(createdLibraryReady(quint64,QVector<PlaylistSummary>,QVector<PlaylistSummary>)));
        QSignalSpy collected(
            &service,
            SIGNAL(collectedLibraryReady(quint64,QVector<PlaylistSummary>)));
        QSignalSpy failed(&service, SIGNAL(failed(ServiceError)));

        QVERIFY(service.requestCreatedLibrary(51, QStringLiteral("10001")));
        QVERIFY(service.requestCollectedLibrary(52, QStringLiteral("10001")));
        QTRY_COMPARE(created.count(), 1);
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(collected.count(), 0);
        QCOMPARE(manager.requests.count(), 2);
        const ServiceError error = failed.at(0).at(0).value<ServiceError>();
        QCOMPARE(error.operationId, quint64(52));
        QCOMPARE(error.kind, ErrorKind::Offline);
    }

    void preservesCollectedLibraryWhenCreatedRequestFails()
    {
        ServiceNetworkAccessManager manager;
        manager.responses << QByteArray() << fixture("user-collected.json");
        manager.errors << QNetworkReply::TimeoutError
                       << QNetworkReply::NoError;
        QqMusicService service(&manager);
        QSignalSpy created(
            &service,
            SIGNAL(createdLibraryReady(quint64,QVector<PlaylistSummary>,QVector<PlaylistSummary>)));
        QSignalSpy collected(
            &service,
            SIGNAL(collectedLibraryReady(quint64,QVector<PlaylistSummary>)));
        QSignalSpy failed(&service, SIGNAL(failed(ServiceError)));

        QVERIFY(service.requestCreatedLibrary(53, QStringLiteral("10001")));
        QVERIFY(service.requestCollectedLibrary(54, QStringLiteral("10001")));
        QTRY_COMPARE(collected.count(), 1);
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(created.count(), 0);
        QCOMPARE(collected.at(0).at(0).toULongLong(), quint64(54));
        QCOMPARE(collected.at(0).at(1)
                     .value<QVector<PlaylistSummary> >().size(), 1);
        QCOMPARE(manager.requests.count(), 2);
        const ServiceError error = failed.at(0).at(0).value<ServiceError>();
        QCOMPARE(error.operationId, quint64(53));
        QCOMPARE(error.kind, ErrorKind::Offline);
    }
};

QTEST_GUILESS_MAIN(QqMusicServiceTest)
#include "tst_qqmusicservice.moc"
