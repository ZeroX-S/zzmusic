#include <QtTest>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

#include <limits>

#include "service/qqplaybackparser.h"
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

QString sanitizedTrackMid()
{
    return QStringLiteral("001SANITIZED01");
}

QString sanitizedFilename()
{
    return QStringLiteral("M500%1%1.mp3").arg(sanitizedTrackMid());
}

QString sanitizedPurl()
{
    return sanitizedFilename()
        + QStringLiteral("?vkey=not-a-live-token");
}

QString sanitizedCompactFilename()
{
    return QStringLiteral("M500%1.mp3").arg(sanitizedTrackMid());
}

QString sanitizedCompactPurl()
{
    return sanitizedCompactFilename()
        + QStringLiteral(
            "?fromtag=66&guid=10000&src=sanitized&vkey=not-a-live-token");
}

QJsonObject expectedVkeyRequest(const QString &trackMid,
                                const QString &accountUin)
{
    const QString requestUin = accountUin.isEmpty()
        ? QStringLiteral("0") : accountUin;
    QJsonArray filenames;
    filenames.append(QStringLiteral("F000") + trackMid + trackMid
                     + QStringLiteral(".flac"));
    filenames.append(QStringLiteral("M800") + trackMid + trackMid
                     + QStringLiteral(".mp3"));
    filenames.append(QStringLiteral("M500") + trackMid + trackMid
                     + QStringLiteral(".mp3"));
    QJsonArray songMids;
    QJsonArray songTypes;
    for (int index = 0; index < filenames.size(); ++index) {
        songMids.append(trackMid);
        songTypes.append(0);
    }

    QJsonObject param;
    param.insert(QStringLiteral("filename"), filenames);
    param.insert(QStringLiteral("guid"), QStringLiteral("10000"));
    param.insert(QStringLiteral("songmid"), songMids);
    param.insert(QStringLiteral("songtype"), songTypes);
    param.insert(QStringLiteral("uin"), requestUin);
    param.insert(QStringLiteral("loginflag"), 1);
    param.insert(QStringLiteral("platform"), QStringLiteral("20"));

    QJsonObject requestBlock;
    requestBlock.insert(QStringLiteral("module"),
                        QStringLiteral("vkey.GetVkeyServer"));
    requestBlock.insert(QStringLiteral("method"),
                        QStringLiteral("CgiGetVkey"));
    requestBlock.insert(QStringLiteral("param"), param);

    QJsonObject comm;
    comm.insert(QStringLiteral("uin"), requestUin);
    comm.insert(QStringLiteral("format"), QStringLiteral("json"));
    comm.insert(QStringLiteral("ct"), 24);
    comm.insert(QStringLiteral("cv"), 0);

    QJsonObject request;
    request.insert(QStringLiteral("req_1"), requestBlock);
    request.insert(QStringLiteral("loginUin"), requestUin);
    request.insert(QStringLiteral("comm"), comm);
    return request;
}

QByteArray withPlaybackValues(const QString &purl, const QString &sip)
{
    QJsonObject root = QJsonDocument::fromJson(
        fixture(QStringLiteral("vkey.json"))).object();
    QJsonObject request = root.value(QStringLiteral("req_1")).toObject();
    QJsonObject data = request.value(QStringLiteral("data")).toObject();
    QJsonArray midurlinfo = data.value(QStringLiteral("midurlinfo")).toArray();
    QJsonObject location = midurlinfo.first().toObject();
    location.insert(QStringLiteral("purl"), purl);
    midurlinfo.replace(0, location);
    data.insert(QStringLiteral("midurlinfo"), midurlinfo);
    QJsonArray sips;
    sips.append(sip);
    data.insert(QStringLiteral("sip"), sips);
    request.insert(QStringLiteral("data"), data);
    root.insert(QStringLiteral("req_1"), request);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray withLocationMember(const QString &name, const QJsonValue &value)
{
    QJsonObject root = QJsonDocument::fromJson(
        fixture(QStringLiteral("vkey.json"))).object();
    QJsonObject request = root.value(QStringLiteral("req_1")).toObject();
    QJsonObject data = request.value(QStringLiteral("data")).toObject();
    QJsonArray midurlinfo = data.value(QStringLiteral("midurlinfo")).toArray();
    QJsonObject location = midurlinfo.first().toObject();
    location.insert(name, value);
    midurlinfo.replace(0, location);
    data.insert(QStringLiteral("midurlinfo"), midurlinfo);
    request.insert(QStringLiteral("data"), data);
    root.insert(QStringLiteral("req_1"), request);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray withoutLocationMember(const QString &name)
{
    QJsonObject root = QJsonDocument::fromJson(
        fixture(QStringLiteral("vkey.json"))).object();
    QJsonObject request = root.value(QStringLiteral("req_1")).toObject();
    QJsonObject data = request.value(QStringLiteral("data")).toObject();
    QJsonArray midurlinfo = data.value(QStringLiteral("midurlinfo")).toArray();
    QJsonObject location = midurlinfo.first().toObject();
    location.remove(name);
    midurlinfo.replace(0, location);
    data.insert(QStringLiteral("midurlinfo"), midurlinfo);
    request.insert(QStringLiteral("data"), data);
    root.insert(QStringLiteral("req_1"), request);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QJsonObject qualityLocation(const QString &prefix,
                            const QString &suffix,
                            const QString &purl)
{
    QJsonObject location;
    location.insert(QStringLiteral("filename"),
                    prefix + sanitizedTrackMid() + sanitizedTrackMid()
                        + suffix);
    location.insert(QStringLiteral("purl"), purl);
    return location;
}

QString compactQualityPurl(const QString &prefix, const QString &suffix)
{
    return prefix + sanitizedTrackMid() + suffix
        + QStringLiteral("?vkey=not-a-live-token");
}

QByteArray withQualityCandidates(const QString &losslessPurl,
                                 const QString &highPurl,
                                 const QString &standardPurl)
{
    QJsonObject root = QJsonDocument::fromJson(
        fixture(QStringLiteral("vkey.json"))).object();
    QJsonObject request = root.value(QStringLiteral("req_1")).toObject();
    QJsonObject data = request.value(QStringLiteral("data")).toObject();
    QJsonArray locations;
    locations.append(qualityLocation(QStringLiteral("M500"),
                                     QStringLiteral(".mp3"), standardPurl));
    locations.append(qualityLocation(QStringLiteral("M800"),
                                     QStringLiteral(".mp3"), highPurl));
    locations.append(qualityLocation(QStringLiteral("F000"),
                                     QStringLiteral(".flac"), losslessPurl));
    data.insert(QStringLiteral("midurlinfo"), locations);
    request.insert(QStringLiteral("data"), data);
    root.insert(QStringLiteral("req_1"), request);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

}

class QqPlaybackParserTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsHighestAvailableAuthenticatedRequest();
    void rejectsUnsafePlaybackRequestIdentifiers();
    void rejectsInvalidExpectedTrackMid();
    void parsesSanitizedPlaybackLocation();
    void selectsLosslessBeforeLowerQualities();
    void fallsBackToHighestPlayableQuality();
    void parsesSanitizedLivePlaybackShape();
    void parsesCompactPurlAndNormalizesReviewedHttpSip();
    void acceptsDecimalUin();
    void acceptsExplicitDefaultHttpPort();
    void acceptsReviewedMediaHosts_data();
    void acceptsReviewedMediaHosts();
    void acceptsExplicitDefaultMediaPort();
    void classifiesEmptyPurlAsNotPlayable();
    void rejectsInvalidResponseFilenames_data();
    void rejectsInvalidResponseFilenames();
    void rejectsMismatchedResponseFilename();
    void parsesDifferentCompactPurlMid();
    void rejectsUnsafePlaybackLocations_data();
    void rejectsUnsafePlaybackLocations();
    void convertsOffsetPlaybackExpiryToUtc();
    void acceptsLatestSafePlaybackExpiry();
    void rejectsInvalidPlaybackExpiry_data();
    void rejectsInvalidPlaybackExpiry();
    void rejectsChangedPlaybackSchemas();
};

void QqPlaybackParserTest::buildsHighestAvailableAuthenticatedRequest()
{
    const QString trackMid = QStringLiteral("003OUlho2HcRHC");
    const QString accountUin = QStringLiteral("1234567890");
    const QUrl url = QqRequestBuilder::vkeyUrl();
    QCOMPARE(url.toString(),
             QStringLiteral("https://u.y.qq.com/cgi-bin/musicu.fcg"));
    QVERIFY(QUrlQuery(url).queryItems().isEmpty());

    const QJsonDocument body =
        QJsonDocument::fromJson(
            QqRequestBuilder::vkeyBody(trackMid, accountUin));
    QVERIFY(body.isObject());
    QCOMPARE(body.object(), expectedVkeyRequest(trackMid, accountUin));

    const QJsonDocument publicBody = QJsonDocument::fromJson(
        QqRequestBuilder::vkeyBody(trackMid));
    QCOMPARE(publicBody.object(), expectedVkeyRequest(trackMid, QString()));
}

void QqPlaybackParserTest::rejectsUnsafePlaybackRequestIdentifiers()
{
    QVERIFY(QqRequestBuilder::vkeyBody(QStringLiteral("../cookie")).isEmpty());
    QVERIFY(QqRequestBuilder::vkeyBody(QString(15, QLatin1Char('A'))).isEmpty());
    QVERIFY(QqRequestBuilder::vkeyBody(
        QStringLiteral("003OUlho2HcRHC"), QStringLiteral("account")).isEmpty());
}

void QqPlaybackParserTest::rejectsInvalidExpectedTrackMid()
{
    const QDateTime now(
        QDate(2026, 7, 25), QTime(0, 0), Qt::UTC);
    const ServiceResult<PlaybackLocation> result = QqPlaybackParser::parse(
        fixture(QStringLiteral("vkey.json")), QStringLiteral("invalid"),
        now, 21);

    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::Internal);
    QCOMPARE(result.error.operationId, quint64(21));
    QCOMPARE(result.error.diagnosticCode,
             QStringLiteral("qq.playback.expectedId.invalid"));
}

void QqPlaybackParserTest::parsesSanitizedPlaybackLocation()
{
    const QDateTime now(
        QDate(2026, 7, 25), QTime(0, 0), Qt::UTC);
    const ServiceResult<PlaybackLocation> result = QqPlaybackParser::parse(
        fixture(QStringLiteral("vkey.json")), sanitizedTrackMid(), now, 22);

    QVERIFY(result.ok());
    QCOMPARE(result.value.url.scheme(), QStringLiteral("https"));
    QCOMPARE(result.value.url.host(),
             QStringLiteral("ws.stream.qqmusic.qq.com"));
    QCOMPARE(result.value.url.path(QUrl::FullyDecoded),
             QLatin1Char('/') + sanitizedFilename());
    QCOMPARE(QUrlQuery(result.value.url).queryItemValue(
                 QStringLiteral("vkey"), QUrl::FullyDecoded),
             QStringLiteral("not-a-live-token"));
    QCOMPARE(result.value.bitrate, QStringLiteral("128 kbps"));
    QCOMPARE(result.value.expiresAt, now.addSecs(10 * 60));
    QVERIFY(result.value.expiresAt > now);
    QVERIFY(!result.value.accountBound);
}

void QqPlaybackParserTest::selectsLosslessBeforeLowerQualities()
{
    const QDateTime now(
        QDate(2026, 7, 25), QTime(0, 0), Qt::UTC);
    const QString lossless = compactQualityPurl(
        QStringLiteral("F000"), QStringLiteral(".flac"));
    const ServiceResult<PlaybackLocation> result = QqPlaybackParser::parse(
        withQualityCandidates(
            lossless,
            compactQualityPurl(QStringLiteral("M800"),
                               QStringLiteral(".mp3")),
            compactQualityPurl(QStringLiteral("M500"),
                               QStringLiteral(".mp3"))),
        sanitizedTrackMid(), now, 2210);

    QVERIFY(result.ok());
    QCOMPARE(result.value.bitrate, QStringLiteral("FLAC lossless"));
    QCOMPARE(result.value.url.path(QUrl::FullyDecoded),
             QLatin1Char('/') + lossless.section(QLatin1Char('?'), 0, 0));
}

void QqPlaybackParserTest::fallsBackToHighestPlayableQuality()
{
    const QDateTime now(
        QDate(2026, 7, 25), QTime(0, 0), Qt::UTC);
    const QString high = compactQualityPurl(
        QStringLiteral("M800"), QStringLiteral(".mp3"));
    const ServiceResult<PlaybackLocation> highResult = QqPlaybackParser::parse(
        withQualityCandidates(
            QString(), high,
            compactQualityPurl(QStringLiteral("M500"),
                               QStringLiteral(".mp3"))),
        sanitizedTrackMid(), now, 2211);

    QVERIFY(highResult.ok());
    QCOMPARE(highResult.value.bitrate, QStringLiteral("320 kbps"));
    QCOMPARE(highResult.value.url.path(QUrl::FullyDecoded),
             QLatin1Char('/') + high.section(QLatin1Char('?'), 0, 0));

    const ServiceResult<PlaybackLocation> standardResult =
        QqPlaybackParser::parse(
            withQualityCandidates(
                QString(), QString(),
                compactQualityPurl(QStringLiteral("M500"),
                                   QStringLiteral(".mp3"))),
            sanitizedTrackMid(), now, 2212);

    QVERIFY(standardResult.ok());
    QCOMPARE(standardResult.value.bitrate, QStringLiteral("128 kbps"));
}

void QqPlaybackParserTest::parsesSanitizedLivePlaybackShape()
{
    const QDateTime now(
        QDate(2026, 7, 25), QTime(0, 0), Qt::UTC);
    const ServiceResult<PlaybackLocation> result = QqPlaybackParser::parse(
        fixture(QStringLiteral("vkey-live-shape.json")), sanitizedTrackMid(),
        now, 220);

    QVERIFY(result.ok());
    QCOMPARE(result.value.url.scheme(), QStringLiteral("https"));
    QCOMPARE(result.value.url.host(),
             QStringLiteral("aqqmusic.tc.qq.com"));
    QCOMPARE(result.value.url.path(QUrl::FullyDecoded),
             QStringLiteral("/M500002SANITIZED02.mp3"));
    const QUrlQuery query(result.value.url);
    QCOMPARE(query.queryItemValue(QStringLiteral("guid")),
             QStringLiteral("10000"));
    QVERIFY(query.hasQueryItem(QStringLiteral("uin")));
    QVERIFY(query.queryItemValue(QStringLiteral("uin")).isEmpty());
    QCOMPARE(query.queryItemValue(QStringLiteral("fromtag")),
             QStringLiteral("291"));
}

void QqPlaybackParserTest::parsesCompactPurlAndNormalizesReviewedHttpSip()
{
    const QDateTime now(
        QDate(2026, 7, 25), QTime(0, 0), Qt::UTC);
    const ServiceResult<PlaybackLocation> result = QqPlaybackParser::parse(
        withPlaybackValues(
            sanitizedCompactPurl(),
            QStringLiteral("http://isure.stream.qqmusic.qq.com/")),
        sanitizedTrackMid(), now, 221);

    QVERIFY(result.ok());
    QCOMPARE(result.value.url.scheme(), QStringLiteral("https"));
    QCOMPARE(result.value.url.host(),
             QStringLiteral("isure.stream.qqmusic.qq.com"));
    QCOMPARE(result.value.url.port(-1), -1);
    QCOMPARE(result.value.url.path(QUrl::FullyDecoded),
             QLatin1Char('/') + sanitizedCompactFilename());
    const QUrlQuery query(result.value.url);
    QCOMPARE(query.queryItemValue(QStringLiteral("fromtag")),
             QStringLiteral("66"));
    QCOMPARE(query.queryItemValue(QStringLiteral("guid")),
             QStringLiteral("10000"));
    QCOMPARE(query.queryItemValue(QStringLiteral("src")),
             QStringLiteral("sanitized"));
    QCOMPARE(query.queryItemValue(QStringLiteral("vkey")),
             QStringLiteral("not-a-live-token"));
}

void QqPlaybackParserTest::acceptsDecimalUin()
{
    const QDateTime now(
        QDate(2026, 7, 25), QTime(0, 0), Qt::UTC);
    const ServiceResult<PlaybackLocation> result = QqPlaybackParser::parse(
        withPlaybackValues(
            sanitizedCompactFilename()
                + QStringLiteral("?uin=1234567890&vkey=not-a-live-token"),
            QStringLiteral("https://ws.stream.qqmusic.qq.com/")),
        sanitizedTrackMid(), now, 223);

    QVERIFY(result.ok());
    QCOMPARE(QUrlQuery(result.value.url).queryItemValue(
                 QStringLiteral("uin"), QUrl::FullyDecoded),
             QStringLiteral("1234567890"));
}

void QqPlaybackParserTest::acceptsExplicitDefaultHttpPort()
{
    const QDateTime now(
        QDate(2026, 7, 25), QTime(0, 0), Qt::UTC);
    const ServiceResult<PlaybackLocation> result = QqPlaybackParser::parse(
        withPlaybackValues(
            sanitizedCompactPurl(),
            QStringLiteral("http://ws.stream.qqmusic.qq.com:80/")),
        sanitizedTrackMid(), now, 222);

    QVERIFY(result.ok());
    QCOMPARE(result.value.url.scheme(), QStringLiteral("https"));
    QCOMPARE(result.value.url.port(-1), -1);
}

void QqPlaybackParserTest::acceptsReviewedMediaHosts_data()
{
    QTest::addColumn<QString>("host");
    QTest::newRow("ws") << QStringLiteral("ws.stream.qqmusic.qq.com");
    QTest::newRow("isure") << QStringLiteral("isure.stream.qqmusic.qq.com");
    QTest::newRow("download") << QStringLiteral("dl.stream.qqmusic.qq.com");
    QTest::newRow("tc") << QStringLiteral("aqqmusic.tc.qq.com");
}

void QqPlaybackParserTest::acceptsReviewedMediaHosts()
{
    QFETCH(QString, host);
    const QDateTime now(
        QDate(2026, 7, 25), QTime(0, 0), Qt::UTC);
    const QByteArray payload = withPlaybackValues(
        sanitizedPurl(),
        QStringLiteral("https://%1/").arg(host));

    const ServiceResult<PlaybackLocation> result =
        QqPlaybackParser::parse(payload, sanitizedTrackMid(), now, 23);
    QVERIFY(result.ok());
    QCOMPARE(result.value.url.host(), host);
}

void QqPlaybackParserTest::acceptsExplicitDefaultMediaPort()
{
    const QDateTime now(
        QDate(2026, 7, 25), QTime(0, 0), Qt::UTC);
    const ServiceResult<PlaybackLocation> result = QqPlaybackParser::parse(
        withPlaybackValues(
            sanitizedPurl(),
            QStringLiteral("https://ws.stream.qqmusic.qq.com:443/")),
        sanitizedTrackMid(), now, 231);

    QVERIFY(result.ok());
    QCOMPARE(result.value.url.host(),
             QStringLiteral("ws.stream.qqmusic.qq.com"));
    QCOMPARE(result.value.url.port(-1), 443);
}

void QqPlaybackParserTest::classifiesEmptyPurlAsNotPlayable()
{
    const QDateTime now(
        QDate(2026, 7, 25), QTime(0, 0), Qt::UTC);
    const ServiceResult<PlaybackLocation> result = QqPlaybackParser::parse(
        withPlaybackValues(QString(),
                           QStringLiteral("https://ws.stream.qqmusic.qq.com/")),
        sanitizedTrackMid(), now, 24);

    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::NotPlayable);
    QCOMPARE(result.error.operationId, quint64(24));
    QCOMPARE(result.error.diagnosticCode,
             QStringLiteral("qq.playback.purl.empty"));
}

void QqPlaybackParserTest::rejectsInvalidResponseFilenames_data()
{
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<QString>("diagnostic");

    QTest::newRow("missing")
        << withoutLocationMember(QStringLiteral("filename"))
        << QStringLiteral("qq.playback.json.filename.string");
    QTest::newRow("wrong-type")
        << withLocationMember(QStringLiteral("filename"), 7)
        << QStringLiteral("qq.playback.json.filename.string");
    QTest::newRow("empty")
        << withLocationMember(QStringLiteral("filename"), QString())
        << QStringLiteral("qq.playback.filename.invalid");
    QTest::newRow("unreviewed-quality")
        << withLocationMember(
               QStringLiteral("filename"),
               QStringLiteral("A000001SANITIZED01001SANITIZED01.ape"))
        << QStringLiteral("qq.playback.filename.invalid");
    QTest::newRow("malformed-mid")
        << withLocationMember(
               QStringLiteral("filename"),
               QStringLiteral("M500SANITIZEDPLACEHOLDER.mp3"))
        << QStringLiteral("qq.playback.filename.invalid");
    QTest::newRow("different-mid-halves")
        << withLocationMember(
               QStringLiteral("filename"),
               QStringLiteral("M500001SANITIZED01002SANITIZED02.mp3"))
        << QStringLiteral("qq.playback.filename.invalid");
}

void QqPlaybackParserTest::rejectsInvalidResponseFilenames()
{
    QFETCH(QByteArray, payload);
    QFETCH(QString, diagnostic);
    const QDateTime now(
        QDate(2026, 7, 25), QTime(0, 0), Qt::UTC);
    const ServiceResult<PlaybackLocation> result =
        QqPlaybackParser::parse(
            payload, sanitizedTrackMid(), now, 240);

    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(240));
    QCOMPARE(result.error.diagnosticCode, diagnostic);
}

void QqPlaybackParserTest::rejectsMismatchedResponseFilename()
{
    const QString otherFilename =
        QStringLiteral("M500002SANITIZED02002SANITIZED02.mp3");
    const QDateTime now(
        QDate(2026, 7, 25), QTime(0, 0), Qt::UTC);
    const ServiceResult<PlaybackLocation> result = QqPlaybackParser::parse(
        withLocationMember(QStringLiteral("filename"), otherFilename),
        sanitizedTrackMid(), now, 241);

    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(241));
    QCOMPARE(result.error.diagnosticCode,
             QStringLiteral("qq.playback.filename.invalid"));
}

void QqPlaybackParserTest::parsesDifferentCompactPurlMid()
{
    const QString mediaMidPurl = QStringLiteral(
        "M500002SANITIZED02.mp3?vkey=not-a-live-token");
    const QDateTime now(
        QDate(2026, 7, 25), QTime(0, 0), Qt::UTC);
    const ServiceResult<PlaybackLocation> result = QqPlaybackParser::parse(
        withPlaybackValues(
            mediaMidPurl,
            QStringLiteral("http://ws.stream.qqmusic.qq.com/")),
        sanitizedTrackMid(), now, 242);

    QVERIFY(result.ok());
    QCOMPARE(result.value.url.path(QUrl::FullyDecoded),
             QStringLiteral("/M500002SANITIZED02.mp3"));
}

void QqPlaybackParserTest::rejectsUnsafePlaybackLocations_data()
{
    QTest::addColumn<QString>("purl");
    QTest::addColumn<QString>("sip");
    QTest::addColumn<QString>("diagnostic");

    QTest::newRow("http-lookalike-host")
        << sanitizedPurl()
        << QStringLiteral("http://ws.stream.qqmusic.qq.com.evil.invalid/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("http-userinfo")
        << sanitizedPurl()
        << QStringLiteral("http://user@ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("http-wrong-port")
        << sanitizedPurl()
        << QStringLiteral("http://ws.stream.qqmusic.qq.com:81/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("http-https-port")
        << sanitizedPurl()
        << QStringLiteral("http://ws.stream.qqmusic.qq.com:443/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("http-zero-padded-default-port")
        << sanitizedPurl()
        << QStringLiteral("http://ws.stream.qqmusic.qq.com:080/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("lookalike-host")
        << sanitizedPurl()
        << QStringLiteral("https://ws.stream.qqmusic.qq.com.evil.invalid/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("sip-userinfo")
        << sanitizedPurl()
        << QStringLiteral("https://user@ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("sip-empty-userinfo")
        << sanitizedPurl()
        << QStringLiteral("https://@ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("sip-password-only")
        << sanitizedPurl()
        << QStringLiteral("https://:pass@ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("sip-empty-user-and-password")
        << sanitizedPurl()
        << QStringLiteral("https://:@ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("wrong-port")
        << sanitizedPurl()
        << QStringLiteral("https://ws.stream.qqmusic.qq.com:444/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("sip-empty-port-no-path")
        << sanitizedPurl()
        << QStringLiteral("https://ws.stream.qqmusic.qq.com:")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("sip-empty-port")
        << sanitizedPurl()
        << QStringLiteral("https://ws.stream.qqmusic.qq.com:/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("sip-zero-padded-default-port")
        << sanitizedPurl()
        << QStringLiteral("https://ws.stream.qqmusic.qq.com:0443/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("sip-overlong-default-port")
        << sanitizedPurl()
        << QStringLiteral(
               "https://ws.stream.qqmusic.qq.com:000000000000000000000443/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("sip-overflow-to-default-port")
        << sanitizedPurl()
        << QStringLiteral("https://ws.stream.qqmusic.qq.com:4294967739/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("sip-overlong-numeric-port")
        << sanitizedPurl()
        << QStringLiteral(
               "https://ws.stream.qqmusic.qq.com:999999999999999999999999/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("sip-nondecimal-port")
        << sanitizedPurl()
        << QStringLiteral("https://ws.stream.qqmusic.qq.com:port/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("sip-mixed-port")
        << sanitizedPurl()
        << QStringLiteral("https://ws.stream.qqmusic.qq.com:443x/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("sip-multiple-colon-port")
        << sanitizedPurl()
        << QStringLiteral("https://ws.stream.qqmusic.qq.com:443:443/")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("sip-fragment")
        << sanitizedPurl()
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/#fragment")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("sip-empty-fragment")
        << sanitizedPurl()
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/#")
        << QStringLiteral("qq.playback.url.invalid");
    QTest::newRow("wrong-prefix")
        << QStringLiteral("M800001SANITIZED01001SANITIZED01.mp3")
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.filename");
    QTest::newRow("compact-short-media-mid")
        << QStringLiteral("M500SHORT.mp3?vkey=not-a-live-token")
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.filename");
    QTest::newRow("compact-malformed-media-mid")
        << QStringLiteral(
               "M500002SANITIZED_2.mp3?vkey=not-a-live-token")
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.filename");
    QTest::newRow("compact-non-ascii-media-mid")
        << (QStringLiteral("M500002SANITIZED0") + QChar(0x0661)
            + QStringLiteral(".mp3?vkey=not-a-live-token"))
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.filename");
    QTest::newRow("normalized-dot-segments")
        << QStringLiteral("M500/../M800other.mp3")
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.invalid");
    QTest::newRow("percent-encoded-dot-segments")
        << QStringLiteral("M500%2F..%2FM800other.mp3")
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.invalid");
    QTest::newRow("slash")
        << sanitizedFilename() + QStringLiteral("/other.mp3")
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.invalid");
    QTest::newRow("backslash")
        << sanitizedFilename() + QStringLiteral("\\other.mp3")
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.invalid");
    QTest::newRow("percent-encoded-backslash")
        << sanitizedFilename() + QStringLiteral("%5Cother.mp3")
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.invalid");
    QTest::newRow("absolute")
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
           + sanitizedFilename()
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.invalid");
    QTest::newRow("authority")
        << QStringLiteral("//ws.stream.qqmusic.qq.com/")
           + sanitizedFilename()
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.invalid");
    QTest::newRow("authority-empty-userinfo")
        << QStringLiteral("//@ws.stream.qqmusic.qq.com/")
           + sanitizedFilename()
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.invalid");
    QTest::newRow("purl-fragment")
        << sanitizedFilename() + QStringLiteral("#fragment")
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.invalid");
    QTest::newRow("purl-empty-fragment")
        << sanitizedFilename() + QLatin1Char('#')
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.invalid");
    QTest::newRow("purl-unreviewed-query-key")
        << sanitizedFilename() + QStringLiteral("?token=not-reviewed")
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.invalid");
    QTest::newRow("purl-duplicate-query-key")
        << sanitizedFilename()
           + QStringLiteral("?vkey=first&vkey=second")
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.invalid");
    QTest::newRow("purl-alphabetic-uin")
        << sanitizedFilename() + QStringLiteral("?uin=account")
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.invalid");
    QTest::newRow("purl-signed-uin")
        << sanitizedFilename() + QStringLiteral("?uin=-123")
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.invalid");
    QTest::newRow("purl-unicode-uin")
        << sanitizedFilename() + QStringLiteral("?uin=") + QChar(0x0661)
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.invalid");
    QTest::newRow("purl-duplicate-uin")
        << sanitizedFilename() + QStringLiteral("?uin=&uin=123")
        << QStringLiteral("https://ws.stream.qqmusic.qq.com/")
        << QStringLiteral("qq.playback.purl.invalid");
    QTest::newRow("empty-sip")
        << sanitizedPurl() << QString()
        << QStringLiteral("qq.playback.url.invalid");
}

void QqPlaybackParserTest::rejectsUnsafePlaybackLocations()
{
    QFETCH(QString, purl);
    QFETCH(QString, sip);
    QFETCH(QString, diagnostic);
    const QDateTime now(
        QDate(2026, 7, 25), QTime(0, 0), Qt::UTC);
    const ServiceResult<PlaybackLocation> result = QqPlaybackParser::parse(
        withPlaybackValues(purl, sip), sanitizedTrackMid(), now, 25);

    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(25));
    QCOMPARE(result.error.diagnosticCode, diagnostic);
}

void QqPlaybackParserTest::convertsOffsetPlaybackExpiryToUtc()
{
    QDateTime now(
        QDate(2026, 7, 25), QTime(8, 0), Qt::OffsetFromUTC);
    now.setOffsetFromUtc(8 * 60 * 60);
    QVERIFY(now.isValid());
    QCOMPARE(now.timeSpec(), Qt::OffsetFromUTC);
    QCOMPARE(now.offsetFromUtc(), 8 * 60 * 60);

    const ServiceResult<PlaybackLocation> result = QqPlaybackParser::parse(
        fixture(QStringLiteral("vkey.json")), sanitizedTrackMid(), now, 30);

    QVERIFY(result.ok());
    QCOMPARE(result.value.expiresAt.timeSpec(), Qt::UTC);
    QCOMPARE(result.value.expiresAt.toMSecsSinceEpoch(),
             now.toMSecsSinceEpoch() + 600000);
    QVERIFY(result.value.expiresAt > now);
}

void QqPlaybackParserTest::acceptsLatestSafePlaybackExpiry()
{
    const qint64 maximum = std::numeric_limits<qint64>::max();
    const qint64 latestSafeNowMs = maximum - 600000;
    const QDateTime now = QDateTime::fromMSecsSinceEpoch(
        latestSafeNowMs, Qt::UTC);
    QVERIFY(now.isValid());
    QCOMPARE(now.toMSecsSinceEpoch(), latestSafeNowMs);

    const ServiceResult<PlaybackLocation> result = QqPlaybackParser::parse(
        fixture(QStringLiteral("vkey.json")), sanitizedTrackMid(), now, 29);

    QVERIFY(result.ok());
    QVERIFY(result.value.expiresAt.isValid());
    QCOMPARE(result.value.expiresAt.timeSpec(), Qt::UTC);
    QCOMPARE(result.value.expiresAt.toMSecsSinceEpoch(), maximum);
    QVERIFY(result.value.expiresAt > now);
}

void QqPlaybackParserTest::rejectsInvalidPlaybackExpiry_data()
{
    QTest::addColumn<QDateTime>("now");
    QTest::addColumn<qint64>("expectedNowMs");

    QTest::newRow("invalid-now") << QDateTime() << qint64(0);

    const qint64 firstOverflowingMs =
        std::numeric_limits<qint64>::max() - 599999;
    const QDateTime firstOverflowing = QDateTime::fromMSecsSinceEpoch(
        firstOverflowingMs, Qt::UTC);
    QTest::newRow("expiry-overflow")
        << firstOverflowing << firstOverflowingMs;
}

void QqPlaybackParserTest::rejectsInvalidPlaybackExpiry()
{
    QFETCH(QDateTime, now);
    QFETCH(qint64, expectedNowMs);
    if (now.isValid()) {
        QCOMPARE(now.toMSecsSinceEpoch(), expectedNowMs);
    }
    const ServiceResult<PlaybackLocation> result = QqPlaybackParser::parse(
        fixture(QStringLiteral("vkey.json")), sanitizedTrackMid(), now, 28);

    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::Internal);
    QCOMPARE(result.error.operationId, quint64(28));
    QCOMPARE(result.error.diagnosticCode,
             QStringLiteral("qq.playback.expiry.invalid"));
}

void QqPlaybackParserTest::rejectsChangedPlaybackSchemas()
{
    const QDateTime now(
        QDate(2026, 7, 25), QTime(0, 0), Qt::UTC);
    const ServiceResult<PlaybackLocation> missing = QqPlaybackParser::parse(
        withoutLocationMember(QStringLiteral("purl")), sanitizedTrackMid(),
        now, 26);
    QVERIFY(!missing.ok());
    QCOMPARE(missing.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(missing.error.operationId, quint64(26));
    QCOMPARE(missing.error.diagnosticCode,
             QStringLiteral("qq.playback.json.purl.string"));

    const ServiceResult<PlaybackLocation> emptyArray = QqPlaybackParser::parse(
        QByteArrayLiteral(
            "{\"req_1\":{\"data\":{\"midurlinfo\":[],"
            "\"sip\":[\"https://ws.stream.qqmusic.qq.com/\"]}}}"),
        sanitizedTrackMid(), now, 27);
    QVERIFY(!emptyArray.ok());
    QCOMPARE(emptyArray.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(emptyArray.error.operationId, quint64(27));
}

QTEST_APPLESS_MAIN(QqPlaybackParserTest)

#include "tst_qqplaybackparser.moc"
