#include <QtTest>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QUrlQuery>

#include <algorithm>
#include <limits>

#include "core/qqidentifier.h"
#include "service/qqpublicparser.h"
#include "service/qqrequestbuilder.h"
#include "service/qqrequestpolicy.h"

namespace {

QByteArray fixture(const QString &name)
{
    const QByteArray relativePath =
        (QStringLiteral("../fixtures/") + name).toUtf8();
    QFile file(QFINDTESTDATA(relativePath.constData()));
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

QString queryValue(const QUrl &url, const QString &name)
{
    return QUrlQuery(url).queryItemValue(name, QUrl::FullyDecoded);
}

QJsonObject queryJson(const QUrl &url, const QString &name)
{
    return QJsonDocument::fromJson(queryValue(url, name).toUtf8()).object();
}

QJsonObject bodyJson(const QByteArray &body)
{
    return QJsonDocument::fromJson(body).object();
}

typedef QPair<QString, QString> QueryItem;

bool queryItemLessThan(const QueryItem &left, const QueryItem &right)
{
    return left.first < right.first
        || (left.first == right.first && left.second < right.second);
}

QList<QueryItem> decodedQuery(
    const QUrl &url,
    const QStringList &excluded = QStringList())
{
    QList<QueryItem> result;
    const QList<QueryItem> items =
        QUrlQuery(url).queryItems(QUrl::FullyDecoded);
    for (int index = 0; index < items.size(); ++index) {
        if (!excluded.contains(items.at(index).first)) {
            result.append(items.at(index));
        }
    }
    std::sort(result.begin(), result.end(), queryItemLessThan);
    return result;
}

QList<QueryItem> expectedQueryItems(const QMap<QString, QString> &items)
{
    QList<QueryItem> result;
    QMap<QString, QString>::const_iterator iterator = items.constBegin();
    for (; iterator != items.constEnd(); ++iterator) {
        result.append(QueryItem(iterator.key(), iterator.value()));
    }
    return result;
}

int queryItemCount(const QUrl &url, const QString &name)
{
    const QList<QueryItem> items =
        QUrlQuery(url).queryItems(QUrl::FullyDecoded);
    int count = 0;
    for (int index = 0; index < items.size(); ++index) {
        if (items.at(index).first == name) {
            ++count;
        }
    }
    return count;
}

QMap<QString, QString> expectedMusicuQuery()
{
    QMap<QString, QString> result;
    result.insert(QStringLiteral("format"), QStringLiteral("json"));
    result.insert(QStringLiteral("inCharset"), QStringLiteral("utf8"));
    result.insert(QStringLiteral("outCharset"), QStringLiteral("utf-8"));
    result.insert(QStringLiteral("platform"), QStringLiteral("yqq.json"));
    result.insert(QStringLiteral("needNewCode"), QStringLiteral("0"));
    return result;
}

QMap<QString, QString> expectedPublicCatalogQuery()
{
    QMap<QString, QString> result;
    result.insert(QStringLiteral("picmid"), QStringLiteral("1"));
    result.insert(QStringLiteral("g_tk"), QStringLiteral("732560869"));
    result.insert(QStringLiteral("loginUin"), QStringLiteral("0"));
    result.insert(QStringLiteral("hostUin"), QStringLiteral("0"));
    result.insert(QStringLiteral("format"), QStringLiteral("json"));
    result.insert(QStringLiteral("inCharset"), QStringLiteral("utf8"));
    result.insert(QStringLiteral("outCharset"), QStringLiteral("utf-8"));
    result.insert(QStringLiteral("notice"), QStringLiteral("0"));
    result.insert(QStringLiteral("platform"), QStringLiteral("yqq.json"));
    result.insert(QStringLiteral("needNewCode"), QStringLiteral("0"));
    return result;
}

QJsonObject expectedRecommendationRequest()
{
    QJsonObject comm;
    comm.insert(QStringLiteral("ct"), 24);

    QJsonObject param;
    param.insert(QStringLiteral("async"), 1);
    param.insert(QStringLiteral("cmd"), 2);

    QJsonObject recommendation;
    recommendation.insert(QStringLiteral("method"),
                          QStringLiteral("get_hot_recommend"));
    recommendation.insert(QStringLiteral("param"), param);
    recommendation.insert(QStringLiteral("module"),
                          QStringLiteral("playlist.HotRecommendServer"));

    QJsonObject request;
    request.insert(QStringLiteral("comm"), comm);
    request.insert(QStringLiteral("recomPlaylist"), recommendation);
    return request;
}

QJsonObject expectedToplistRequest(int limit)
{
    QJsonObject comm;
    comm.insert(QStringLiteral("cv"), 1602);
    comm.insert(QStringLiteral("ct"), 20);

    QJsonObject param;
    param.insert(QStringLiteral("topid"), 26);
    param.insert(QStringLiteral("num"), limit);
    param.insert(QStringLiteral("period"), QStringLiteral("2026-07-24"));

    QJsonObject toplist;
    toplist.insert(QStringLiteral("module"),
                   QStringLiteral("musicToplist.ToplistInfoServer"));
    toplist.insert(QStringLiteral("method"), QStringLiteral("GetDetail"));
    toplist.insert(QStringLiteral("param"), param);

    QJsonObject request;
    request.insert(QStringLiteral("comm"), comm);
    request.insert(QStringLiteral("toplist"), toplist);
    return request;
}

QJsonObject expectedSearchRequest(int searchType, int page)
{
    QJsonObject comm;
    comm.insert(QStringLiteral("ct"), QStringLiteral("19"));
    comm.insert(QStringLiteral("cv"), QStringLiteral("1859"));
    comm.insert(QStringLiteral("uin"), QStringLiteral("0"));

    QJsonObject param;
    param.insert(QStringLiteral("grp"), 1);
    param.insert(QStringLiteral("num_per_page"), 50);
    param.insert(QStringLiteral("page_num"), page);
    param.insert(QStringLiteral("query"), QStringLiteral("keyword"));
    param.insert(QStringLiteral("search_type"), searchType);

    QJsonObject requestBlock;
    requestBlock.insert(QStringLiteral("method"),
                        QStringLiteral("DoSearchForQQMusicDesktop"));
    requestBlock.insert(QStringLiteral("module"),
                        QStringLiteral("music.search.SearchCgiService"));
    requestBlock.insert(QStringLiteral("param"), param);

    QJsonObject request;
    request.insert(QStringLiteral("comm"), comm);
    request.insert(QStringLiteral("req"), requestBlock);
    return request;
}

QJsonObject searchParam(const QByteArray &body)
{
    return bodyJson(body).value(QStringLiteral("req")).toObject()
        .value(QStringLiteral("param")).toObject();
}

QByteArray filteredPlaylistsPayload()
{
    return QByteArrayLiteral(
        "{\"data\":{\"sum\":31,\"list\":[{"
        "\"dissid\":\"9451963602\","
        "\"dissname\":\"专注 &amp; 工作\","
        "\"imgurl\":\"https://y.gtimg.cn/music/photo_new/playlist.jpg\"}]}}"
    );
}

QJsonObject searchTrackFixture(int index)
{
    const QJsonObject root = QJsonDocument::fromJson(
        fixture(QStringLiteral("search-tracks.json"))).object();
    return root.value(QStringLiteral("req")).toObject()
        .value(QStringLiteral("data")).toObject()
        .value(QStringLiteral("body")).toObject()
        .value(QStringLiteral("song")).toObject()
        .value(QStringLiteral("list")).toArray()
        .at(index).toObject();
}

QJsonObject personalizedTrack(const QString &mid,
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

QByteArray withSearchTrack(int index, const QJsonObject &track)
{
    QJsonObject root = QJsonDocument::fromJson(
        fixture(QStringLiteral("search-tracks.json"))).object();
    QJsonObject req = root.value(QStringLiteral("req")).toObject();
    QJsonObject data = req.value(QStringLiteral("data")).toObject();
    QJsonObject body = data.value(QStringLiteral("body")).toObject();
    QJsonObject song = body.value(QStringLiteral("song")).toObject();
    QJsonArray tracks = song.value(QStringLiteral("list")).toArray();
    tracks.replace(index, track);
    song.insert(QStringLiteral("list"), tracks);
    body.insert(QStringLiteral("song"), song);
    data.insert(QStringLiteral("body"), body);
    req.insert(QStringLiteral("data"), data);
    root.insert(QStringLiteral("req"), req);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

}

class QqPublicParserTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsPinnedPublicUrls();
    void addsPerCallCategoryCacheBusters();
    void addsPerCallPlaylistCacheBusters();
    void clampsPlaylistAndToplistBounds();
    void mapsSearchKindsAndPagination();
    void buildsAuthenticatedPersonalizedRequests();
    void clampsSearchQueryByUnicodeCodePoint();
    void parsesRecommendations();
    void acceptsLiveRecommendationFieldsSafely();
    void rejectsChangedRecommendationSchemas();
    void parsesUsableCategoryGroups();
    void parsesFilteredPlaylistPages();
    void convertsBothSongShapesAndSwitchFlags();
    void parsesPersonalizedTrackSurfaces();
    void acceptsAlbumlessSearchTracks();
    void rejectsInconsistentSearchAlbumMetadata();
    void usesModernActionSwitchFallback();
    void preservesTopLevelSwitchPrecedence();
    void rejectsInvalidModernActionSwitch();
    void prefersLegacySongShapeWhenModernKeysCollide();
    void acceptsMissingSingerMid();
    void rejectsWrongTypedOptionalSingerMid();
    void parsesPlaylistSearchResults();
    void toleratesMixedLivePlaylistSearchArtworkShapes();
    void preservesValidEmptySearchPages();
    void rejectsInvalidSongFieldsWithoutCoercion();
    void parsesToplistSummaries();
    void validatesToplistIdIntBoundary();
    void extractsPeriodOnlyFromPinnedChartButtons();
    void parsesToplistDetail();
};

void QqPublicParserTest::buildsAuthenticatedPersonalizedRequests()
{
    const QString uin = QStringLiteral("1234567890");
    const QByteArray key = QByteArrayLiteral("sanitized-music-key");
    QCOMPARE(QqRequestBuilder::personalizedUrl(),
             QUrl(QStringLiteral("https://u.y.qq.com/cgi-bin/musicu.fcg")));

    const QJsonObject daily = bodyJson(
        QqRequestBuilder::dailyThirtyBody(uin, key, 2));
    QCOMPARE(daily.value(QStringLiteral("comm")).toObject()
                 .value(QStringLiteral("uin")).toString(), uin);
    QCOMPARE(daily.value(QStringLiteral("comm")).toObject()
                 .value(QStringLiteral("ct")).toInt(), 19);
    QCOMPARE(daily.value(QStringLiteral("comm")).toObject()
                 .value(QStringLiteral("authst")).toString(),
             QString::fromLatin1(key));
    const QJsonObject radar = daily.value(QStringLiteral("radar")).toObject();
    QCOMPARE(radar.value(QStringLiteral("module")).toString(),
             QStringLiteral("music.recommend.TrackRelationServer"));
    QCOMPARE(radar.value(QStringLiteral("method")).toString(),
             QStringLiteral("GetRadarSong"));
    QCOMPARE(radar.value(QStringLiteral("param")).toObject()
                 .value(QStringLiteral("Page")).toInt(), 2);

    const QJsonObject guess = bodyJson(
        QqRequestBuilder::guessYouLikeBody(uin, key));
    const QJsonObject radio = guess.value(QStringLiteral("radio")).toObject();
    QCOMPARE(radio.value(QStringLiteral("module")).toString(),
             QStringLiteral("music.radioProxy.MbTrackRadioSvr"));
    QCOMPARE(radio.value(QStringLiteral("method")).toString(),
             QStringLiteral("get_radio_track"));
    QVERIFY(radio.value(QStringLiteral("param")).toObject().isEmpty());

    QVERIFY(QqRequestBuilder::dailyThirtyBody(QString(), key, 1).isEmpty());
    QVERIFY(QqRequestBuilder::dailyThirtyBody(uin, QByteArray(), 1).isEmpty());
    QVERIFY(QqRequestBuilder::dailyThirtyBody(uin, key, 0).isEmpty());
}

void QqPublicParserTest::parsesPersonalizedTrackSurfaces()
{
    const QJsonObject first = personalizedTrack(
        QStringLiteral("001DAILYTRACK1"), QStringLiteral("Daily One"));
    QJsonObject wrapped;
    wrapped.insert(QStringLiteral("Track"), first);
    QJsonArray dailyItems;
    dailyItems.append(wrapped);
    QJsonObject dailyData;
    dailyData.insert(QStringLiteral("VecSongs"), dailyItems);
    QJsonObject dailyBlock;
    dailyBlock.insert(QStringLiteral("data"), dailyData);
    QJsonObject dailyRoot;
    dailyRoot.insert(QStringLiteral("radar"), dailyBlock);
    const ServiceResult<QVector<Track> > daily =
        QqPublicParser::dailyThirtyTracks(
            QJsonDocument(dailyRoot).toJson(QJsonDocument::Compact), 71);
    QVERIFY(daily.ok());
    QCOMPARE(daily.value.size(), 1);
    QCOMPARE(daily.value.first().mid, QStringLiteral("001DAILYTRACK1"));

    QJsonArray guessItems;
    guessItems.append(personalizedTrack(
        QStringLiteral("001GUESSTRACK1"), QStringLiteral("Guess One")));
    QJsonObject guessData;
    guessData.insert(QStringLiteral("tracks"), guessItems);
    QJsonObject guessBlock;
    guessBlock.insert(QStringLiteral("data"), guessData);
    QJsonObject guessRoot;
    guessRoot.insert(QStringLiteral("radio"), guessBlock);
    const ServiceResult<QVector<Track> > guess =
        QqPublicParser::guessYouLikeTracks(
            QJsonDocument(guessRoot).toJson(QJsonDocument::Compact), 72);
    QVERIFY(guess.ok());
    QCOMPARE(guess.value.size(), 1);
    QCOMPARE(guess.value.first().mid, QStringLiteral("001GUESSTRACK1"));

    dailyRoot.insert(QStringLiteral("radar"), QJsonObject());
    const ServiceResult<QVector<Track> > invalid =
        QqPublicParser::dailyThirtyTracks(
            QJsonDocument(dailyRoot).toJson(QJsonDocument::Compact), 73);
    QVERIFY(!invalid.ok());
    QCOMPARE(invalid.error.operationId, quint64(73));
}

void QqPublicParserTest::buildsPinnedPublicUrls()
{
    const QUrl recommendations = QqRequestBuilder::recommendationsUrl();
    QCOMPARE(recommendations.scheme(), QStringLiteral("https"));
    QCOMPARE(recommendations.host(), QStringLiteral("u.y.qq.com"));
    QCOMPARE(recommendations.path(), QStringLiteral("/cgi-bin/musicu.fcg"));
    QMap<QString, QString> recommendationQuery = expectedMusicuQuery();
    recommendationQuery.insert(QStringLiteral("loginUin"), QStringLiteral("0"));
    recommendationQuery.insert(QStringLiteral("hostUin"), QStringLiteral("0"));
    QCOMPARE(decodedQuery(
                 recommendations, QStringList() << QStringLiteral("data")),
             expectedQueryItems(recommendationQuery));
    QCOMPARE(queryJson(recommendations, QStringLiteral("data")),
             expectedRecommendationRequest());

    const QUrl categories = QqRequestBuilder::categoriesUrl();
    QCOMPARE(categories.scheme(), QStringLiteral("https"));
    QCOMPARE(categories.host(), QStringLiteral("c.y.qq.com"));
    QCOMPARE(categories.path(),
             QStringLiteral("/splcloud/fcgi-bin/fcg_get_diss_tag_conf.fcg"));
    QCOMPARE(decodedQuery(
                 categories, QStringList() << QStringLiteral("rnd")),
             expectedQueryItems(expectedPublicCatalogQuery()));
    QCOMPARE(queryItemCount(categories, QStringLiteral("rnd")), 1);

    const QUrl playlists =
        QqRequestBuilder::playlistsUrl(QStringLiteral("165"), 30);
    QCOMPARE(playlists.scheme(), QStringLiteral("https"));
    QCOMPARE(playlists.host(), QStringLiteral("c.y.qq.com"));
    QCOMPARE(playlists.path(),
             QStringLiteral("/splcloud/fcgi-bin/fcg_get_diss_by_tag.fcg"));
    QMap<QString, QString> playlistQuery = expectedPublicCatalogQuery();
    playlistQuery.insert(QStringLiteral("categoryId"), QStringLiteral("165"));
    playlistQuery.insert(QStringLiteral("sortId"), QStringLiteral("5"));
    playlistQuery.insert(QStringLiteral("sin"), QStringLiteral("30"));
    playlistQuery.insert(QStringLiteral("ein"), QStringLiteral("59"));
    QCOMPARE(decodedQuery(
                 playlists, QStringList() << QStringLiteral("rnd")),
             expectedQueryItems(playlistQuery));
    QCOMPARE(queryItemCount(playlists, QStringLiteral("rnd")), 1);

    const QUrl toplists = QqRequestBuilder::toplistsUrl();
    QCOMPARE(toplists.scheme(), QStringLiteral("https"));
    QCOMPARE(toplists.host(), QStringLiteral("c.y.qq.com"));
    QCOMPARE(toplists.path(), QStringLiteral("/v8/fcg-bin/fcg_myqq_toplist.fcg"));
    QMap<QString, QString> toplistQuery;
    toplistQuery.insert(QStringLiteral("g_tk"), QStringLiteral("5381"));
    toplistQuery.insert(QStringLiteral("inCharset"), QStringLiteral("utf-8"));
    toplistQuery.insert(QStringLiteral("outCharset"), QStringLiteral("utf-8"));
    toplistQuery.insert(QStringLiteral("notice"), QStringLiteral("0"));
    toplistQuery.insert(QStringLiteral("format"), QStringLiteral("json"));
    toplistQuery.insert(QStringLiteral("uin"), QStringLiteral("0"));
    toplistQuery.insert(QStringLiteral("needNewCode"), QStringLiteral("1"));
    toplistQuery.insert(QStringLiteral("platform"), QStringLiteral("h5"));
    QCOMPARE(decodedQuery(toplists), expectedQueryItems(toplistQuery));

    const QUrl period = QqRequestBuilder::toplistPeriodUrl();
    QCOMPARE(period.toString(),
             QStringLiteral("https://c.y.qq.com/node/pc/wk_v15/top.html"));

    QCOMPARE(QqRequestBuilder::recommendationsUrl(), recommendations);
    QCOMPARE(QqRequestBuilder::toplistsUrl(), toplists);
    QCOMPARE(QqRequestBuilder::toplistPeriodUrl(), period);
}

void QqPublicParserTest::addsPerCallCategoryCacheBusters()
{
    QSet<QString> categoryValues;

    for (int index = 0; index < 8; ++index) {
        const QString categoryValue = queryValue(
            QqRequestBuilder::categoriesUrl(), QStringLiteral("rnd"));
        bool categoryOk = false;
        const double categoryNumber = categoryValue.toDouble(&categoryOk);
        QVERIFY(categoryOk);
        QVERIFY(categoryNumber >= 0.0);
        QVERIFY(categoryNumber < 1.0);
        categoryValues.insert(categoryValue);
    }

    QVERIFY(categoryValues.size() > 1);
}

void QqPublicParserTest::addsPerCallPlaylistCacheBusters()
{
    QSet<QString> playlistValues;

    for (int index = 0; index < 8; ++index) {
        const QString playlistValue = queryValue(
            QqRequestBuilder::playlistsUrl(QStringLiteral("165"), 30),
            QStringLiteral("rnd"));
        bool playlistOk = false;
        const double playlistNumber = playlistValue.toDouble(&playlistOk);
        QVERIFY(playlistOk);
        QVERIFY(playlistNumber >= 0.0);
        QVERIFY(playlistNumber < 1.0);
        playlistValues.insert(playlistValue);
    }

    QVERIFY(playlistValues.size() > 1);
}

void QqPublicParserTest::clampsPlaylistAndToplistBounds()
{
    const QUrl low = QqRequestBuilder::playlistsUrl(QStringLiteral("165"), -1);
    QCOMPARE(queryValue(low, QStringLiteral("sin")), QStringLiteral("0"));
    QCOMPARE(queryValue(low, QStringLiteral("ein")), QStringLiteral("29"));

    const QUrl high = QqRequestBuilder::playlistsUrl(QStringLiteral("165"), 9000);
    QCOMPARE(queryValue(high, QStringLiteral("sin")), QStringLiteral("5970"));
    QCOMPARE(queryValue(high, QStringLiteral("ein")), QStringLiteral("5999"));

    const QUrl detail =
        QqRequestBuilder::toplistUrl(26, QStringLiteral("2026-07-24"), 500);
    QCOMPARE(detail.scheme(), QStringLiteral("https"));
    QCOMPARE(detail.host(), QStringLiteral("u.y.qq.com"));
    QCOMPARE(detail.path(), QStringLiteral("/cgi-bin/musicu.fcg"));
    QCOMPARE(decodedQuery(detail, QStringList() << QStringLiteral("data")),
             expectedQueryItems(expectedMusicuQuery()));
    QCOMPARE(queryJson(detail, QStringLiteral("data")),
             expectedToplistRequest(100));
    QCOMPARE(QqRequestBuilder::toplistUrl(
                 26, QStringLiteral("2026-07-24"), 500),
             detail);
}

void QqPublicParserTest::mapsSearchKindsAndPagination()
{
    const QByteArray tracks = QqRequestBuilder::searchBody(
        QStringLiteral("keyword"), SearchKind::Tracks, 0);
    QCOMPARE(bodyJson(tracks), expectedSearchRequest(0, 1));

    const QByteArray playlists = QqRequestBuilder::searchBody(
        QStringLiteral("keyword"), SearchKind::Playlists, 201);
    QCOMPARE(bodyJson(playlists), expectedSearchRequest(3, 200));
    QCOMPARE(QqRequestBuilder::searchBody(
                 QStringLiteral("keyword"), SearchKind::Playlists, 201),
             playlists);

    const QJsonObject accountSearch = bodyJson(
        QqRequestBuilder::searchBody(
            QStringLiteral("keyword"), SearchKind::Tracks, 1,
            QStringLiteral("1234567890")));
    QCOMPARE(accountSearch.value(QStringLiteral("comm")).toObject()
                 .value(QStringLiteral("uin")).toString(),
             QStringLiteral("1234567890"));
    QVERIFY(QqRequestBuilder::searchBody(
                QStringLiteral("keyword"), SearchKind::Tracks, 1,
                QStringLiteral("invalid-account")).isEmpty());
}

void QqPublicParserTest::clampsSearchQueryByUnicodeCodePoint()
{
    const uint codePoint = 0x1f600;
    const QString emoji = QString::fromUcs4(&codePoint, 1);
    const QString query = QString(127, QLatin1Char('a')) + emoji
        + QStringLiteral("discarded");
    const QString clamped = searchParam(QqRequestBuilder::searchBody(
        query, SearchKind::Tracks, 1)).value(QStringLiteral("query")).toString();
    QCOMPARE(clamped, QString(127, QLatin1Char('a')) + emoji);
    QCOMPARE(clamped.size(), 129);

    const QString boundary(128, QLatin1Char('b'));
    QCOMPARE(searchParam(QqRequestBuilder::searchBody(
                 boundary, SearchKind::Tracks, 1))
                 .value(QStringLiteral("query")).toString(),
             boundary);
}

void QqPublicParserTest::parsesRecommendations()
{
    const ServiceResult<QVector<PlaylistSummary> > result =
        QqPublicParser::recommendations(
            fixture(QStringLiteral("recommendations.json")), 7);
    QVERIFY(result.ok());
    QCOMPARE(result.value.size(), 1);
    QCOMPARE(result.value.first().id, QStringLiteral("9451963602"));
    QCOMPARE(result.value.first().title, QString::fromUtf8("华语流行"));
    QCOMPARE(result.value.first().artworkUrl.toString(),
             QStringLiteral("https://y.gtimg.cn/music/photo_new/"
                            "T002R300x300M000fixture.jpg"));
    QCOMPARE(result.value.first().sourceUrl.toString(),
             QStringLiteral("https://y.qq.com/n/ryqq/playlist/9451963602"));
    QVERIFY(QqIdentifier::isSafeSourceUrl(result.value.first().sourceUrl));
}

void QqPublicParserTest::acceptsLiveRecommendationFieldsSafely()
{
    const QByteArray liveShape = QByteArrayLiteral(
        "{\"recomPlaylist\":{\"data\":{\"v_hot\":[{"
        "\"content_id\":9451963602,"
        "\"title\":\"Live response shape\","
        "\"cover\":\"http://y.gtimg.cn/music/photo_new/live.jpg\"}]}}}");
    const ServiceResult<QVector<PlaylistSummary> > result =
        QqPublicParser::recommendations(liveShape, 73);

    QVERIFY(result.ok());
    QCOMPARE(result.value.size(), 1);
    QCOMPARE(result.value.first().id, QStringLiteral("9451963602"));
    QCOMPARE(result.value.first().artworkUrl.toString(),
             QStringLiteral("https://y.gtimg.cn/music/photo_new/live.jpg"));

    const QByteArray unreviewedArtwork = QByteArrayLiteral(
        "{\"recomPlaylist\":{\"data\":{\"v_hot\":[{"
        "\"content_id\":9451963602,"
        "\"title\":\"Unreviewed artwork\","
        "\"cover\":\"http://example.invalid/live.jpg\"}]}}}");
    const ServiceResult<QVector<PlaylistSummary> > rejected =
        QqPublicParser::recommendations(unreviewedArtwork, 74);
    QVERIFY(!rejected.ok());
    QCOMPARE(rejected.error.diagnosticCode,
             QStringLiteral("qq.public.recommendation.artwork.invalid"));
}

void QqPublicParserTest::rejectsChangedRecommendationSchemas()
{
    const ServiceResult<QVector<PlaylistSummary> > missing =
        QqPublicParser::recommendations(
            QByteArrayLiteral("{\"recomPlaylist\":{\"data\":{}}}"), 71);
    QVERIFY(!missing.ok());
    QCOMPARE(missing.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(missing.error.operationId, quint64(71));
    QCOMPARE(missing.error.diagnosticCode,
             QStringLiteral("json.recomPlaylist.data.v_hot.array"));

    QJsonObject root = QJsonDocument::fromJson(
        fixture(QStringLiteral("recommendations.json"))).object();
    QJsonObject recommendation = root.value(QStringLiteral("recomPlaylist")).toObject();
    QJsonObject data = recommendation.value(QStringLiteral("data")).toObject();
    QJsonArray items = data.value(QStringLiteral("v_hot")).toArray();
    QJsonObject item = items.first().toObject();
    item.insert(QStringLiteral("title"), 42);
    items.replace(0, item);
    data.insert(QStringLiteral("v_hot"), items);
    recommendation.insert(QStringLiteral("data"), data);
    root.insert(QStringLiteral("recomPlaylist"), recommendation);

    const ServiceResult<QVector<PlaylistSummary> > wrongTitle =
        QqPublicParser::recommendations(
            QJsonDocument(root).toJson(QJsonDocument::Compact), 72);
    QVERIFY(!wrongTitle.ok());
    QCOMPARE(wrongTitle.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(wrongTitle.error.operationId, quint64(72));
    QCOMPARE(wrongTitle.error.diagnosticCode,
             QStringLiteral("qq.public.recommendation.title.string"));
    QVERIFY(!wrongTitle.error.diagnosticCode.contains(QStringLiteral("42")));
}

void QqPublicParserTest::parsesUsableCategoryGroups()
{
    const ServiceResult<QVector<CategoryGroup> > result =
        QqPublicParser::categories(
            fixture(QStringLiteral("categories.json")), 8);
    QVERIFY(result.ok());
    QCOMPARE(result.value.size(), 2);
    QCOMPARE(result.value.at(0).title, QString::fromUtf8("语种 & 风格"));
    QCOMPARE(result.value.at(0).items.size(), 2);
    QCOMPARE(result.value.at(0).items.at(0).id, QStringLiteral("165"));
    QCOMPARE(result.value.at(0).items.at(0).title,
             QString::fromUtf8("华语 & 流行"));
    QCOMPARE(result.value.at(1).title, QString::fromUtf8("场景"));
    QVERIFY(result.value.at(1).items.isEmpty());
}

void QqPublicParserTest::parsesFilteredPlaylistPages()
{
    const ServiceResult<PlaylistPage> result =
        QqPublicParser::playlists(filteredPlaylistsPayload(), 9);
    QVERIFY(result.ok());
    QCOMPARE(result.value.total, qint64(31));
    QCOMPARE(result.value.items.size(), 1);
    QCOMPARE(result.value.items.first().id, QStringLiteral("9451963602"));
    QCOMPARE(result.value.items.first().title,
             QString::fromUtf8("专注 & 工作"));
    QCOMPARE(result.value.items.first().sourceUrl.toString(),
             QStringLiteral("https://y.qq.com/n/ryqq/playlist/9451963602"));
    QVERIFY(QqIdentifier::isSafeSourceUrl(result.value.items.first().sourceUrl));
}

void QqPublicParserTest::convertsBothSongShapesAndSwitchFlags()
{
    const ServiceResult<TrackPage> result = QqPublicParser::searchTracks(
        fixture(QStringLiteral("search-tracks.json")), 10);
    QVERIFY(result.ok());
    QCOMPARE(result.value.total, qint64(72));
    QCOMPARE(result.value.items.size(), 2);

    const Track modern = result.value.items.at(0);
    QCOMPARE(modern.mid, QStringLiteral("003OUlho2HcRHC"));
    QCOMPARE(modern.title, QString::fromUtf8("晨光 & 海"));
    QCOMPARE(modern.artist, QString::fromUtf8("歌手 \"甲\""));
    QCOMPARE(modern.artistMid, QStringLiteral("002ABCdef23456"));
    QCOMPARE(modern.album, QString::fromUtf8("专辑 & 一"));
    QCOMPARE(modern.artworkUrl.toString(),
             QStringLiteral("https://y.gtimg.cn/music/photo_new/"
                            "T002R300x300M000000ABCdef23456.jpg"));
    QCOMPARE(modern.sourceUrl.toString(),
             QStringLiteral("https://y.qq.com/n/ryqq/songDetail/003OUlho2HcRHC"));
    QVERIFY(modern.playable);

    const Track legacy = result.value.items.at(1);
    QCOMPARE(legacy.mid, QStringLiteral("001ABCdef23456"));
    QCOMPARE(legacy.title, QString::fromUtf8("旧版歌曲"));
    QCOMPARE(legacy.albumMid, QStringLiteral("005ABCdef23456"));
    QVERIFY(!legacy.playable);
    QCOMPARE(legacy.sourceUrl.toString(),
             QStringLiteral("https://y.qq.com/n/ryqq/songDetail/001ABCdef23456"));
    QVERIFY(QqIdentifier::isSafeSourceUrl(legacy.sourceUrl));
}

void QqPublicParserTest::acceptsAlbumlessSearchTracks()
{
    QJsonObject modern = searchTrackFixture(0);
    QJsonObject modernAlbum = modern.value(QStringLiteral("album")).toObject();
    modernAlbum.insert(QStringLiteral("mid"), QString());
    modernAlbum.insert(QStringLiteral("name"), QString());
    modern.insert(QStringLiteral("album"), modernAlbum);

    QJsonObject legacy = searchTrackFixture(1);
    legacy.insert(QStringLiteral("albummid"), QString());
    legacy.insert(QStringLiteral("albumname"), QString());

    QJsonObject root = QJsonDocument::fromJson(
        withSearchTrack(0, modern)).object();
    QJsonObject req = root.value(QStringLiteral("req")).toObject();
    QJsonObject data = req.value(QStringLiteral("data")).toObject();
    QJsonObject body = data.value(QStringLiteral("body")).toObject();
    QJsonObject song = body.value(QStringLiteral("song")).toObject();
    QJsonArray tracks = song.value(QStringLiteral("list")).toArray();
    tracks.replace(1, legacy);
    song.insert(QStringLiteral("list"), tracks);
    body.insert(QStringLiteral("song"), song);
    data.insert(QStringLiteral("body"), body);
    req.insert(QStringLiteral("data"), data);
    root.insert(QStringLiteral("req"), req);

    const ServiceResult<TrackPage> result = QqPublicParser::searchTracks(
        QJsonDocument(root).toJson(QJsonDocument::Compact), 1001);

    QVERIFY(result.ok());
    QCOMPARE(result.value.items.size(), 2);
    for (int index = 0; index < result.value.items.size(); ++index) {
        QVERIFY(result.value.items.at(index).albumMid.isEmpty());
        QVERIFY(result.value.items.at(index).album.isEmpty());
        QVERIFY(result.value.items.at(index).artworkUrl.isEmpty());
    }
}

void QqPublicParserTest::rejectsInconsistentSearchAlbumMetadata()
{
    QJsonObject missingMid = searchTrackFixture(0);
    QJsonObject album = missingMid.value(QStringLiteral("album")).toObject();
    album.insert(QStringLiteral("mid"), QString());
    missingMid.insert(QStringLiteral("album"), album);
    const ServiceResult<TrackPage> midResult = QqPublicParser::searchTracks(
        withSearchTrack(0, missingMid), 1002);
    QVERIFY(!midResult.ok());
    QCOMPARE(midResult.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(midResult.error.diagnosticCode,
             QStringLiteral("qq.public.track.albumMid.invalid"));

    QJsonObject missingName = searchTrackFixture(0);
    album = missingName.value(QStringLiteral("album")).toObject();
    album.insert(QStringLiteral("name"), QString());
    missingName.insert(QStringLiteral("album"), album);
    const ServiceResult<TrackPage> nameResult = QqPublicParser::searchTracks(
        withSearchTrack(0, missingName), 1003);
    QVERIFY(!nameResult.ok());
    QCOMPARE(nameResult.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(nameResult.error.diagnosticCode,
             QStringLiteral("qq.public.track.album.empty"));
}

void QqPublicParserTest::usesModernActionSwitchFallback()
{
    QJsonObject action;
    action.insert(QStringLiteral("switch"), 0);
    QJsonObject modern = searchTrackFixture(0);
    modern.insert(QStringLiteral("action"), action);
    ServiceResult<TrackPage> result = QqPublicParser::searchTracks(
        withSearchTrack(0, modern), 104);
    QVERIFY(result.ok());
    QVERIFY(!result.value.items.first().playable);

    action.insert(QStringLiteral("switch"), 2);
    modern.remove(QStringLiteral("switch"));
    modern.insert(QStringLiteral("action"), action);
    result = QqPublicParser::searchTracks(
        withSearchTrack(0, modern), 105);
    QVERIFY(result.ok());
    QVERIFY(result.value.items.first().playable);

    modern.insert(QStringLiteral("action"), QJsonValue(QJsonValue::Null));
    result = QqPublicParser::searchTracks(
        withSearchTrack(0, modern), 110);
    QVERIFY(result.ok());
    QVERIFY(result.value.items.first().playable);

    const int liveScaleSwitch = 17413891;
    QVERIFY((liveScaleSwitch & 2) != 0);
    action.insert(QStringLiteral("switch"), liveScaleSwitch);
    modern.insert(QStringLiteral("action"), action);
    result = QqPublicParser::searchTracks(
        withSearchTrack(0, modern), 111);
    QVERIFY(result.ok());
    QVERIFY(result.value.items.first().playable);
}

void QqPublicParserTest::preservesTopLevelSwitchPrecedence()
{
    QJsonObject action;
    action.insert(QStringLiteral("switch"), 0);
    QJsonObject modern = searchTrackFixture(0);
    modern.insert(QStringLiteral("switch"), 2);
    modern.insert(QStringLiteral("action"), action);
    ServiceResult<TrackPage> result = QqPublicParser::searchTracks(
        withSearchTrack(0, modern), 106);
    QVERIFY(result.ok());
    QVERIFY(result.value.items.first().playable);

    action.insert(QStringLiteral("switch"), 2);
    QJsonObject legacy = searchTrackFixture(1);
    legacy.insert(QStringLiteral("action"), action);
    result = QqPublicParser::searchTracks(
        withSearchTrack(1, legacy), 107);
    QVERIFY(result.ok());
    QVERIFY(!result.value.items.at(1).playable);
}

void QqPublicParserTest::rejectsInvalidModernActionSwitch()
{
    QJsonObject action;
    action.insert(QStringLiteral("switch"), QStringLiteral("2"));
    QJsonObject modern = searchTrackFixture(0);
    modern.insert(QStringLiteral("action"), action);
    ServiceResult<TrackPage> result = QqPublicParser::searchTracks(
        withSearchTrack(0, modern), 108);
    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(108));
    QCOMPARE(result.error.diagnosticCode,
             QStringLiteral("json.action.switch.integer"));

    action.insert(QStringLiteral("switch"), -1);
    modern.insert(QStringLiteral("action"), action);
    result = QqPublicParser::searchTracks(
        withSearchTrack(0, modern), 109);
    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(109));
    QCOMPARE(result.error.diagnosticCode,
             QStringLiteral("qq.public.track.switch.invalid"));

    modern.insert(QStringLiteral("action"), 2);
    result = QqPublicParser::searchTracks(
        withSearchTrack(0, modern), 112);
    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(112));
    QCOMPARE(result.error.diagnosticCode,
             QStringLiteral("json.action.switch.integer"));

    modern.insert(QStringLiteral("action"), QJsonObject());
    result = QqPublicParser::searchTracks(
        withSearchTrack(0, modern), 113);
    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(113));
    QCOMPARE(result.error.diagnosticCode,
             QStringLiteral("json.action.switch.integer"));

    action = QJsonObject();
    action.insert(QStringLiteral("switch"), QJsonValue(QJsonValue::Null));
    modern.insert(QStringLiteral("action"), action);
    result = QqPublicParser::searchTracks(
        withSearchTrack(0, modern), 114);
    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(114));
    QCOMPARE(result.error.diagnosticCode,
             QStringLiteral("json.action.switch.integer"));

    action.insert(QStringLiteral("switch"), 2.5);
    modern.insert(QStringLiteral("action"), action);
    result = QqPublicParser::searchTracks(
        withSearchTrack(0, modern), 115);
    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(115));
    QCOMPARE(result.error.diagnosticCode,
             QStringLiteral("json.action.switch.integer"));
}

void QqPublicParserTest::prefersLegacySongShapeWhenModernKeysCollide()
{
    const ServiceResult<TrackPage> result = QqPublicParser::searchTracks(
        fixture(QStringLiteral("search-track-legacy-collision.json")), 103);
    QVERIFY(result.ok());
    QCOMPARE(result.value.items.size(), 1);

    const Track track = result.value.items.first();
    QCOMPARE(track.mid, QStringLiteral("001ABCdef23456"));
    QCOMPARE(track.title, QString::fromUtf8("旧版主标题"));
    QCOMPARE(track.albumMid, QStringLiteral("005ABCdef23456"));
    QCOMPARE(track.album, QString::fromUtf8("旧版主专辑"));
    QCOMPARE(track.sourceUrl.toString(),
             QStringLiteral("https://y.qq.com/n/ryqq/songDetail/001ABCdef23456"));
}

void QqPublicParserTest::acceptsMissingSingerMid()
{
    const ServiceResult<TrackPage> result = QqPublicParser::searchTracks(
        fixture(QStringLiteral("search-track-optional-singer-mid.json")), 101);
    QVERIFY(result.ok());
    QCOMPARE(result.value.items.size(), 1);
    QCOMPARE(result.value.items.first().artist,
             QString::fromUtf8("无 MID 歌手"));
    QVERIFY(result.value.items.first().artistMid.isEmpty());
}

void QqPublicParserTest::rejectsWrongTypedOptionalSingerMid()
{
    QJsonObject root = QJsonDocument::fromJson(fixture(
        QStringLiteral("search-track-optional-singer-mid.json"))).object();
    QJsonObject req = root.value(QStringLiteral("req")).toObject();
    QJsonObject data = req.value(QStringLiteral("data")).toObject();
    QJsonObject body = data.value(QStringLiteral("body")).toObject();
    QJsonObject song = body.value(QStringLiteral("song")).toObject();
    QJsonArray list = song.value(QStringLiteral("list")).toArray();
    QJsonObject item = list.first().toObject();
    QJsonArray singers = item.value(QStringLiteral("singer")).toArray();
    QJsonObject singer = singers.first().toObject();
    singer.insert(QStringLiteral("mid"), 7);
    singers.replace(0, singer);
    item.insert(QStringLiteral("singer"), singers);
    list.replace(0, item);
    song.insert(QStringLiteral("list"), list);
    body.insert(QStringLiteral("song"), song);
    data.insert(QStringLiteral("body"), body);
    req.insert(QStringLiteral("data"), data);
    root.insert(QStringLiteral("req"), req);

    const ServiceResult<TrackPage> result = QqPublicParser::searchTracks(
        QJsonDocument(root).toJson(QJsonDocument::Compact), 102);
    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(102));
    QCOMPARE(result.error.diagnosticCode, QStringLiteral("json.mid.string"));
}

void QqPublicParserTest::parsesPlaylistSearchResults()
{
    const ServiceResult<PlaylistPage> result = QqPublicParser::searchPlaylists(
        fixture(QStringLiteral("search-playlists.json")), 11);
    QVERIFY(result.ok());
    QCOMPARE(result.value.total, qint64(9));
    QCOMPARE(result.value.items.size(), 1);
    QCOMPARE(result.value.items.first().title,
             QString::fromUtf8("通勤 & 放松"));
    QCOMPARE(result.value.items.first().author,
             QString::fromUtf8("测试 \"用户\""));
    QCOMPARE(result.value.items.first().trackCount, 24);
    QCOMPARE(result.value.items.first().sourceUrl.toString(),
             QStringLiteral("https://y.qq.com/n/ryqq/playlist/9451963602"));
    QVERIFY(QqIdentifier::isSafeSourceUrl(result.value.items.first().sourceUrl));
}

void QqPublicParserTest::toleratesMixedLivePlaylistSearchArtworkShapes()
{
    QJsonObject root = QJsonDocument::fromJson(
        fixture(QStringLiteral("search-playlists.json"))).object();
    QJsonObject req = root.value(QStringLiteral("req")).toObject();
    QJsonObject data = req.value(QStringLiteral("data")).toObject();
    QJsonObject body = data.value(QStringLiteral("body")).toObject();
    QJsonObject songlist = body.value(QStringLiteral("songlist")).toObject();
    const QJsonObject item = songlist.value(QStringLiteral("list"))
        .toArray().first().toObject();
    const auto withArtwork = [&item](const QString &id,
                                     const QString &artwork) -> QJsonObject {
        QJsonObject result = item;
        result.insert(QStringLiteral("dissid"), id);
        result.insert(QStringLiteral("dissname"), id);
        result.insert(QStringLiteral("imgurl"), artwork);
        return result;
    };
    QJsonArray items;
    items.append(withArtwork(QStringLiteral("9451963602"), QStringLiteral(
        "http://y.gtimg.cn/music/photo_new/fixture.jpg")));
    items.append(withArtwork(QStringLiteral("9451963603"), QStringLiteral(
        "http://qpic.y.qq.com/music_cover/fixture.jpg")));
    items.append(withArtwork(QStringLiteral("9451963604"), QStringLiteral(
        "https://music-file.y.qq.com/songlist/fixture.jpg")));
    items.append(withArtwork(QStringLiteral("9451963605"), QStringLiteral(
        "https://y.qq.com/music/photo/fixture.jpg")));
    items.append(withArtwork(QStringLiteral("9451963606"), QStringLiteral(
        "http://i.gtimg.cn/music/photo/fixture.jpg")));
    items.append(withArtwork(QStringLiteral("9451963607"), QString()));
    items.append(withArtwork(QStringLiteral("9451963608"), QStringLiteral(
        "https://y.qq.com.evil.invalid/fixture.jpg")));
    songlist.insert(QStringLiteral("list"), items);
    body.insert(QStringLiteral("songlist"), songlist);
    data.insert(QStringLiteral("body"), body);
    req.insert(QStringLiteral("data"), data);
    root.insert(QStringLiteral("req"), req);

    const ServiceResult<PlaylistPage> result = QqPublicParser::searchPlaylists(
        QJsonDocument(root).toJson(QJsonDocument::Compact), 103);

    QVERIFY(result.ok());
    QCOMPARE(result.value.items.size(), 7);
    QCOMPARE(result.value.items.at(0).artworkUrl.toString(), QStringLiteral(
        "https://y.gtimg.cn/music/photo_new/fixture.jpg"));
    QCOMPARE(result.value.items.at(1).artworkUrl.toString(), QStringLiteral(
        "https://qpic.y.qq.com/music_cover/fixture.jpg"));
    QCOMPARE(result.value.items.at(2).artworkUrl.toString(), QStringLiteral(
        "https://music-file.y.qq.com/songlist/fixture.jpg"));
    QCOMPARE(result.value.items.at(3).artworkUrl.toString(), QStringLiteral(
        "https://y.qq.com/music/photo/fixture.jpg"));
    QCOMPARE(result.value.items.at(4).artworkUrl.toString(), QStringLiteral(
        "https://i.gtimg.cn/music/photo/fixture.jpg"));
    QVERIFY(result.value.items.at(5).artworkUrl.isEmpty());
    QVERIFY(result.value.items.at(6).artworkUrl.isEmpty());
    QVERIFY(!QqRequestPolicy::allowsArtwork(
        result.value.items.at(5).artworkUrl));
    QVERIFY(!QqRequestPolicy::allowsArtwork(
        result.value.items.at(6).artworkUrl));

    QJsonObject filteredRoot = QJsonDocument::fromJson(
        filteredPlaylistsPayload()).object();
    QJsonObject filteredData = filteredRoot.value(QStringLiteral("data"))
        .toObject();
    QJsonArray filteredItems = filteredData.value(QStringLiteral("list"))
        .toArray();
    QJsonObject filteredItem = filteredItems.first().toObject();
    filteredItem.insert(QStringLiteral("imgurl"), QString());
    filteredItems.replace(0, filteredItem);
    filteredData.insert(QStringLiteral("list"), filteredItems);
    filteredRoot.insert(QStringLiteral("data"), filteredData);
    const ServiceResult<PlaylistPage> strict = QqPublicParser::playlists(
        QJsonDocument(filteredRoot).toJson(QJsonDocument::Compact), 104);
    QVERIFY(!strict.ok());
    QCOMPARE(strict.error.diagnosticCode,
             QStringLiteral("qq.public.playlist.artwork.invalid"));
}

void QqPublicParserTest::preservesValidEmptySearchPages()
{
    const QByteArray empty = QByteArrayLiteral(
        "{\"req\":{\"data\":{\"body\":{\"song\":{\"list\":[]}},"
        "\"meta\":{\"sum\":0}}}}"
    );
    const ServiceResult<TrackPage> result =
        QqPublicParser::searchTracks(empty, 12);
    QVERIFY(result.ok());
    QVERIFY(result.value.items.isEmpty());
    QCOMPARE(result.value.total, qint64(0));
}

void QqPublicParserTest::rejectsInvalidSongFieldsWithoutCoercion()
{
    const QJsonObject validRoot = QJsonDocument::fromJson(
        fixture(QStringLiteral("search-tracks.json"))).object();
    QJsonObject root = validRoot;
    QJsonObject req = root.value(QStringLiteral("req")).toObject();
    QJsonObject data = req.value(QStringLiteral("data")).toObject();
    QJsonObject body = data.value(QStringLiteral("body")).toObject();
    QJsonObject song = body.value(QStringLiteral("song")).toObject();
    QJsonArray list = song.value(QStringLiteral("list")).toArray();
    QJsonObject first = list.first().toObject();
    first.insert(QStringLiteral("mid"), 3003);
    list.replace(0, first);
    song.insert(QStringLiteral("list"), list);
    body.insert(QStringLiteral("song"), song);
    data.insert(QStringLiteral("body"), body);
    req.insert(QStringLiteral("data"), data);
    root.insert(QStringLiteral("req"), req);

    const ServiceResult<TrackPage> result = QqPublicParser::searchTracks(
        QJsonDocument(root).toJson(QJsonDocument::Compact), 13);
    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(13));
    QCOMPARE(result.error.diagnosticCode, QStringLiteral("json.mid.string"));

    root = validRoot;
    req = root.value(QStringLiteral("req")).toObject();
    data = req.value(QStringLiteral("data")).toObject();
    body = data.value(QStringLiteral("body")).toObject();
    song = body.value(QStringLiteral("song")).toObject();
    list = song.value(QStringLiteral("list")).toArray();
    first = list.first().toObject();
    first.insert(QStringLiteral("switch"), QStringLiteral("2"));
    list.replace(0, first);
    song.insert(QStringLiteral("list"), list);
    body.insert(QStringLiteral("song"), song);
    data.insert(QStringLiteral("body"), body);
    req.insert(QStringLiteral("data"), data);
    root.insert(QStringLiteral("req"), req);

    const ServiceResult<TrackPage> wrongSwitch = QqPublicParser::searchTracks(
        QJsonDocument(root).toJson(QJsonDocument::Compact), 131);
    QVERIFY(!wrongSwitch.ok());
    QCOMPARE(wrongSwitch.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(wrongSwitch.error.operationId, quint64(131));
    QCOMPARE(wrongSwitch.error.diagnosticCode,
             QStringLiteral("json.switch.integer"));
}

void QqPublicParserTest::parsesToplistSummaries()
{
    const ServiceResult<QVector<PlaylistSummary> > result =
        QqPublicParser::toplists(fixture(QStringLiteral("toplists.json")), 14);
    QVERIFY(result.ok());
    QCOMPARE(result.value.size(), 1);
    QCOMPARE(result.value.first().id, QStringLiteral("26"));
    QCOMPARE(result.value.first().title,
             QString::fromUtf8("热歌榜 & 周榜"));
    QCOMPARE(result.value.first().author, QStringLiteral("QQ Music"));
    QCOMPARE(result.value.first().artworkUrl.toString(),
             QStringLiteral("https://y.gtimg.cn/music/photo_new/"
                            "T002R300x300M000toplist.jpg"));
    QCOMPARE(result.value.first().sourceUrl.toString(),
             QStringLiteral("https://y.qq.com/n/ryqq/toplist/26"));
    QVERIFY(QqIdentifier::isSafeSourceUrl(result.value.first().sourceUrl));
}

void QqPublicParserTest::validatesToplistIdIntBoundary()
{
    QJsonObject root = QJsonDocument::fromJson(
        fixture(QStringLiteral("toplists.json"))).object();
    QJsonObject data = root.value(QStringLiteral("data")).toObject();
    QJsonArray items = data.value(QStringLiteral("topList")).toArray();
    QJsonObject item = items.first().toObject();
    const qint64 maximum = std::numeric_limits<int>::max();

    item.insert(QStringLiteral("id"), static_cast<double>(maximum));
    items.replace(0, item);
    data.insert(QStringLiteral("topList"), items);
    root.insert(QStringLiteral("data"), data);
    const ServiceResult<QVector<PlaylistSummary> > accepted =
        QqPublicParser::toplists(
            QJsonDocument(root).toJson(QJsonDocument::Compact), 140);
    QVERIFY(accepted.ok());
    QCOMPARE(accepted.value.first().id, QString::number(maximum));

    item.insert(QStringLiteral("id"), static_cast<double>(maximum + 1));
    items.replace(0, item);
    data.insert(QStringLiteral("topList"), items);
    root.insert(QStringLiteral("data"), data);
    const ServiceResult<QVector<PlaylistSummary> > rejected =
        QqPublicParser::toplists(
            QJsonDocument(root).toJson(QJsonDocument::Compact), 141);
    QVERIFY(!rejected.ok());
    QCOMPARE(rejected.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(rejected.error.operationId, quint64(141));
    QCOMPARE(rejected.error.diagnosticCode,
             QStringLiteral("qq.public.toplist.id.invalid"));
}

void QqPublicParserTest::extractsPeriodOnlyFromPinnedChartButtons()
{
    const ServiceResult<QString> period = QqPublicParser::toplistPeriod(
        fixture(QStringLiteral("toplist-period.html")), 26, 15);
    QVERIFY(period.ok());
    QCOMPARE(period.value, QStringLiteral("2026-07-24"));

    const ServiceResult<QString> decoy = QqPublicParser::toplistPeriod(
        fixture(QStringLiteral("toplist-period.html")), 99, 16);
    QVERIFY(!decoy.ok());
    QCOMPARE(decoy.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(decoy.error.operationId, quint64(16));
    QCOMPARE(decoy.error.diagnosticCode,
             QStringLiteral("qq.public.toplistPeriod.missing"));

    const ServiceResult<QString> malformed = QqPublicParser::toplistPeriod(
        QByteArrayLiteral("<i class=\"play_cover__btn c_tx_link js_icon_play\" "
                          "data-tid=\"rank/26\" data-date=\"not a period!\"></i>"),
        26, 17);
    QVERIFY(!malformed.ok());

    const ServiceResult<QString> missingPinnedAttributes =
        QqPublicParser::toplistPeriod(
            QByteArrayLiteral(
                "<i class=\"play_cover__btn c_tx_link js_icon_play\" "
                "data-tid=\"rank/26\" data-date=\"2026-07-24\"></i>"),
            26, 171);
    QVERIFY(!missingPinnedAttributes.ok());
    QCOMPARE(missingPinnedAttributes.error.operationId, quint64(171));
}

void QqPublicParserTest::parsesToplistDetail()
{
    const ServiceResult<PlaylistDetail> result = QqPublicParser::toplistDetail(
        fixture(QStringLiteral("toplist.json")), 26, 18);
    QVERIFY(result.ok());
    QCOMPARE(result.value.info.id, QStringLiteral("26"));
    QCOMPARE(result.value.info.title,
             QString::fromUtf8("热歌榜 & 周榜"));
    QCOMPARE(result.value.info.author, QStringLiteral("QQ Music"));
    QCOMPARE(result.value.info.trackCount, 1);
    QCOMPARE(result.value.info.artworkUrl.toString(),
             QStringLiteral("https://y.gtimg.cn/music/photo_new/"
                            "T002R300x300M000chart.jpg"));
    QCOMPARE(result.value.info.sourceUrl.toString(),
             QStringLiteral("https://y.qq.com/n/ryqq/toplist/26"));
    QCOMPARE(result.value.tracks.size(), 1);
    QCOMPARE(result.value.tracks.first().mid,
             QStringLiteral("006ABCdef23456"));
    QCOMPARE(result.value.tracks.first().title,
             QString::fromUtf8("榜单歌曲 & 一"));
    QVERIFY(result.value.tracks.first().playable);
}

QTEST_APPLESS_MAIN(QqPublicParserTest)

#include "tst_qqpublicparser.moc"
