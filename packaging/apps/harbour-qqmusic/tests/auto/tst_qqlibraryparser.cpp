#include <QtTest>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

#include <algorithm>

#include "core/qqidentifier.h"
#include "service/qqlibraryparser.h"
#include "service/qqrequestbuilder.h"

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

void sortQueryItems(QList<QueryItem> *items)
{
    std::sort(items->begin(), items->end(), queryItemLessThan);
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

QString queryValue(const QUrl &url, const QString &name)
{
    return QUrlQuery(url).queryItemValue(name, QUrl::FullyDecoded);
}

QJsonObject queryJson(const QUrl &url, const QString &name)
{
    return QJsonDocument::fromJson(queryValue(url, name).toUtf8()).object();
}

QJsonObject expectedUserRequest(const QString &uin)
{
    QJsonObject comm;
    comm.insert(QStringLiteral("ct"), 24);
    comm.insert(QStringLiteral("cv"), 0);

    QJsonArray users;
    users.append(uin);

    QJsonObject vipParam;
    vipParam.insert(QStringLiteral("uin_list"), users);
    QJsonObject vip;
    vip.insert(QStringLiteral("module"),
               QStringLiteral("userInfo.VipQueryServer"));
    vip.insert(QStringLiteral("method"),
               QStringLiteral("SRFVipQuery_V2"));
    vip.insert(QStringLiteral("param"), vipParam);

    QJsonObject baseParam;
    baseParam.insert(QStringLiteral("vec_uin"), users);
    QJsonObject base;
    base.insert(QStringLiteral("module"),
                QStringLiteral("userInfo.BaseUserInfoServer"));
    base.insert(QStringLiteral("method"),
                QStringLiteral("get_user_baseinfo_v2"));
    base.insert(QStringLiteral("param"), baseParam);

    QJsonObject request;
    request.insert(QStringLiteral("comm"), comm);
    request.insert(QStringLiteral("vip"), vip);
    request.insert(QStringLiteral("base"), base);
    return request;
}

QByteArray withPlaylist(const QJsonObject &playlist)
{
    QJsonObject root;
    QJsonArray items;
    items.append(playlist);
    root.insert(QStringLiteral("cdlist"), items);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QJsonObject firstPlaylist()
{
    return QJsonDocument::fromJson(
        fixture(QStringLiteral("playlist.json"))).object()
        .value(QStringLiteral("cdlist")).toArray().first().toObject();
}

QByteArray withFirstTrack(const QJsonObject &track)
{
    QJsonObject playlist = firstPlaylist();
    QJsonArray tracks = playlist.value(QStringLiteral("songlist")).toArray();
    tracks.replace(0, track);
    playlist.insert(QStringLiteral("songlist"), tracks);
    return withPlaylist(playlist);
}

QJsonObject firstTrack()
{
    return firstPlaylist().value(QStringLiteral("songlist"))
        .toArray().first().toObject();
}

QByteArray withCreatedItems(const QJsonArray &items)
{
    QJsonObject data;
    data.insert(QStringLiteral("disslist"), items);
    QJsonObject root;
    root.insert(QStringLiteral("data"), data);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray withCollectedItems(const QJsonArray &items)
{
    QJsonObject data;
    data.insert(QStringLiteral("cdlist"), items);
    QJsonObject root;
    root.insert(QStringLiteral("data"), data);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

}

class QqLibraryParserTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsPinnedReadOnlyUrls();
    void rejectsUnsafeRequestIdentifiers();
    void parsesPlaylistAndLegacyTracks();
    void preservesValidEmptyPlaylist();
    void parsesMixedPlaylistWithAlbumlessTrack();
    void rejectsInconsistentAlbumMetadata();
    void parsesValidSingerMid();
    void rejectsWrongSingerMidType();
    void rejectsInvalidSingerMid();
    void defaultsMissingSwitchToPlayable();
    void rejectsWrongSwitchType();
    void rejectsNegativeSwitch();
    void marksSwitchZeroUnplayable();
    void rejectsMissingSingerArrays();
    void rejectsWrongPlaylistIdentifierTypes();
    void normalizesReviewedHttpPlaylistArtwork();
    void rejectsUnsafePlaylistArtwork();
    void parsesUserKeyedByUinWithoutSecrets();
    void normalizesReviewedHttpUserAvatar();
    void acceptsEmptyUserAvatar();
    void rejectsInvalidUserContracts();
    void classifiesCreatedLikedAndCollectedPlaylists();
    void acceptsIntegerVisibilityFlags();
    void preservesValidEmptyUserLibrary();
    void acceptsEmptyAccountArtwork();
    void rejectsWrongAccountPlaylistIdentifierTypes();
    void rejectsUnsafeAccountArtwork();
    void identifiesMalformedLibraryHalf();
    void mapsExpiredAccountLibraryApplicationCodes();
};

void QqLibraryParserTest::buildsPinnedReadOnlyUrls()
{
    const QString uin = QStringLiteral("1000123456");
    const QUrl playlist =
        QqRequestBuilder::playlistUrl(QStringLiteral("9451963602"));
    QCOMPARE(playlist.scheme(), QStringLiteral("https"));
    QCOMPARE(playlist.host(), QStringLiteral("i.y.qq.com"));
    QCOMPARE(playlist.path(), QStringLiteral(
        "/qzone-music/fcg-bin/fcg_ucc_getcdinfo_byids_cp.fcg"));
    QList<QueryItem> playlistQuery;
    playlistQuery
        << QueryItem(QStringLiteral("type"), QStringLiteral("1"))
        << QueryItem(QStringLiteral("json"), QStringLiteral("1"))
        << QueryItem(QStringLiteral("utf8"), QStringLiteral("1"))
        << QueryItem(QStringLiteral("onlysong"), QStringLiteral("0"))
        << QueryItem(QStringLiteral("nosign"), QStringLiteral("1"))
        << QueryItem(QStringLiteral("disstid"), QStringLiteral("9451963602"))
        << QueryItem(QStringLiteral("g_tk"), QStringLiteral("5381"))
        << QueryItem(QStringLiteral("loginUin"), QStringLiteral("0"))
        << QueryItem(QStringLiteral("hostUin"), QStringLiteral("0"))
        << QueryItem(QStringLiteral("format"), QStringLiteral("json"))
        << QueryItem(QStringLiteral("inCharset"), QStringLiteral("GB2312"))
        << QueryItem(QStringLiteral("outCharset"), QStringLiteral("utf-8"))
        << QueryItem(QStringLiteral("notice"), QStringLiteral("0"))
        << QueryItem(QStringLiteral("platform"), QStringLiteral("yqq"))
        << QueryItem(QStringLiteral("needNewCode"), QStringLiteral("0"));
    sortQueryItems(&playlistQuery);
    QCOMPARE(decodedQuery(playlist), playlistQuery);

    const QUrl user = QqRequestBuilder::userUrl(uin);
    QCOMPARE(user.scheme(), QStringLiteral("https"));
    QCOMPARE(user.host(), QStringLiteral("u.y.qq.com"));
    QCOMPARE(user.path(), QStringLiteral("/cgi-bin/musicu.fcg"));
    QList<QueryItem> userQuery;
    userQuery
        << QueryItem(QStringLiteral("format"), QStringLiteral("json"))
        << QueryItem(QStringLiteral("inCharset"), QStringLiteral("utf8"))
        << QueryItem(QStringLiteral("outCharset"), QStringLiteral("utf-8"))
        << QueryItem(QStringLiteral("platform"), QStringLiteral("yqq.json"))
        << QueryItem(QStringLiteral("needNewCode"), QStringLiteral("0"))
        << QueryItem(QStringLiteral("loginUin"), uin)
        << QueryItem(QStringLiteral("hostUin"), QStringLiteral("0"));
    sortQueryItems(&userQuery);
    QCOMPARE(decodedQuery(user, QStringList() << QStringLiteral("data")),
             userQuery);
    QCOMPARE(queryItemCount(user, QStringLiteral("data")), 1);
    QCOMPARE(queryJson(user, QStringLiteral("data")),
             expectedUserRequest(uin));

    const QUrl created = QqRequestBuilder::userCreatedUrl(uin);
    QCOMPARE(created.scheme(), QStringLiteral("https"));
    QCOMPARE(created.host(), QStringLiteral("c.y.qq.com"));
    QCOMPARE(created.path(),
             QStringLiteral("/rsc/fcgi-bin/fcg_user_created_diss"));
    QList<QueryItem> createdQuery;
    createdQuery
        << QueryItem(QStringLiteral("cv"), QStringLiteral("4747474"))
        << QueryItem(QStringLiteral("ct"), QStringLiteral("24"))
        << QueryItem(QStringLiteral("format"), QStringLiteral("json"))
        << QueryItem(QStringLiteral("inCharset"), QStringLiteral("utf-8"))
        << QueryItem(QStringLiteral("outCharset"), QStringLiteral("utf-8"))
        << QueryItem(QStringLiteral("notice"), QStringLiteral("0"))
        << QueryItem(QStringLiteral("platform"), QStringLiteral("yqq.json"))
        << QueryItem(QStringLiteral("needNewCode"), QStringLiteral("1"))
        << QueryItem(QStringLiteral("uin"), uin)
        << QueryItem(QStringLiteral("hostuin"), uin)
        << QueryItem(QStringLiteral("sin"), QStringLiteral("0"))
        << QueryItem(QStringLiteral("size"), QStringLiteral("100"));
    sortQueryItems(&createdQuery);
    QCOMPARE(decodedQuery(created), createdQuery);

    const QUrl collected = QqRequestBuilder::userCollectedUrl(uin);
    QCOMPARE(collected.scheme(), QStringLiteral("https"));
    QCOMPARE(collected.host(), QStringLiteral("c.y.qq.com"));
    QCOMPARE(collected.path(),
             QStringLiteral("/fav/fcgi-bin/fcg_get_profile_order_asset.fcg"));
    QList<QueryItem> collectedQuery;
    collectedQuery
        << QueryItem(QStringLiteral("ct"), QStringLiteral("20"))
        << QueryItem(QStringLiteral("cid"), QStringLiteral("205360956"))
        << QueryItem(QStringLiteral("userid"), uin)
        << QueryItem(QStringLiteral("reqtype"), QStringLiteral("3"))
        << QueryItem(QStringLiteral("sin"), QStringLiteral("0"))
        << QueryItem(QStringLiteral("ein"), QStringLiteral("100"));
    sortQueryItems(&collectedQuery);
    QCOMPARE(decodedQuery(collected), collectedQuery);
}

void QqLibraryParserTest::rejectsUnsafeRequestIdentifiers()
{
    QVERIFY(QqRequestBuilder::playlistUrl(QStringLiteral("../cookie")).isEmpty());
    QVERIFY(QqRequestBuilder::playlistUrl(QStringLiteral("0")).isEmpty());
    QVERIFY(QqRequestBuilder::userUrl(QStringLiteral("1000x")).isEmpty());
    QVERIFY(QqRequestBuilder::userCreatedUrl(QStringLiteral("0123")).isEmpty());
    QVERIFY(QqRequestBuilder::userCollectedUrl(QString()).isEmpty());
}

void QqLibraryParserTest::parsesPlaylistAndLegacyTracks()
{
    const ServiceResult<PlaylistDetail> result = QqLibraryParser::playlist(
        fixture(QStringLiteral("playlist.json")),
        QStringLiteral("9451963602"), 17);
    QVERIFY(result.ok());
    QCOMPARE(result.value.info.id, QStringLiteral("9451963602"));
    QCOMPARE(result.value.info.title, QString::fromUtf8("通勤 & 收藏"));
    QCOMPARE(result.value.info.artworkUrl.toString(),
             QStringLiteral("https://y.gtimg.cn/music/photo_new/"
                            "playlist-library.jpg"));
    QCOMPARE(result.value.info.sourceUrl.toString(),
             QStringLiteral("https://y.qq.com/n/ryqq/playlist/9451963602"));
    QVERIFY(QqIdentifier::isSafeSourceUrl(result.value.info.sourceUrl));
    QCOMPARE(result.value.info.trackCount, 1);
    QCOMPARE(result.value.tracks.size(), 1);

    const Track track = result.value.tracks.first();
    QCOMPARE(track.mid, QStringLiteral("003OUlho2HcRHC"));
    QCOMPARE(track.title, QString::fromUtf8("晨光 & 海"));
    QCOMPARE(track.artist, QString::fromUtf8("测试歌手"));
    QVERIFY(track.artistMid.isEmpty());
    QCOMPARE(track.album, QString::fromUtf8("测试专辑"));
    QCOMPARE(track.albumMid, QStringLiteral("000ABCdef23456"));
    QCOMPARE(track.artworkUrl.toString(),
             QStringLiteral("https://y.gtimg.cn/music/photo_new/"
                            "T002R300x300M000000ABCdef23456.jpg"));
    QCOMPARE(track.sourceUrl.toString(),
             QStringLiteral("https://y.qq.com/n/ryqq/songDetail/003OUlho2HcRHC"));
    QVERIFY(QqIdentifier::isSafeSourceUrl(track.sourceUrl));
    QVERIFY(track.playable);
}

void QqLibraryParserTest::preservesValidEmptyPlaylist()
{
    QJsonObject playlist = firstPlaylist();
    playlist.insert(QStringLiteral("songlist"), QJsonArray());
    const ServiceResult<PlaylistDetail> result = QqLibraryParser::playlist(
        withPlaylist(playlist), QStringLiteral("9451963602"), 171);
    QVERIFY(result.ok());
    QVERIFY(result.value.tracks.isEmpty());
    QCOMPARE(result.value.info.trackCount, 0);
}

void QqLibraryParserTest::parsesMixedPlaylistWithAlbumlessTrack()
{
    QJsonObject playlist = firstPlaylist();
    QJsonArray tracks = playlist.value(QStringLiteral("songlist")).toArray();
    QJsonObject albumless = firstTrack();
    albumless.insert(QStringLiteral("albummid"), QString());
    albumless.insert(QStringLiteral("albumname"), QString());
    tracks.append(albumless);
    playlist.insert(QStringLiteral("songlist"), tracks);

    const ServiceResult<PlaylistDetail> result = QqLibraryParser::playlist(
        withPlaylist(playlist), QStringLiteral("9451963602"), 1711);

    QVERIFY(result.ok());
    QCOMPARE(result.value.info.trackCount, 2);
    QCOMPARE(result.value.tracks.size(), 2);
    QVERIFY(result.value.tracks.at(1).albumMid.isEmpty());
    QVERIFY(result.value.tracks.at(1).album.isEmpty());
    QVERIFY(result.value.tracks.at(1).artworkUrl.isEmpty());
    QVERIFY(result.value.tracks.at(1).playable);
}

void QqLibraryParserTest::rejectsInconsistentAlbumMetadata()
{
    QJsonObject missingMid = firstTrack();
    missingMid.insert(QStringLiteral("albummid"), QString());
    const ServiceResult<PlaylistDetail> midResult = QqLibraryParser::playlist(
        withFirstTrack(missingMid), QStringLiteral("9451963602"), 1712);
    QVERIFY(!midResult.ok());
    QCOMPARE(midResult.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(midResult.error.diagnosticCode,
             QStringLiteral("qq.library.playlist.track.albumMid.invalid"));

    QJsonObject missingName = firstTrack();
    missingName.insert(QStringLiteral("albumname"), QString());
    const ServiceResult<PlaylistDetail> nameResult = QqLibraryParser::playlist(
        withFirstTrack(missingName), QStringLiteral("9451963602"), 1713);
    QVERIFY(!nameResult.ok());
    QCOMPARE(nameResult.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(nameResult.error.diagnosticCode,
             QStringLiteral("qq.library.playlist.track.album.empty"));
}

void QqLibraryParserTest::parsesValidSingerMid()
{
    QJsonObject track = firstTrack();
    QJsonArray singers = track.value(QStringLiteral("singer")).toArray();
    QJsonObject singer = singers.first().toObject();
    singer.insert(QStringLiteral("mid"), QStringLiteral("002ABCdef23456"));
    singers.replace(0, singer);
    track.insert(QStringLiteral("singer"), singers);

    const ServiceResult<PlaylistDetail> result = QqLibraryParser::playlist(
        withFirstTrack(track),
        QStringLiteral("9451963602"), 172);
    QVERIFY(result.ok());
    QCOMPARE(result.value.tracks.first().artistMid,
             QStringLiteral("002ABCdef23456"));
}

void QqLibraryParserTest::rejectsWrongSingerMidType()
{
    QJsonObject track = firstTrack();
    QJsonArray singers = track.value(QStringLiteral("singer")).toArray();
    QJsonObject singer = singers.first().toObject();
    singer.insert(QStringLiteral("mid"), 2);
    singers.replace(0, singer);
    track.insert(QStringLiteral("singer"), singers);

    const ServiceResult<PlaylistDetail> result = QqLibraryParser::playlist(
        withFirstTrack(track), QStringLiteral("9451963602"), 1721);
    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(1721));
    QCOMPARE(result.error.diagnosticCode,
             QStringLiteral(
                 "qq.library.playlist.track.singer.json.mid.string"));
}

void QqLibraryParserTest::rejectsInvalidSingerMid()
{
    QJsonObject track = firstTrack();
    QJsonArray singers = track.value(QStringLiteral("singer")).toArray();
    QJsonObject singer = singers.first().toObject();
    singer.insert(QStringLiteral("mid"), QStringLiteral("not-a-track-mid"));
    singers.replace(0, singer);
    track.insert(QStringLiteral("singer"), singers);

    const ServiceResult<PlaylistDetail> result = QqLibraryParser::playlist(
        withFirstTrack(track), QStringLiteral("9451963602"), 1722);
    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(1722));
    QCOMPARE(result.error.diagnosticCode,
             QStringLiteral("qq.library.playlist.track.singer.mid.invalid"));
}

void QqLibraryParserTest::defaultsMissingSwitchToPlayable()
{
    QJsonObject track = firstTrack();
    track.remove(QStringLiteral("switch"));

    const ServiceResult<PlaylistDetail> result = QqLibraryParser::playlist(
        withFirstTrack(track), QStringLiteral("9451963602"), 1723);
    QVERIFY(result.ok());
    QVERIFY(result.value.tracks.first().playable);
}

void QqLibraryParserTest::rejectsWrongSwitchType()
{
    QJsonObject track = firstTrack();
    track.insert(QStringLiteral("switch"), QStringLiteral("2"));

    const ServiceResult<PlaylistDetail> result = QqLibraryParser::playlist(
        withFirstTrack(track), QStringLiteral("9451963602"), 1724);
    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(1724));
    QCOMPARE(result.error.diagnosticCode,
             QStringLiteral("qq.library.playlist.track.json.switch.integer"));
}

void QqLibraryParserTest::rejectsNegativeSwitch()
{
    QJsonObject track = firstTrack();
    track.insert(QStringLiteral("switch"), -1);

    const ServiceResult<PlaylistDetail> result = QqLibraryParser::playlist(
        withFirstTrack(track), QStringLiteral("9451963602"), 1725);
    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(1725));
    QCOMPARE(result.error.diagnosticCode,
             QStringLiteral("qq.library.playlist.track.switch.invalid"));
}

void QqLibraryParserTest::marksSwitchZeroUnplayable()
{
    QJsonObject track = firstTrack();
    track.insert(QStringLiteral("switch"), 0);

    const ServiceResult<PlaylistDetail> result = QqLibraryParser::playlist(
        withFirstTrack(track), QStringLiteral("9451963602"), 1726);
    QVERIFY(result.ok());
    QVERIFY(!result.value.tracks.first().playable);
}

void QqLibraryParserTest::rejectsMissingSingerArrays()
{
    QJsonObject playlist = firstPlaylist();
    QJsonArray songs = playlist.value(QStringLiteral("songlist")).toArray();
    QJsonObject song = songs.first().toObject();
    song.remove(QStringLiteral("singer"));
    songs.replace(0, song);
    playlist.insert(QStringLiteral("songlist"), songs);

    const ServiceResult<PlaylistDetail> missing = QqLibraryParser::playlist(
        withPlaylist(playlist), QStringLiteral("9451963602"), 173);
    QVERIFY(!missing.ok());
    QCOMPARE(missing.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(missing.error.operationId, quint64(173));
    QVERIFY(missing.error.diagnosticCode.contains(QStringLiteral("singer")));

    song.insert(QStringLiteral("singer"), QJsonArray());
    songs.replace(0, song);
    playlist.insert(QStringLiteral("songlist"), songs);
    const ServiceResult<PlaylistDetail> empty = QqLibraryParser::playlist(
        withPlaylist(playlist), QStringLiteral("9451963602"), 174);
    QVERIFY(!empty.ok());
    QCOMPARE(empty.error.operationId, quint64(174));
    QVERIFY(empty.error.diagnosticCode.contains(QStringLiteral("singer")));
}

void QqLibraryParserTest::rejectsWrongPlaylistIdentifierTypes()
{
    QJsonObject playlist = firstPlaylist();
    playlist.insert(QStringLiteral("disstid"), 9451963602.0);
    const ServiceResult<PlaylistDetail> wrongType = QqLibraryParser::playlist(
        withPlaylist(playlist), QStringLiteral("9451963602"), 175);
    QVERIFY(!wrongType.ok());
    QCOMPARE(wrongType.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(wrongType.error.operationId, quint64(175));
    QVERIFY(wrongType.error.diagnosticCode.contains(QStringLiteral("id")));

    const ServiceResult<PlaylistDetail> unsafeExpected =
        QqLibraryParser::playlist(
            fixture(QStringLiteral("playlist.json")),
            QStringLiteral("../cookie"), 176);
    QVERIFY(!unsafeExpected.ok());
    QCOMPARE(unsafeExpected.error.operationId, quint64(176));
    QVERIFY(unsafeExpected.error.diagnosticCode.contains(
        QStringLiteral("expectedId")));
}

void QqLibraryParserTest::normalizesReviewedHttpPlaylistArtwork()
{
    QJsonObject playlist = firstPlaylist();
    playlist.insert(QStringLiteral("logo"), QStringLiteral(
        "http://p.qpic.cn/music_cover/sanitized.jpg"));

    const ServiceResult<PlaylistDetail> result = QqLibraryParser::playlist(
        withPlaylist(playlist), QStringLiteral("9451963602"), 1761);

    QVERIFY(result.ok());
    QCOMPARE(result.value.info.artworkUrl.toString(),
             QStringLiteral(
                 "https://p.qpic.cn/music_cover/sanitized.jpg"));
}

void QqLibraryParserTest::rejectsUnsafePlaylistArtwork()
{
    QJsonObject playlist = firstPlaylist();
    playlist.insert(QStringLiteral("logo"),
                    QStringLiteral("http://example.invalid/art.jpg"));
    const ServiceResult<PlaylistDetail> result = QqLibraryParser::playlist(
        withPlaylist(playlist), QStringLiteral("9451963602"), 177);
    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(177));
    QVERIFY(result.error.diagnosticCode.contains(QStringLiteral("artwork")));

    playlist.insert(QStringLiteral("logo"), QStringLiteral(
        "https://p.qpic.cn.evil.invalid/music_cover/sanitized.jpg"));
    const ServiceResult<PlaylistDetail> lookalike = QqLibraryParser::playlist(
        withPlaylist(playlist), QStringLiteral("9451963602"), 1771);
    QVERIFY(!lookalike.ok());
    QCOMPARE(lookalike.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(lookalike.error.operationId, quint64(1771));
}

void QqLibraryParserTest::parsesUserKeyedByUinWithoutSecrets()
{
    const ServiceResult<UserProfile> user = QqLibraryParser::user(
        fixture(QStringLiteral("user.json")), QStringLiteral("1000123456"), 18);
    QVERIFY(user.ok());
    QCOMPARE(user.value.uin, QStringLiteral("1000123456"));
    QCOMPARE(user.value.nickname, QString::fromUtf8("测试用户"));
    QCOMPARE(user.value.avatarUrl.toString(),
             QStringLiteral("https://thirdqq.qlogo.cn/g?b=oidb&k=fake&spec=100"));
    QVERIFY(!QJsonDocument::fromJson(
        fixture(QStringLiteral("user.json"))).toJson().contains("cookie"));
}

void QqLibraryParserTest::normalizesReviewedHttpUserAvatar()
{
    QJsonObject root = QJsonDocument::fromJson(
        fixture(QStringLiteral("user.json"))).object();
    QJsonObject base = root.value(QStringLiteral("base")).toObject();
    QJsonObject data = base.value(QStringLiteral("data")).toObject();
    QJsonObject users = data.value(QStringLiteral("map_userinfo")).toObject();
    QJsonObject profile = users.value(QStringLiteral("1000123456")).toObject();
    profile.insert(QStringLiteral("headurl"), QStringLiteral(
        "http://thirdqq.qlogo.cn/g?b=oidb&k=fake&spec=100"));
    users.insert(QStringLiteral("1000123456"), profile);
    data.insert(QStringLiteral("map_userinfo"), users);
    base.insert(QStringLiteral("data"), data);
    root.insert(QStringLiteral("base"), base);

    const ServiceResult<UserProfile> user = QqLibraryParser::user(
        QJsonDocument(root).toJson(QJsonDocument::Compact),
        QStringLiteral("1000123456"), 1801);
    QVERIFY(user.ok());
    QCOMPARE(user.value.avatarUrl.toString(),
             QStringLiteral(
                 "https://thirdqq.qlogo.cn/g?b=oidb&k=fake&spec=100"));
}

void QqLibraryParserTest::acceptsEmptyUserAvatar()
{
    QJsonObject root = QJsonDocument::fromJson(
        fixture(QStringLiteral("user.json"))).object();
    QJsonObject base = root.value(QStringLiteral("base")).toObject();
    QJsonObject data = base.value(QStringLiteral("data")).toObject();
    QJsonObject users = data.value(QStringLiteral("map_userinfo")).toObject();
    QJsonObject profile = users.value(QStringLiteral("1000123456")).toObject();
    profile.insert(QStringLiteral("headurl"), QString());
    users.insert(QStringLiteral("1000123456"), profile);
    data.insert(QStringLiteral("map_userinfo"), users);
    base.insert(QStringLiteral("data"), data);
    root.insert(QStringLiteral("base"), base);

    const ServiceResult<UserProfile> user = QqLibraryParser::user(
        QJsonDocument(root).toJson(QJsonDocument::Compact),
        QStringLiteral("1000123456"), 1802);
    QVERIFY(user.ok());
    QVERIFY(user.value.avatarUrl.isEmpty());
}

void QqLibraryParserTest::rejectsInvalidUserContracts()
{
    const ServiceResult<UserProfile> wrongUin = QqLibraryParser::user(
        fixture(QStringLiteral("user.json")), QStringLiteral("uin=bad"), 181);
    QVERIFY(!wrongUin.ok());
    QCOMPARE(wrongUin.error.operationId, quint64(181));
    QVERIFY(wrongUin.error.diagnosticCode.contains(QStringLiteral("uin")));

    QJsonObject root = QJsonDocument::fromJson(
        fixture(QStringLiteral("user.json"))).object();
    QJsonObject base = root.value(QStringLiteral("base")).toObject();
    QJsonObject data = base.value(QStringLiteral("data")).toObject();
    QJsonObject users = data.value(QStringLiteral("map_userinfo")).toObject();
    QJsonObject profile = users.value(QStringLiteral("1000123456")).toObject();
    profile.insert(QStringLiteral("headurl"),
                   QStringLiteral("http://example.invalid/avatar.jpg"));
    users.insert(QStringLiteral("1000123456"), profile);
    data.insert(QStringLiteral("map_userinfo"), users);
    base.insert(QStringLiteral("data"), data);
    root.insert(QStringLiteral("base"), base);
    const ServiceResult<UserProfile> unsafeAvatar = QqLibraryParser::user(
        QJsonDocument(root).toJson(QJsonDocument::Compact),
        QStringLiteral("1000123456"), 182);
    QVERIFY(!unsafeAvatar.ok());
    QCOMPARE(unsafeAvatar.error.operationId, quint64(182));
    QVERIFY(unsafeAvatar.error.diagnosticCode.contains(QStringLiteral("avatar")));
}

void QqLibraryParserTest::classifiesCreatedLikedAndCollectedPlaylists()
{
    const ServiceResult<UserLibrary> result = QqLibraryParser::userLibrary(
        fixture(QStringLiteral("user-created.json")),
        fixture(QStringLiteral("user-collected.json")), 19);
    QVERIFY(result.ok());
    QCOMPARE(result.value.liked.size(), 1);
    QCOMPARE(result.value.created.size(), 1);
    QCOMPARE(result.value.collected.size(), 1);

    const PlaylistSummary liked = result.value.liked.first();
    QCOMPARE(liked.id, QStringLiteral("9451963604"));
    QCOMPARE(liked.title, QString::fromUtf8("我喜欢"));
    QCOMPARE(liked.artworkUrl.toString(),
             QStringLiteral("https://y.gtimg.cn/mediastyle/y/img/"
                            "cover_love_300.jpg"));
    QVERIFY(QqIdentifier::isSafeSourceUrl(liked.sourceUrl));

    const PlaylistSummary created = result.value.created.first();
    QCOMPARE(created.id, QStringLiteral("9451963602"));
    QCOMPARE(created.title, QString::fromUtf8("自建 & 歌单"));
    QCOMPARE(created.artworkUrl.toString(),
             QStringLiteral("https://y.gtimg.cn/music/photo_new/created.jpg"));
    QVERIFY(QqIdentifier::isSafeSourceUrl(created.sourceUrl));

    const PlaylistSummary collected = result.value.collected.first();
    QCOMPARE(collected.id, QStringLiteral("9451963605"));
    QCOMPARE(collected.title, QString::fromUtf8("收藏 & 歌单"));
    QCOMPARE(collected.artworkUrl.toString(),
             QStringLiteral("https://y.gtimg.cn/music/photo_new/collected.jpg"));
    QVERIFY(QqIdentifier::isSafeSourceUrl(collected.sourceUrl));
}

void QqLibraryParserTest::acceptsIntegerVisibilityFlags()
{
    QJsonObject createdRoot = QJsonDocument::fromJson(
        fixture(QStringLiteral("user-created.json"))).object();
    QJsonArray createdItems = createdRoot.value(QStringLiteral("data"))
        .toObject().value(QStringLiteral("disslist")).toArray();
    QJsonObject visibleCreated = createdItems.at(0).toObject();
    visibleCreated.insert(QStringLiteral("dir_show"), 2);
    createdItems.replace(0, visibleCreated);
    QJsonObject hiddenCreated = createdItems.at(1).toObject();
    hiddenCreated.insert(QStringLiteral("dir_show"), -1);
    createdItems.replace(1, hiddenCreated);

    QJsonObject collectedRoot = QJsonDocument::fromJson(
        fixture(QStringLiteral("user-collected.json"))).object();
    QJsonArray collectedItems = collectedRoot.value(QStringLiteral("data"))
        .toObject().value(QStringLiteral("cdlist")).toArray();
    QJsonObject visibleCollected = collectedItems.at(0).toObject();
    visibleCollected.insert(QStringLiteral("dir_show"), 3);
    collectedItems.replace(0, visibleCollected);
    QJsonObject hiddenCollected = collectedItems.at(1).toObject();
    hiddenCollected.insert(QStringLiteral("dir_show"), -1);
    collectedItems.replace(1, hiddenCollected);

    const ServiceResult<UserLibrary> result = QqLibraryParser::userLibrary(
        withCreatedItems(createdItems),
        withCollectedItems(collectedItems), 201);
    QVERIFY(result.ok());
    QCOMPARE(result.value.liked.size(), 1);
    QCOMPARE(result.value.created.size(), 1);
    QCOMPARE(result.value.collected.size(), 1);
}

void QqLibraryParserTest::preservesValidEmptyUserLibrary()
{
    const ServiceResult<UserLibrary> result = QqLibraryParser::userLibrary(
        withCreatedItems(QJsonArray()), withCollectedItems(QJsonArray()), 191);
    QVERIFY(result.ok());
    QVERIFY(result.value.liked.isEmpty());
    QVERIFY(result.value.created.isEmpty());
    QVERIFY(result.value.collected.isEmpty());
}

void QqLibraryParserTest::acceptsEmptyAccountArtwork()
{
    QJsonObject createdRoot = QJsonDocument::fromJson(
        fixture(QStringLiteral("user-created.json"))).object();
    QJsonObject created = createdRoot.value(QStringLiteral("data")).toObject()
        .value(QStringLiteral("disslist")).toArray().first().toObject();
    created.insert(QStringLiteral("diss_cover"), QString());
    QJsonArray createdItems;
    createdItems.append(created);

    QJsonObject collectedRoot = QJsonDocument::fromJson(
        fixture(QStringLiteral("user-collected.json"))).object();
    QJsonObject collected = collectedRoot.value(QStringLiteral("data")).toObject()
        .value(QStringLiteral("cdlist")).toArray().first().toObject();
    collected.insert(QStringLiteral("logo"), QString());
    QJsonArray collectedItems;
    collectedItems.append(collected);

    const ServiceResult<UserLibrary> result = QqLibraryParser::userLibrary(
        withCreatedItems(createdItems), withCollectedItems(collectedItems), 200);
    QVERIFY(result.ok());
    QCOMPARE(result.value.created.size(), 1);
    QVERIFY(result.value.created.first().artworkUrl.isEmpty());
    QCOMPARE(result.value.collected.size(), 1);
    QVERIFY(result.value.collected.first().artworkUrl.isEmpty());
}

void QqLibraryParserTest::rejectsWrongAccountPlaylistIdentifierTypes()
{
    QJsonObject createdRoot = QJsonDocument::fromJson(
        fixture(QStringLiteral("user-created.json"))).object();
    QJsonArray createdItems = createdRoot.value(QStringLiteral("data")).toObject()
        .value(QStringLiteral("disslist")).toArray();
    QJsonObject created = createdItems.first().toObject();
    created.insert(QStringLiteral("tid"), QStringLiteral("9451963602"));
    createdItems.replace(0, created);
    const ServiceResult<UserLibrary> wrongCreated =
        QqLibraryParser::userLibrary(
            withCreatedItems(createdItems),
            fixture(QStringLiteral("user-collected.json")), 192);
    QVERIFY(!wrongCreated.ok());
    QCOMPARE(wrongCreated.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(wrongCreated.error.operationId, quint64(192));
    QVERIFY(wrongCreated.error.diagnosticCode.startsWith(
        QStringLiteral("qq.library.created")));
    QVERIFY(wrongCreated.error.diagnosticCode.contains(QStringLiteral("id")));

    QJsonObject collectedRoot = QJsonDocument::fromJson(
        fixture(QStringLiteral("user-collected.json"))).object();
    QJsonArray collectedItems = collectedRoot.value(QStringLiteral("data"))
        .toObject().value(QStringLiteral("cdlist")).toArray();
    QJsonObject collected = collectedItems.first().toObject();
    collected.insert(QStringLiteral("dissid"),
                     QStringLiteral("9451963605"));
    collectedItems.replace(0, collected);
    const ServiceResult<UserLibrary> wrongCollected =
        QqLibraryParser::userLibrary(
            fixture(QStringLiteral("user-created.json")),
            withCollectedItems(collectedItems), 193);
    QVERIFY(!wrongCollected.ok());
    QCOMPARE(wrongCollected.error.operationId, quint64(193));
    QVERIFY(wrongCollected.error.diagnosticCode.startsWith(
        QStringLiteral("qq.library.collected")));
    QVERIFY(wrongCollected.error.diagnosticCode.contains(QStringLiteral("id")));
}

void QqLibraryParserTest::rejectsUnsafeAccountArtwork()
{
    QJsonObject createdRoot = QJsonDocument::fromJson(
        fixture(QStringLiteral("user-created.json"))).object();
    QJsonArray createdItems = createdRoot.value(QStringLiteral("data")).toObject()
        .value(QStringLiteral("disslist")).toArray();
    QJsonObject created = createdItems.first().toObject();
    created.insert(QStringLiteral("diss_cover"),
                   QStringLiteral("http://example.invalid/created.jpg"));
    createdItems.replace(0, created);
    const ServiceResult<UserLibrary> wrongCreated =
        QqLibraryParser::userLibrary(
            withCreatedItems(createdItems),
            fixture(QStringLiteral("user-collected.json")), 194);
    QVERIFY(!wrongCreated.ok());
    QCOMPARE(wrongCreated.error.operationId, quint64(194));
    QCOMPARE(wrongCreated.error.diagnosticCode,
             QStringLiteral("qq.library.created.item.artwork.invalid"));

    QJsonObject collectedRoot = QJsonDocument::fromJson(
        fixture(QStringLiteral("user-collected.json"))).object();
    QJsonArray collectedItems = collectedRoot.value(QStringLiteral("data"))
        .toObject().value(QStringLiteral("cdlist")).toArray();
    QJsonObject collected = collectedItems.first().toObject();
    collected.insert(QStringLiteral("logo"),
                     QStringLiteral("http://example.invalid/collected.jpg"));
    collectedItems.replace(0, collected);
    const ServiceResult<UserLibrary> wrongCollected =
        QqLibraryParser::userLibrary(
            fixture(QStringLiteral("user-created.json")),
            withCollectedItems(collectedItems), 195);
    QVERIFY(!wrongCollected.ok());
    QCOMPARE(wrongCollected.error.operationId, quint64(195));
    QCOMPARE(wrongCollected.error.diagnosticCode,
             QStringLiteral("qq.library.collected.item.artwork.invalid"));
}

void QqLibraryParserTest::identifiesMalformedLibraryHalf()
{
    const ServiceResult<UserLibrary> created = QqLibraryParser::userLibrary(
        QByteArrayLiteral("{\"data\":{\"disslist\":7}}"),
        withCollectedItems(QJsonArray()), 196);
    QVERIFY(!created.ok());
    QCOMPARE(created.error.operationId, quint64(196));
    QVERIFY(created.error.diagnosticCode.startsWith(
        QStringLiteral("qq.library.created")));

    const ServiceResult<UserLibrary> collected = QqLibraryParser::userLibrary(
        withCreatedItems(QJsonArray()),
        QByteArrayLiteral("{\"data\":{\"cdlist\":7}}"), 197);
    QVERIFY(!collected.ok());
    QCOMPARE(collected.error.operationId, quint64(197));
    QVERIFY(collected.error.diagnosticCode.startsWith(
        QStringLiteral("qq.library.collected")));
}

void QqLibraryParserTest::mapsExpiredAccountLibraryApplicationCodes()
{
    const ServiceResult<UserLibrary> created = QqLibraryParser::userCreated(
        QByteArrayLiteral("{\"code\":5000}"), 198);
    QVERIFY(!created.ok());
    QCOMPARE(created.error.kind, ErrorKind::AuthenticationExpired);
    QCOMPARE(created.error.operationId, quint64(198));
    QCOMPARE(created.error.diagnosticCode,
             QStringLiteral("qq.library.created.code.5000"));

    const ServiceResult<UserLibrary> collected =
        QqLibraryParser::userCollected(
            QByteArrayLiteral("{\"code\":4000}"), 199);
    QVERIFY(!collected.ok());
    QCOMPARE(collected.error.kind, ErrorKind::AuthenticationExpired);
    QCOMPARE(collected.error.operationId, quint64(199));
    QCOMPARE(collected.error.diagnosticCode,
             QStringLiteral("qq.library.collected.code.4000"));
}

QTEST_APPLESS_MAIN(QqLibraryParserTest)

#include "tst_qqlibraryparser.moc"
