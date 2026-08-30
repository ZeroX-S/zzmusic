#include <QtTest>

#include "login/nativeloginprotocol.h"

#include <QBuffer>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

namespace {

QByteArray pngData(int width = 120, int height = 120)
{
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        return QByteArray();
    }
    return bytes;
}

QByteArray createResponse(const QString &qrData,
                          const QString &qrId = QStringLiteral("challenge-1"),
                          int expires = 900)
{
    QJsonObject data;
    data.insert(QStringLiteral("qrcodeID"), qrId);
    data.insert(QStringLiteral("qrcode"), qrData);
    data.insert(QStringLiteral("expiresIn"), expires);
    QJsonObject request;
    request.insert(QStringLiteral("code"), 0);
    request.insert(QStringLiteral("data"), data);
    QJsonObject root;
    root.insert(QStringLiteral("code"), 0);
    root.insert(QStringLiteral("req_0"), request);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QJsonObject valueObject(const QString &value)
{
    QJsonObject result;
    result.insert(QStringLiteral("value"), value);
    return result;
}

QByteArray exchangeResponse(const QJsonValue &uin,
                            const QString &musicKey)
{
    QJsonObject data;
    data.insert(QStringLiteral("musicid"), uin);
    data.insert(QStringLiteral("musickey"), musicKey);
    QJsonObject request;
    request.insert(QStringLiteral("code"), 0);
    request.insert(QStringLiteral("data"), data);
    QJsonObject root;
    root.insert(QStringLiteral("code"), 0);
    root.insert(QStringLiteral("req_0"), request);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

}

class NativeLoginProtocolTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsCurrentMobileLoginRequestsAndFixedEndpoints()
    {
        const QByteArray signedBody(
            "{\"comm\":{},\"req_0\":{\"method\":\"CreateQRCode\"}}");
        QCOMPARE(NativeLoginProtocol::signBody(signedBody),
                 QByteArray("zzcdccd700pzg3vokcujrpnbo2tbymtthrto74580c54"));

        const QJsonObject create = QJsonDocument::fromJson(
            NativeLoginProtocol::createRequestBody()).object();
        const QJsonObject createComm =
            create.value(QStringLiteral("comm")).toObject();
        const QJsonObject createRequest =
            create.value(QStringLiteral("req_0")).toObject();
        const QJsonObject createParameters =
            createRequest.value(QStringLiteral("param")).toObject();
        QCOMPARE(createComm.value(QStringLiteral("ct")).toInt(), 23);
        QCOMPARE(createComm.value(QStringLiteral("cv")).toInt(), 0);
        QCOMPARE(createComm.value(QStringLiteral("platform")).toString(),
                 QStringLiteral("h5"));
        QVERIFY(!createComm.contains(QStringLiteral("tmeAppID")));
        QCOMPARE(createRequest.value(QStringLiteral("module")).toString(),
                 QStringLiteral("music.login.LoginServer"));
        QCOMPARE(createRequest.value(QStringLiteral("method")).toString(),
                 QStringLiteral("CreateQRCode"));
        QCOMPARE(createParameters.value(QStringLiteral("tmeAppID")).toString(),
                 QStringLiteral("qqmusic"));
        QCOMPARE(createParameters.value(QStringLiteral("ct")).toInt(), 11);
        QCOMPARE(createParameters.value(QStringLiteral("cv")).toInt(),
                 20030508);

        const QByteArray exchangeBody =
            NativeLoginProtocol::exchangeRequestBody(
                QStringLiteral("o10001"), QStringLiteral("challenge-1"),
                QByteArrayLiteral("sanitized-token"));
        const QJsonObject exchange =
            QJsonDocument::fromJson(exchangeBody).object();
        const QJsonObject exchangeComm =
            exchange.value(QStringLiteral("comm")).toObject();
        const QJsonObject exchangeRequest =
            exchange.value(QStringLiteral("req_0")).toObject();
        const QJsonObject exchangeParameters =
            exchangeRequest.value(QStringLiteral("param")).toObject();
        QCOMPARE(exchangeComm.value(QStringLiteral("ct")).toInt(), 11);
        QCOMPARE(exchangeComm.value(QStringLiteral("cv")).toInt(), 14090008);
        QCOMPARE(exchangeComm.value(QStringLiteral("v")).toInt(), 14090008);
        QCOMPARE(exchangeComm.value(QStringLiteral("chid")).toString(),
                 QStringLiteral("10003505"));
        QCOMPARE(exchangeComm.value(QStringLiteral("tmeAppID")).toString(),
                 QStringLiteral("qqmusic"));
        QCOMPARE(exchangeComm.value(QStringLiteral("tmeLoginType")).toInt(),
                 6);
        QCOMPARE(exchangeComm.value(QStringLiteral("format")).toString(),
                 QStringLiteral("json"));
        QCOMPARE(exchangeComm.value(QStringLiteral("inCharset")).toString(),
                 QStringLiteral("utf-8"));
        QCOMPARE(exchangeComm.value(QStringLiteral("outCharset")).toString(),
                 QStringLiteral("utf-8"));
        QCOMPARE(exchangeComm.value(QStringLiteral("QIMEI36")).toString(),
                 QStringLiteral("6c9d3cd110abca9b16311cee10001e717614"));
        QCOMPARE(exchangeRequest.value(QStringLiteral("method")).toString(),
                 QStringLiteral("Login"));
        QCOMPARE(exchangeParameters.value(QStringLiteral("musicid")).toInt(),
                 10001);
        QCOMPARE(exchangeParameters.value(QStringLiteral("qrCodeID")).toString(),
                 QStringLiteral("challenge-1"));
        QCOMPARE(exchangeParameters.value(QStringLiteral("token")).toString(),
                 QStringLiteral("sanitized-token"));
        QCOMPARE(exchangeParameters.size(), 3);

        const QUrl service = NativeLoginProtocol::serviceUrl(
            QStringLiteral("Login"), exchangeBody, 1784937600000LL);
        QCOMPARE(service.scheme(), QStringLiteral("https"));
        QCOMPARE(service.host(), QStringLiteral("u.y.qq.com"));
        QCOMPARE(service.path(), QStringLiteral("/cgi-bin/musicu.fcg"));
        QVERIFY(!service.hasQuery());
        const QByteArray createBody = NativeLoginProtocol::createRequestBody();
        const QUrl createService = NativeLoginProtocol::serviceUrl(
            QStringLiteral("CreateQRCode"), createBody, 1784937600000LL);
        QCOMPARE(createService.host(), QStringLiteral("u6.y.qq.com"));
        QCOMPARE(createService.path(), QStringLiteral("/cgi-bin/musics.fcg"));
        const QUrlQuery createQuery(createService);
        QCOMPARE(createQuery.queryItems().size(), 3);
        QCOMPARE(createQuery.queryItemValue(QStringLiteral("_webcgikey")),
                 QStringLiteral("CreateQRCode"));
        QCOMPARE(createQuery.queryItemValue(QStringLiteral("sign")),
                 QString::fromLatin1(NativeLoginProtocol::signBody(createBody)));
        QCOMPARE(createQuery.queryItemValue(QStringLiteral("_")),
                 QStringLiteral("1784937600000"));
        QVERIFY(NativeLoginProtocol::serviceUrl(
                    QStringLiteral("Unknown"), exchangeBody,
                    1784937600000LL).isEmpty());
        const QUrl mqtt = NativeLoginProtocol::mqttWebSocketUrl();
        QCOMPARE(mqtt.scheme(), QStringLiteral("wss"));
        QCOMPARE(mqtt.host(), QStringLiteral("mu.y.qq.com"));
        QCOMPARE(mqtt.path(), QStringLiteral("/ws/handshake"));
        QCOMPARE(NativeLoginProtocol::mqttWebSocketUrl(
                     QStringLiteral("2.2.2.2:29001")).path(),
                 QStringLiteral("/ws/handshake/2.2.2.2:29001"));
        QVERIFY(NativeLoginProtocol::mqttWebSocketUrl(
                    QStringLiteral("../redirect")).isEmpty());
    }

    void acceptsBoundedInlinePngChallenge()
    {
        const QByteArray png = pngData();
        QVERIFY(!png.isEmpty());
        const QString dataUri = QStringLiteral("data:image/png;base64,")
            + QString::fromLatin1(png.toBase64());
        const ServiceResult<NativeQrChallenge> result =
            NativeLoginProtocol::parseCreateResponse(createResponse(dataUri));
        QVERIFY(result.ok());
        QCOMPARE(result.value.qrId, QStringLiteral("challenge-1"));
        QCOMPARE(result.value.png, png);
        QCOMPARE(result.value.width, 120);
        QCOMPARE(result.value.height, 120);
        QCOMPARE(result.value.expiresSeconds, 900);
    }

    void rejectsRemoteMalformedAndOversizedChallenges()
    {
        QVERIFY(!NativeLoginProtocol::parseCreateResponse(
            createResponse(QStringLiteral("https://example.invalid/qr.png"))).ok());
        QVERIFY(!NativeLoginProtocol::parseCreateResponse(
            createResponse(QStringLiteral("data:image/png;base64,not-base64"))).ok());
        QVERIFY(!NativeLoginProtocol::parseCreateResponse(
            createResponse(QStringLiteral("data:image/png;base64,")
                           + QString::fromLatin1(
                               QByteArray(600 * 1024, 'x').toBase64()))).ok());
        QVERIFY(!NativeLoginProtocol::parseCreateResponse(
            createResponse(QStringLiteral("data:image/png;base64,")
                           + QString::fromLatin1(
                               pngData(1200, 120).toBase64()))).ok());
    }

    void parsesMqttStatusAndCandidateEvents()
    {
        const ServiceResult<NativeLoginEvent> scanned =
            NativeLoginProtocol::parseMqttEvent(
                QStringLiteral("scanned"), QByteArray(),
                QStringLiteral("challenge-1"));
        QVERIFY(scanned.ok());
        QCOMPARE(scanned.value.kind, NativeLoginEvent::Scanned);

        QJsonObject cookies;
        cookies.insert(QStringLiteral("qqmusic_key"),
                       valueObject(QStringLiteral("sanitized-token")));
        cookies.insert(QStringLiteral("qqmusic_uin"),
                       valueObject(QStringLiteral("o10001")));
        QJsonObject candidatePayload;
        candidatePayload.insert(QStringLiteral("cookies"), cookies);

        const ServiceResult<NativeLoginEvent> result =
            NativeLoginProtocol::parseMqttEvent(
                QStringLiteral("cookies"),
                QJsonDocument(candidatePayload).toJson(QJsonDocument::Compact),
                QStringLiteral("challenge-1"));
        QVERIFY(result.ok());
        QCOMPARE(result.value.kind, NativeLoginEvent::Candidate);
        QCOMPARE(result.value.musicUin, QStringLiteral("o10001"));
        QCOMPARE(result.value.qrId, QStringLiteral("challenge-1"));
    }

    void rejectsCandidateForAnotherChallengeAndUnknownEvent()
    {
        QJsonObject cookies;
        cookies.insert(QStringLiteral("qqmusic_key"),
                       valueObject(QStringLiteral("sanitized-token")));
        cookies.insert(QStringLiteral("qqmusic_uin"),
                       valueObject(QStringLiteral("o10001")));
        cookies.insert(QStringLiteral("qrcode_id"),
                       valueObject(QStringLiteral("different-challenge")));
        QJsonObject payload;
        payload.insert(QStringLiteral("cookies"), cookies);
        QVERIFY(!NativeLoginProtocol::parseMqttEvent(
            QStringLiteral("cookies"),
            QJsonDocument(payload).toJson(QJsonDocument::Compact),
            QStringLiteral("challenge-1")).ok());

        cookies.insert(QStringLiteral("qrcode_id"),
                       valueObject(QStringLiteral("challenge-1")));
        cookies.insert(QStringLiteral("qqmusic_key"),
                       valueObject(QString::fromUtf8("non-latin-\xE5\xAF\x86\xE9\x92\xA5")));
        payload.insert(QStringLiteral("cookies"), cookies);
        QVERIFY(!NativeLoginProtocol::parseMqttEvent(
            QStringLiteral("cookies"),
            QJsonDocument(payload).toJson(QJsonDocument::Compact),
            QStringLiteral("challenge-1")).ok());

        QVERIFY(!NativeLoginProtocol::parseMqttEvent(
            QStringLiteral("unexpected"), QByteArray(),
            QStringLiteral("challenge-1")).ok());
    }

    void acceptsEmptyPayloadForNonCredentialEvents()
    {
        const QStringList eventTypes = QStringList()
            << QStringLiteral("scanned")
            << QStringLiteral("canceled")
            << QStringLiteral("timeout")
            << QStringLiteral("loginFailed");
        for (int index = 0; index < eventTypes.size(); ++index) {
            const ServiceResult<NativeLoginEvent> result =
                NativeLoginProtocol::parseMqttEvent(
                    eventTypes.at(index), QByteArray(),
                    QStringLiteral("challenge-1"));
            QVERIFY2(result.ok(), qPrintable(eventTypes.at(index)));
        }
    }

    void convertsExchangeIntoReviewedCandidateCookies()
    {
        const QDateTime now = QDateTime::fromMSecsSinceEpoch(
            1784937600000LL, Qt::UTC);
        const ServiceResult<QVector<CookieRecord> > result =
            NativeLoginProtocol::parseExchangeResponse(
                exchangeResponse(QStringLiteral("10001"),
                                 QStringLiteral("sanitized-final-key")), now);
        QVERIFY(result.ok());
        QCOMPARE(result.value.size(), 4);
        for (int index = 0; index < result.value.size(); ++index) {
            const CookieRecord &record = result.value.at(index);
            QVERIFY(CookiePolicy::isAllowedName(record.name));
            QCOMPARE(record.domain, QStringLiteral(".qq.com"));
            QCOMPARE(record.path, QStringLiteral("/"));
            QVERIFY(record.secure);
            QVERIFY(record.expiresAt > now);
            QVERIFY(record.expiresAt <= now.addDays(400));
        }

        const ServiceResult<QVector<CookieRecord> > numericResult =
            NativeLoginProtocol::parseExchangeResponse(
                exchangeResponse(QJsonValue(10001),
                                 QStringLiteral("sanitized-final-key")), now);
        QVERIFY(numericResult.ok());
        QCOMPARE(numericResult.value.size(), 4);
    }

    void rejectsMalformedOrSensitiveExchangeFields()
    {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        QVERIFY(!NativeLoginProtocol::parseExchangeResponse(
            exchangeResponse(QStringLiteral("not-numeric"),
                             QStringLiteral("sanitized-final-key")), now).ok());
        QVERIFY(!NativeLoginProtocol::parseExchangeResponse(
            exchangeResponse(QStringLiteral("10001"),
                             QString::fromLatin1(QByteArray(5000, 'x'))), now).ok());
        QVERIFY(!NativeLoginProtocol::parseExchangeResponse(
            exchangeResponse(QStringLiteral("10001"),
                             QString::fromUtf8("non-latin-\xE5\xAF\x86\xE9\x92\xA5")), now).ok());
        QVERIFY(!NativeLoginProtocol::parseExchangeResponse(
            exchangeResponse(QJsonValue(10001.5),
                             QStringLiteral("sanitized-final-key")), now).ok());
        QVERIFY(!NativeLoginProtocol::parseExchangeResponse(
            QByteArray("{not-json"), now).ok());
    }

    void reportsRedactedExchangeResponseFailureStage()
    {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        QJsonObject rootCode;
        rootCode.insert(QStringLiteral("code"), 1);
        QCOMPARE(NativeLoginProtocol::parseExchangeResponse(
                     QJsonDocument(rootCode).toJson(QJsonDocument::Compact), now)
                     .error.diagnosticCode,
                 QStringLiteral("qq.login.exchange.root.code"));

        QJsonObject requestCode;
        requestCode.insert(QStringLiteral("code"), 1);
        QJsonObject root;
        root.insert(QStringLiteral("code"), 0);
        root.insert(QStringLiteral("req_0"), requestCode);
        QCOMPARE(NativeLoginProtocol::parseExchangeResponse(
                     QJsonDocument(root).toJson(QJsonDocument::Compact), now)
                     .error.diagnosticCode,
                 QStringLiteral("qq.login.exchange.request.code"));

        requestCode.insert(QStringLiteral("code"), 0);
        requestCode.insert(QStringLiteral("data"), QStringLiteral("redacted"));
        root.insert(QStringLiteral("req_0"), requestCode);
        QCOMPARE(NativeLoginProtocol::parseExchangeResponse(
                     QJsonDocument(root).toJson(QJsonDocument::Compact), now)
                     .error.diagnosticCode,
                 QStringLiteral("qq.login.exchange.request.data"));
    }
};

QTEST_GUILESS_MAIN(NativeLoginProtocolTest)
#include "tst_nativeloginprotocol.moc"
