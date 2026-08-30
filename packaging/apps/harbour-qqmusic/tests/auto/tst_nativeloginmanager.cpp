#include <QtTest>

#include "login/nativeloginmanager.h"
#include "login/nativemqttprotocol.h"

#include <QBuffer>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QUrlQuery>

namespace {

struct ReplyScript
{
    QByteArray body;
    int status = 200;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    bool neverFinish = false;
    QUrl redirectTarget;
};

class ScriptedReply : public QNetworkReply
{
    Q_OBJECT

public:
    ScriptedReply(const QNetworkRequest &request,
                  QNetworkAccessManager::Operation operation,
                  const ReplyScript &script,
                  QObject *parent)
        : QNetworkReply(parent)
        , m_body(script.body)
        , m_error(script.error)
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(operation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, script.status);
        if (script.redirectTarget.isValid()) {
            setAttribute(QNetworkRequest::RedirectionTargetAttribute,
                         script.redirectTarget);
        }
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        if (!script.neverFinish) {
            QTimer::singleShot(0, this, SLOT(deliver()));
        }
    }

    void abort() override
    {
        aborted = true;
        emit abortCalled();
        if (!isFinished()) {
            setError(QNetworkReply::OperationCanceledError,
                     QStringLiteral("cancelled"));
            setFinished(true);
            emit finished();
        }
    }

    qint64 bytesAvailable() const override
    {
        return m_body.size() - m_offset + QIODevice::bytesAvailable();
    }

    bool aborted = false;

signals:
    void abortCalled();

private slots:
    void deliver()
    {
        if (isFinished()) {
            return;
        }
        if (m_error != QNetworkReply::NoError) {
            setError(m_error, QStringLiteral("scripted"));
        }
        if (!m_body.isEmpty()) {
            emit readyRead();
        }
        setFinished(true);
        emit finished();
    }

protected:
    qint64 readData(char *data, qint64 maximum) override
    {
        const qint64 count = qMin(maximum, m_body.size() - m_offset);
        if (count <= 0) {
            return -1;
        }
        memcpy(data, m_body.constData() + m_offset, size_t(count));
        m_offset += count;
        return count;
    }

private:
    QByteArray m_body;
    qint64 m_offset = 0;
    QNetworkReply::NetworkError m_error;
};

class ScriptedNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT

public:
    QList<ReplyScript> scripts;
    QList<QNetworkAccessManager::Operation> operations;
    QList<QNetworkRequest> requests;
    QList<QByteArray> bodies;
    QList<QPointer<ScriptedReply> > replies;
    int abortCount = 0;

protected:
    QNetworkReply *createRequest(Operation operation,
                                 const QNetworkRequest &request,
                                 QIODevice *outgoingData) override
    {
        operations.append(operation);
        requests.append(request);
        bodies.append(outgoingData ? outgoingData->readAll() : QByteArray());
        ReplyScript script;
        if (!scripts.isEmpty()) {
            script = scripts.takeFirst();
        } else {
            script.error = QNetworkReply::UnknownNetworkError;
        }
        ScriptedReply *reply = new ScriptedReply(
            request, operation, script, this);
        connect(reply, &ScriptedReply::abortCalled,
                this, [this]() { ++abortCount; });
        replies.append(reply);
        return reply;
    }
};

class ScriptedTransport : public NativeLoginTransport
{
    Q_OBJECT

public:
    explicit ScriptedTransport(QObject *parent = 0)
        : NativeLoginTransport(parent)
    {
    }

    bool open(const QUrl &url,
              const QMap<QByteArray, QByteArray> &headers) override
    {
        urls.append(url);
        requestHeaders.append(headers);
        opened = true;
        return openSucceeds;
    }

    bool sendBinaryMessage(const QByteArray &message) override
    {
        sent.append(message);
        const bool result = opened && sendSucceeds;
        if (result && !synchronousReplies.isEmpty()) {
            const QByteArray reply = synchronousReplies.takeFirst();
            emit binaryMessageReceived(reply);
        }
        return result;
    }

    void close() override
    {
        opened = false;
        ++closeCount;
    }

    void connectNow()
    {
        emit connected();
    }

    void deliver(const QByteArray &message)
    {
        emit binaryMessageReceived(message);
    }

    void failNow(const QString &diagnosticCode)
    {
        emit failed(diagnosticCode);
    }

    QList<QUrl> urls;
    QList<QMap<QByteArray, QByteArray> > requestHeaders;
    QList<QByteArray> sent;
    QList<QByteArray> synchronousReplies;
    bool openSucceeds = true;
    bool sendSucceeds = true;
    bool opened = false;
    int closeCount = 0;
};

QByteArray pngData()
{
    QImage image(120, 120, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

QByteArray createResponse(
    const QString &qrId = QStringLiteral("challenge-1"))
{
    const QString dataUri = QStringLiteral("data:image/png;base64,")
        + QString::fromLatin1(pngData().toBase64());
    QJsonObject data;
    data.insert(QStringLiteral("qrcodeID"), qrId);
    data.insert(QStringLiteral("qrcode"), dataUri);
    data.insert(QStringLiteral("expiresIn"), 900);
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

QByteArray mqttProperty(const QByteArray &key, const QByteArray &value)
{
    QByteArray result(1, static_cast<char>(0x26));
    result.append(static_cast<char>((key.size() >> 8) & 0xff));
    result.append(static_cast<char>(key.size() & 0xff));
    result.append(key);
    result.append(static_cast<char>((value.size() >> 8) & 0xff));
    result.append(static_cast<char>(value.size() & 0xff));
    result.append(value);
    return result;
}

QByteArray mqttFrame(quint8 header, const QByteArray &body)
{
    QByteArray result(1, static_cast<char>(header));
    int value = body.size();
    do {
        quint8 encoded = static_cast<quint8>(value % 128);
        value /= 128;
        if (value != 0) {
            encoded |= 0x80;
        }
        result.append(static_cast<char>(encoded));
    } while (value != 0);
    result.append(body);
    return result;
}

QByteArray mqttPublish(
    const QByteArray &type,
    const QByteArray &payload,
    const QByteArray &topic =
        QByteArrayLiteral("management.qrcode_login/challenge-1"))
{
    const QByteArray properties =
        mqttProperty(QByteArrayLiteral("type"), type);
    QByteArray body;
    body.append(static_cast<char>((topic.size() >> 8) & 0xff));
    body.append(static_cast<char>(topic.size() & 0xff));
    body.append(topic);
    body.append(static_cast<char>(properties.size()));
    body.append(properties);
    body.append(payload);
    return mqttFrame(0x30, body);
}

QByteArray mqttCandidatePayload()
{
    QJsonObject cookies;
    cookies.insert(QStringLiteral("qqmusic_key"),
                   valueObject(QStringLiteral("sanitized-token")));
    cookies.insert(QStringLiteral("qqmusic_uin"),
                   valueObject(QStringLiteral("o10001")));
    QJsonObject candidate;
    candidate.insert(QStringLiteral("cookies"), cookies);
    return QJsonDocument(candidate).toJson(QJsonDocument::Compact);
}

QByteArray exchangeResponse()
{
    QJsonObject data;
    data.insert(QStringLiteral("musicid"), 10001);
    data.insert(QStringLiteral("musickey"),
                QStringLiteral("sanitized-final-key"));
    QJsonObject request;
    request.insert(QStringLiteral("code"), 0);
    request.insert(QStringLiteral("data"), data);
    QJsonObject root;
    root.insert(QStringLiteral("code"), 0);
    root.insert(QStringLiteral("req_0"), request);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

}

class NativeLoginManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void completesBoundedQrCandidateFlow()
    {
        ScriptedNetworkAccessManager network;
        ScriptedTransport transport;
        ReplyScript create;
        create.body = createResponse();
        ReplyScript exchange;
        exchange.body = exchangeResponse();
        network.scripts << create << exchange;

        NativeLoginLimits limits;
        limits.mqttRetryDelayMs = 1;
        NativeLoginManager manager(&network, &transport, limits);
        QList<NativeLoginManager::State> states;
        int qrCount = 0;
        int candidateCount = 0;
        QVector<CookieRecord> candidate;
        connect(&manager, &NativeLoginManager::stateChanged,
                this, [&states](NativeLoginManager::State state,
                                const QString &) {
            states.append(state);
        });
        connect(&manager, &NativeLoginManager::qrReady,
                this, [&qrCount](const QString &, const QByteArray &png,
                                 int width, int height, int generation) {
            ++qrCount;
            QVERIFY(png.startsWith(QByteArray::fromHex("89504e470d0a1a0a")));
            QCOMPARE(width, 120);
            QCOMPARE(height, 120);
            QCOMPARE(generation, 1);
        });
        connect(&manager, &NativeLoginManager::candidateReady,
                this, [&candidateCount, &candidate](
                    const QString &, const QVector<CookieRecord> &records) {
            ++candidateCount;
            candidate = records;
        });

        manager.start();
        QTRY_COMPARE(transport.urls.size(), 1);
        QCOMPARE(transport.urls.first().scheme(), QStringLiteral("wss"));
        QCOMPARE(transport.urls.first().host(), QStringLiteral("mu.y.qq.com"));
        QCOMPARE(transport.urls.first().path(), QStringLiteral("/ws/handshake"));
        QCOMPARE(transport.requestHeaders.first().value(
                     QByteArrayLiteral("Sec-WebSocket-Protocol")),
                 QByteArrayLiteral("mqtt"));
        transport.connectNow();
        QTRY_COMPARE(transport.sent.size(), 1);
        QCOMPARE(static_cast<quint8>(transport.sent.at(0).at(0)), quint8(0x10));
        transport.deliver(QByteArray::fromHex("2003000000"));
        QTRY_COMPARE(transport.sent.size(), 2);
        QCOMPARE(static_cast<quint8>(transport.sent.at(1).at(0)), quint8(0x82));
        transport.deliver(QByteArray::fromHex("900400010000"));
        transport.deliver(mqttPublish(QByteArrayLiteral("scanned"),
                                      QByteArray()));
        QTRY_COMPARE(manager.state(), NativeLoginManager::Scanned);
        transport.deliver(mqttPublish(QByteArrayLiteral("cookies"),
                                      mqttCandidatePayload()));
        QTRY_COMPARE(candidateCount, 1);
        QCOMPARE(manager.attemptId().size(), 36);
        QVERIFY(!manager.attemptId().contains(QLatin1Char('{')));
        QVERIFY(!manager.attemptId().contains(QLatin1Char('}')));
        QCOMPARE(qrCount, 1);
        QCOMPARE(candidate.size(), 4);
        QCOMPARE(manager.state(), NativeLoginManager::CandidateReady);
        QVERIFY(!manager.active());
        QCOMPARE(network.requests.size(), 2);
        QCOMPARE(network.operations.size(), 2);
        for (int index = 0; index < network.operations.size(); ++index) {
            QCOMPARE(network.operations.at(index),
                     QNetworkAccessManager::PostOperation);
        }
        QCOMPARE(network.requests.at(0).url().host(),
                 QStringLiteral("u6.y.qq.com"));
        QCOMPARE(network.requests.at(1).url().host(),
                 QStringLiteral("u.y.qq.com"));
        QCOMPARE(network.requests.at(1).url().path(),
                 QStringLiteral("/cgi-bin/musicu.fcg"));
        QVERIFY(!network.requests.at(1).url().hasQuery());
        QCOMPARE(network.requests.at(0).rawHeader("Content-Type"),
                 QByteArrayLiteral("application/x-www-form-urlencoded"));
        QCOMPARE(network.requests.at(1).rawHeader("Content-Type"),
                 QByteArrayLiteral("application/json"));
        QCOMPARE(network.requests.at(0).rawHeader("Origin"),
                 QByteArrayLiteral("https://y.qq.com"));
        QCOMPARE(network.requests.at(0).rawHeader("Referer"),
                 QByteArrayLiteral(
                     "https://y.qq.com/m/client/qr_code_login/index.html"));
        QCOMPARE(network.requests.at(1).rawHeader("User-Agent"),
                 QByteArrayLiteral("QQMusic 14090008(android 10)"));
        QVERIFY(network.requests.at(1).rawHeader("Origin").isEmpty());
        QVERIFY(network.requests.at(1).rawHeader("Referer").isEmpty());
        QCOMPARE(QJsonDocument::fromJson(network.bodies.at(0)).object()
                     .value(QStringLiteral("req_0")).toObject()
                     .value(QStringLiteral("method")).toString(),
                 QStringLiteral("CreateQRCode"));
        QCOMPARE(QJsonDocument::fromJson(network.bodies.at(1)).object()
                     .value(QStringLiteral("req_0")).toObject()
                     .value(QStringLiteral("method")).toString(),
                 QStringLiteral("Login"));
        QCOMPARE(QUrlQuery(network.requests.at(0).url()).queryItemValue(
                     QStringLiteral("_webcgikey")),
                 QStringLiteral("CreateQRCode"));
        QVERIFY(states.contains(NativeLoginManager::Opening));
        QVERIFY(states.contains(NativeLoginManager::QrReady));
        QVERIFY(states.contains(NativeLoginManager::Scanned));
        QVERIFY(states.contains(NativeLoginManager::Exchanging));
        QVERIFY(states.contains(NativeLoginManager::CandidateReady));
    }

    void sendsConnectForProtocolBoundaryQrId()
    {
        ScriptedNetworkAccessManager network;
        ScriptedTransport transport;
        ReplyScript create;
        create.body = createResponse(QString(256, QLatin1Char('q')));
        network.scripts << create;
        NativeLoginManager manager(
            &network, &transport, NativeLoginLimits());
        int failureCount = 0;
        connect(&manager, &NativeLoginManager::attemptFailed,
                this, [&failureCount](const QString &, const ServiceError &) {
            ++failureCount;
        });

        manager.start();
        QTRY_COMPARE(transport.urls.size(), 1);
        transport.connectNow();
        QTRY_COMPARE(transport.sent.size(), 1);
        QVERIFY(!transport.sent.first().isEmpty());
        QCOMPARE(failureCount, 0);
        QCOMPARE(manager.state(), NativeLoginManager::QrReady);
    }

    void distinguishesConnectSendFailure()
    {
        ScriptedNetworkAccessManager network;
        ScriptedTransport transport;
        transport.sendSucceeds = false;
        ReplyScript create;
        create.body = createResponse();
        network.scripts << create;
        NativeLoginLimits limits;
        limits.maximumMqttFailures = 0;
        NativeLoginManager manager(&network, &transport, limits);
        ServiceError failure;
        int failureCount = 0;
        connect(&manager, &NativeLoginManager::attemptFailed,
                this, [&failure, &failureCount](
                    const QString &, const ServiceError &error) {
            failure = error;
            ++failureCount;
        });

        manager.start();
        QTRY_COMPARE(transport.urls.size(), 1);
        transport.connectNow();
        QTRY_COMPARE(failureCount, 1);
        QCOMPARE(manager.state(), NativeLoginManager::Failed);
        QCOMPARE(failure.kind, ErrorKind::Offline);
        QCOMPARE(failure.diagnosticCode,
                 QStringLiteral("qq.login.mqtt.connect.send"));
    }

    void acceptsSynchronousAcksDuringSends()
    {
        ScriptedNetworkAccessManager network;
        ScriptedTransport transport;
        transport.synchronousReplies
            << QByteArray::fromHex("2003000000")
            << QByteArray::fromHex("900400010000");
        ReplyScript create;
        create.body = createResponse();
        network.scripts << create;
        NativeLoginManager manager(
            &network, &transport, NativeLoginLimits());
        ServiceError failure;
        int failureCount = 0;
        connect(&manager, &NativeLoginManager::attemptFailed,
                this, [&failure, &failureCount](
                    const QString &, const ServiceError &error) {
            failure = error;
            ++failureCount;
        });

        manager.start();
        QTRY_COMPARE(transport.urls.size(), 1);
        transport.connectNow();
        QTRY_COMPARE(transport.sent.size(), 2);
        QCOMPARE(static_cast<quint8>(transport.sent.at(0).at(0)), quint8(0x10));
        QCOMPARE(static_cast<quint8>(transport.sent.at(1).at(0)), quint8(0x82));
        QTest::qWait(1);
        QCOMPARE(failureCount, 0);
        QCOMPARE(manager.state(), NativeLoginManager::QrReady);
        QVERIFY(failure.diagnosticCode.isEmpty());
    }

    void cancellationAbortsAttemptOwnedReply()
    {
        ScriptedNetworkAccessManager network;
        ScriptedTransport transport;
        ReplyScript create;
        create.neverFinish = true;
        network.scripts << create;
        NativeLoginManager manager(&network, &transport, NativeLoginLimits());

        manager.start();
        QCOMPARE(manager.state(), NativeLoginManager::Opening);
        QCOMPARE(network.replies.size(), 1);
        manager.cancel();
        QCOMPARE(manager.state(), NativeLoginManager::Cancelled);
        QVERIFY(!manager.active());
        QCOMPARE(network.abortCount, 1);
    }

    void requestTimeoutIsTerminalAndRedacted()
    {
        ScriptedNetworkAccessManager network;
        ScriptedTransport transport;
        ReplyScript create;
        create.neverFinish = true;
        network.scripts << create;
        NativeLoginLimits limits;
        limits.createTimeoutMs = 10;
        NativeLoginManager manager(&network, &transport, limits);
        ServiceError failure;
        int failureCount = 0;
        connect(&manager, &NativeLoginManager::attemptFailed,
                this, [&failure, &failureCount](const QString &,
                                                const ServiceError &error) {
            failure = error;
            ++failureCount;
        });

        manager.start();
        QTRY_COMPARE(failureCount, 1);
        QCOMPARE(manager.state(), NativeLoginManager::TimedOut);
        QCOMPARE(failure.kind, ErrorKind::Timeout);
        QCOMPARE(failure.diagnosticCode,
                 QStringLiteral("qq.login.request.timeout"));
        QCOMPARE(network.abortCount, 1);
    }

    void oversizedCreateResponseIsAbortedBeforeParsing()
    {
        ScriptedNetworkAccessManager network;
        ScriptedTransport transport;
        ReplyScript create;
        create.body = QByteArray(
            NativeLoginProtocol::maximumCreateResponseBytes() + 1, 'x');
        network.scripts << create;
        NativeLoginManager manager(&network, &transport, NativeLoginLimits());
        ServiceError failure;
        int failureCount = 0;
        connect(&manager, &NativeLoginManager::attemptFailed,
                this, [&failure, &failureCount](const QString &,
                                                const ServiceError &error) {
            failure = error;
            ++failureCount;
        });

        manager.start();
        QTRY_COMPARE(failureCount, 1);
        QCOMPARE(manager.state(), NativeLoginManager::Failed);
        QCOMPARE(failure.kind, ErrorKind::DecodeFailed);
        QCOMPARE(failure.diagnosticCode,
                 QStringLiteral("qq.login.response.size"));
        QCOMPARE(network.abortCount, 1);
    }

    void rejectsRedirectWithoutFollowingIt()
    {
        ScriptedNetworkAccessManager network;
        ScriptedTransport transport;
        ReplyScript create;
        create.body = createResponse();
        create.redirectTarget = QUrl(QStringLiteral(
            "https://redirect.invalid/login"));
        network.scripts << create;
        NativeLoginManager manager(&network, &transport, NativeLoginLimits());
        int failureCount = 0;
        connect(&manager, &NativeLoginManager::attemptFailed,
                this, [&failureCount](const QString &, const ServiceError &) {
            ++failureCount;
        });

        manager.start();
        QTRY_COMPARE(failureCount, 1);
        QCOMPARE(network.requests.size(), 1);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
        QCOMPARE(network.requests.first().attribute(
                     QNetworkRequest::RedirectPolicyAttribute).toInt(),
                 int(QNetworkRequest::ManualRedirectPolicy));
#else
        QCOMPARE(network.requests.first().attribute(
                     QNetworkRequest::FollowRedirectsAttribute).toBool(),
                 false);
#endif
        QCOMPARE(manager.state(), NativeLoginManager::Failed);
        QVERIFY(!manager.active());
    }

    void rejectsWrongTopicAndMalformedFrame()
    {
        for (int mode = 0; mode < 2; ++mode) {
            ScriptedNetworkAccessManager network;
            ScriptedTransport transport;
            ReplyScript create;
            create.body = createResponse();
            network.scripts << create;
            NativeLoginManager manager(
                &network, &transport, NativeLoginLimits());
            ServiceError failure;
            int failureCount = 0;
            connect(&manager, &NativeLoginManager::attemptFailed,
                    this, [&failure, &failureCount](
                        const QString &, const ServiceError &error) {
                failure = error;
                ++failureCount;
            });

            manager.start();
            QTRY_COMPARE(transport.urls.size(), 1);
            transport.connectNow();
            QTRY_COMPARE(transport.sent.size(), 1);
            transport.deliver(QByteArray::fromHex("2003000000"));
            QTRY_COMPARE(transport.sent.size(), 2);
            transport.deliver(QByteArray::fromHex("900400010000"));
            if (mode == 0) {
                transport.deliver(mqttPublish(
                    QByteArrayLiteral("scanned"), QByteArray(),
                    QByteArrayLiteral(
                        "management.qrcode_login/different-challenge")));
            } else {
                transport.deliver(QByteArray::fromHex("3000"));
            }

            QTRY_COMPARE(failureCount, 1);
            QCOMPARE(manager.state(), NativeLoginManager::Failed);
            QCOMPARE(failure.kind, ErrorKind::SchemaChanged);
            QVERIFY(failure.diagnosticCode.startsWith(
                QStringLiteral("qq.login.mqtt.")));
        }
    }

    void retriesTransportWithinLimitAndCancellationStopsIt()
    {
        ScriptedNetworkAccessManager network;
        ScriptedTransport transport;
        ReplyScript create;
        create.body = createResponse();
        network.scripts << create;
        NativeLoginLimits limits;
        limits.mqttRetryDelayMs = 1;
        limits.maximumMqttFailures = 1;
        NativeLoginManager manager(&network, &transport, limits);

        manager.start();
        QTRY_COMPARE(transport.urls.size(), 1);
        transport.failNow(QStringLiteral("qq.login.websocket.network"));
        QTRY_COMPARE(transport.urls.size(), 2);
        const int closeCountBeforeCancel = transport.closeCount;
        manager.cancel();
        QCOMPARE(manager.state(), NativeLoginManager::Cancelled);
        QCOMPARE(transport.closeCount, closeCountBeforeCancel + 1);
        QTest::qWait(10);
        QCOMPARE(transport.urls.size(), 2);
    }
};

QTEST_GUILESS_MAIN(NativeLoginManagerTest)
#include "tst_nativeloginmanager.moc"
