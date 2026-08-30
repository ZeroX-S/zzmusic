#include "nativeloginmanager.h"
#include "nativemqttprotocol.h"

#include <QDateTime>
#include <QDebug>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>
#include <QVariant>

namespace {

QByteArray userAgent()
{
    return QByteArrayLiteral(
        "Mozilla/5.0 (Linux; Sailfish OS) AppleWebKit/537.36 "
        "Mobile Safari/537.36 QQMusic/20.3.5.8");
}

QByteArray loginUserAgent()
{
    return QByteArrayLiteral("QQMusic 14090008(android 10)");
}

QString requestDiagnostic(NativeLoginManager::State state)
{
    switch (state) {
    case NativeLoginManager::Opening:
        return QStringLiteral("create");
    case NativeLoginManager::Exchanging:
        return QStringLiteral("exchange");
    default:
        return QStringLiteral("unexpected");
    }
}

QByteArray mqttClientId()
{
    const quint32 suffix = qHash(QUuid::createUuid().toString()) % 9000 + 1000;
    return QByteArray::number(QDateTime::currentMSecsSinceEpoch())
        + QByteArray::number(suffix);
}

QMap<QByteArray, QByteArray> mqttHeaders()
{
    QMap<QByteArray, QByteArray> headers;
    headers.insert(QByteArrayLiteral("Origin"),
                   QByteArrayLiteral("https://y.qq.com"));
    headers.insert(QByteArrayLiteral("Referer"),
                   QByteArrayLiteral("https://y.qq.com/"));
    headers.insert(QByteArrayLiteral("Sec-WebSocket-Protocol"),
                   QByteArrayLiteral("mqtt"));
    headers.insert(QByteArrayLiteral("User-Agent"), userAgent());
    return headers;
}

QString newAttemptId()
{
    const QString uuid = QUuid::createUuid().toString();
    if (uuid.size() == 38 && uuid.startsWith(QLatin1Char('{'))
            && uuid.endsWith(QLatin1Char('}'))) {
        return uuid.mid(1, 36);
    }
    return uuid;
}

}

NativeLoginManager::NativeLoginManager(QObject *parent)
    : NativeLoginManager(new QNetworkAccessManager,
                         new NativeWebSocketTransport,
                         NativeLoginLimits(), parent)
{
    m_networkManager->setParent(this);
    m_transport->setParent(this);
}

NativeLoginManager::NativeLoginManager(
    QNetworkAccessManager *networkManager,
    const NativeLoginLimits &limits,
    QObject *parent)
    : NativeLoginManager(networkManager, new NativeWebSocketTransport,
                         limits, parent)
{
    m_transport->setParent(this);
}

NativeLoginManager::NativeLoginManager(
    QNetworkAccessManager *networkManager,
    NativeLoginTransport *transport,
    const NativeLoginLimits &limits,
    QObject *parent)
    : QObject(parent)
    , m_networkManager(networkManager)
    , m_transport(transport)
    , m_limits(limits)
    , m_requestTimer(new QTimer(this))
    , m_attemptTimer(new QTimer(this))
    , m_retryTimer(new QTimer(this))
    , m_keepAliveTimer(new QTimer(this))
    , m_state(Idle)
    , m_requestKind(RequestKind::None)
    , m_mqttPhase(MqttPhase::None)
    , m_maximumResponseBytes(0)
    , m_generation(0)
    , m_mqttFailures(0)
    , m_mqttRedirects(0)
    , m_closingMqtt(false)
{
    m_requestTimer->setSingleShot(true);
    m_attemptTimer->setSingleShot(true);
    m_retryTimer->setSingleShot(true);
    m_keepAliveTimer->setSingleShot(false);
    connect(m_requestTimer, &QTimer::timeout,
            this, &NativeLoginManager::requestTimedOut);
    connect(m_attemptTimer, &QTimer::timeout,
            this, &NativeLoginManager::attemptTimedOut);
    connect(m_retryTimer, &QTimer::timeout,
            this, &NativeLoginManager::retryMqtt);
    connect(m_keepAliveTimer, &QTimer::timeout,
            this, &NativeLoginManager::mqttKeepAlive);
    if (m_transport) {
        connect(m_transport, &NativeLoginTransport::connected,
                this, &NativeLoginManager::mqttConnected);
        connect(m_transport, &NativeLoginTransport::binaryMessageReceived,
                this, &NativeLoginManager::mqttBinaryMessageReceived);
        connect(m_transport, &NativeLoginTransport::disconnected,
                this, &NativeLoginManager::mqttDisconnected);
        connect(m_transport, &NativeLoginTransport::failed,
                this, &NativeLoginManager::mqttFailed);
    }
}

NativeLoginManager::~NativeLoginManager()
{
    clearReply();
    clearAttemptData();
}

NativeLoginManager::State NativeLoginManager::state() const
{
    return m_state;
}

QString NativeLoginManager::attemptId() const
{
    return m_attemptId;
}

bool NativeLoginManager::active() const
{
    return isAttemptState(m_state);
}

void NativeLoginManager::start()
{
    if (active()) {
        return;
    }
    clearReply();
    clearAttemptData();
    if (!m_networkManager || !m_transport) {
        m_attemptId = newAttemptId();
        finishFailure(ErrorKind::Internal,
                      QStringLiteral("qq.login.network.unavailable"));
        return;
    }
    m_attemptId = newAttemptId();
    setState(Opening);
    sendCreateRequest();
}

void NativeLoginManager::cancel()
{
    if (!active()) {
        return;
    }
    clearReply();
    clearAttemptData();
    setState(Cancelled);
}

void NativeLoginManager::requestFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || reply != m_reply) {
        return;
    }
    const RequestKind kind = m_requestKind;
    const int maximumBytes = m_maximumResponseBytes;
    m_requestTimer->stop();
    m_reply = 0;
    m_requestKind = RequestKind::None;
    m_maximumResponseBytes = 0;
    reply->disconnect(this);

    const QNetworkReply::NetworkError networkError = reply->error();
    const int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    const bool policyOk = replyMatchesPolicy(reply, kind);
    reply->deleteLater();

    if (!policyOk || status != 200
            || networkError != QNetworkReply::NoError
            || body.isEmpty() || body.size() > maximumBytes) {
        finishFailure(networkError == QNetworkReply::TimeoutError
                          ? ErrorKind::Timeout : ErrorKind::Offline,
                      QStringLiteral("qq.login.network.")
                          + requestDiagnostic(m_state));
        return;
    }

    switch (kind) {
    case RequestKind::Create:
        handleCreate(body);
        break;
    case RequestKind::Exchange:
        handleExchange(body);
        break;
    case RequestKind::None:
        finishFailure(ErrorKind::Internal,
                      QStringLiteral("qq.login.reply.unexpected"));
        break;
    }
}

void NativeLoginManager::responseReadyRead()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || reply != m_reply
            || reply->bytesAvailable() <= m_maximumResponseBytes) {
        return;
    }
    const RequestKind kind = m_requestKind;
    clearReply();
    Q_UNUSED(kind);
    finishFailure(ErrorKind::DecodeFailed,
                  QStringLiteral("qq.login.response.size"));
}

void NativeLoginManager::requestTimedOut()
{
    if (m_requestKind == RequestKind::None
            && m_mqttPhase != MqttPhase::None) {
        handleMqttFailure(QStringLiteral("qq.login.mqtt.timeout"));
        return;
    }
    const RequestKind kind = m_requestKind;
    clearReply();
    Q_UNUSED(kind);
    finishTimedOut(QStringLiteral("qq.login.request.timeout"));
}

void NativeLoginManager::attemptTimedOut()
{
    if (active()) {
        clearReply();
        finishTimedOut(QStringLiteral("qq.login.attempt.timeout"));
    }
}

void NativeLoginManager::retryMqtt()
{
    if ((m_state == QrReady || m_state == Scanned)
            && m_mqttPhase == MqttPhase::None && !m_reply) {
        startMqtt(m_serverReference);
    }
}

void NativeLoginManager::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    qInfo().noquote().nospace()
        << "qqmusic_login component=native event=state attempt="
        << m_attemptId << " state=" << stateName(state);
    emit stateChanged(state, m_attemptId);
}

QString NativeLoginManager::stateName(State state) const
{
    switch (state) {
    case Idle: return QStringLiteral("idle");
    case Opening: return QStringLiteral("opening");
    case QrReady: return QStringLiteral("qr");
    case Scanned: return QStringLiteral("scanned");
    case Exchanging: return QStringLiteral("exchanging");
    case CandidateReady: return QStringLiteral("candidate_ready");
    case Cancelled: return QStringLiteral("cancelled");
    case TimedOut: return QStringLiteral("timeout");
    case Failed: return QStringLiteral("failed");
    }
    return QStringLiteral("unknown");
}

void NativeLoginManager::sendCreateRequest()
{
    const QByteArray body = NativeLoginProtocol::createRequestBody();
    beginPost(RequestKind::Create,
              NativeLoginProtocol::serviceUrl(
                  QStringLiteral("CreateQRCode"), body,
                  QDateTime::currentMSecsSinceEpoch()),
              body, QByteArrayLiteral("application/x-www-form-urlencoded"),
              m_limits.createTimeoutMs,
              NativeLoginProtocol::maximumCreateResponseBytes());
}

void NativeLoginManager::startMqtt(const QString &serverReference)
{
    if (!m_transport || m_qrId.isEmpty()) {
        finishFailure(ErrorKind::Internal,
                      QStringLiteral("qq.login.mqtt.challenge.missing"));
        return;
    }
    const QUrl url = NativeLoginProtocol::mqttWebSocketUrl(serverReference);
    if (!url.isValid() || url.isEmpty()) {
        finishFailure(ErrorKind::SchemaChanged,
                      QStringLiteral("qq.login.mqtt.server-reference"));
        return;
    }
    clearMqtt();
    m_serverReference = serverReference;
    m_mqttPhase = MqttPhase::Opening;
    if (!m_transport->open(url, mqttHeaders())) {
        m_mqttPhase = MqttPhase::None;
        handleMqttFailure(QStringLiteral("qq.login.websocket.open"));
        return;
    }
    m_requestTimer->start(m_limits.mqttConnectTimeoutMs);
}

void NativeLoginManager::sendExchangeRequest(const NativeLoginEvent &event)
{
    clearReply();
    clearMqtt();
    setState(Exchanging);
    const QByteArray body = NativeLoginProtocol::exchangeRequestBody(
        event.musicUin, event.qrId, event.token);
    beginPost(RequestKind::Exchange,
              NativeLoginProtocol::serviceUrl(
                  QStringLiteral("Login"), body,
                  QDateTime::currentMSecsSinceEpoch()),
              body, QByteArrayLiteral("application/json"),
              m_limits.exchangeTimeoutMs,
              NativeLoginProtocol::maximumExchangeResponseBytes());
}

void NativeLoginManager::beginPost(RequestKind kind,
                                   const QUrl &url,
                                   const QByteArray &body,
                                   const QByteArray &contentType,
                                   int timeoutMs,
                                   int maximumResponseBytes)
{
    if (!m_networkManager || m_reply || !url.isValid()
            || url.scheme() != QStringLiteral("https")
            || body.isEmpty() || timeoutMs < 1 || maximumResponseBytes < 1) {
        finishFailure(ErrorKind::Internal,
                      QStringLiteral("qq.login.request.invalid"));
        return;
    }
    QNetworkRequest request(url);
    if (kind == RequestKind::Exchange) {
        request.setRawHeader("User-Agent", loginUserAgent());
    } else {
        request.setRawHeader("User-Agent", userAgent());
        request.setRawHeader("Origin", QByteArrayLiteral("https://y.qq.com"));
        request.setRawHeader(
            "Referer",
            QByteArrayLiteral(
                "https://y.qq.com/m/client/qr_code_login/index.html"));
    }
    request.setRawHeader("Content-Type", contentType);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
#elif QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, false);
#endif

    QNetworkReply *reply = m_networkManager->post(request, body);
    if (!reply) {
        finishFailure(ErrorKind::Internal,
                      QStringLiteral("qq.login.request.start"));
        return;
    }
    reply->setReadBufferSize(maximumResponseBytes + 1);
    m_reply = reply;
    m_requestKind = kind;
    m_maximumResponseBytes = maximumResponseBytes;
    connect(reply, &QNetworkReply::finished,
            this, &NativeLoginManager::requestFinished);
    connect(reply, &QNetworkReply::readyRead,
            this, &NativeLoginManager::responseReadyRead);
    m_requestTimer->start(timeoutMs);
}

void NativeLoginManager::handleCreate(const QByteArray &body)
{
    const ServiceResult<NativeQrChallenge> parsed =
        NativeLoginProtocol::parseCreateResponse(body);
    if (!parsed.ok()) {
        finishFailure(parsed.error.kind, parsed.error.diagnosticCode);
        return;
    }
    m_qrId = parsed.value.qrId;
    m_mqttFailures = 0;
    m_mqttRedirects = 0;
    m_serverReference.clear();
    ++m_generation;
    const int timeoutMs = qMin(
        parsed.value.expiresSeconds * 1000,
        m_limits.attemptTimeoutCapMs);
    if (timeoutMs < 1) {
        finishFailure(ErrorKind::Internal,
                      QStringLiteral("qq.login.attempt.limit"));
        return;
    }
    m_attemptTimer->start(timeoutMs);
    setState(QrReady);
    qInfo().noquote().nospace()
        << "qqmusic_login component=native event=qr_ready attempt="
        << m_attemptId
        << " bytes=" << parsed.value.png.size()
        << " width=" << parsed.value.width
        << " height=" << parsed.value.height
        << " generation=" << m_generation;
    emit qrReady(m_attemptId, parsed.value.png,
                 parsed.value.width, parsed.value.height, m_generation);
    startMqtt();
}

void NativeLoginManager::mqttConnected()
{
    if (m_mqttPhase != MqttPhase::Opening || !m_transport) {
        return;
    }
    m_requestTimer->stop();
    const QByteArray packet = NativeMqttProtocol::connectPacket(
        m_qrId.toLatin1(), mqttClientId());
    if (packet.isEmpty()) {
        finishFailure(ErrorKind::Internal,
                      QStringLiteral("qq.login.mqtt.connect.encode"));
        return;
    }
    m_mqttPhase = MqttPhase::WaitingConnAck;
    if (!m_transport->sendBinaryMessage(packet)) {
        handleMqttFailure(QStringLiteral("qq.login.mqtt.connect.send"));
        return;
    }
    if (m_mqttPhase == MqttPhase::WaitingConnAck) {
        m_requestTimer->start(m_limits.mqttAckTimeoutMs);
    }
}

void NativeLoginManager::mqttBinaryMessageReceived(const QByteArray &message)
{
    NativeMqttProtocol::Packet packet;
    QByteArray diagnostic;
    if (!NativeMqttProtocol::parse(message, &packet, &diagnostic)) {
        finishFailure(ErrorKind::SchemaChanged,
                      QStringLiteral("qq.login.mqtt.")
                          + QString::fromLatin1(diagnostic));
        return;
    }
    if (packet.event == NativeMqttProtocol::EventConnAck) {
        if (m_mqttPhase != MqttPhase::WaitingConnAck) {
            finishFailure(ErrorKind::SchemaChanged,
                          QStringLiteral("qq.login.mqtt.connack.order"));
            return;
        }
        m_requestTimer->stop();
        if (packet.reasonCode == 156 || packet.reasonCode == 157) {
            if (packet.serverReference.isEmpty()
                    || m_mqttRedirects >= m_limits.maximumMqttRedirects) {
                finishFailure(ErrorKind::Offline,
                              QStringLiteral("qq.login.mqtt.redirect"));
                return;
            }
            ++m_mqttRedirects;
            qInfo().noquote().nospace()
                << "qqmusic_login component=native event=mqtt_redirect attempt="
                << m_attemptId << " count=" << m_mqttRedirects;
            startMqtt(packet.serverReference);
            return;
        }
        if (packet.reasonCode != 0 || !m_transport) {
            finishFailure(ErrorKind::Offline,
                          QStringLiteral("qq.login.mqtt.connack"));
            return;
        }
        qInfo().noquote().nospace()
            << "qqmusic_login component=native event=mqtt_connected attempt="
            << m_attemptId;
        const QByteArray subscribe = NativeMqttProtocol::subscribePacket(
            m_qrId.toLatin1(), 1);
        if (subscribe.isEmpty()) {
            finishFailure(ErrorKind::Internal,
                          QStringLiteral("qq.login.mqtt.subscribe.encode"));
            return;
        }
        m_mqttPhase = MqttPhase::WaitingSubAck;
        if (!m_transport->sendBinaryMessage(subscribe)) {
            handleMqttFailure(QStringLiteral("qq.login.mqtt.subscribe.send"));
            return;
        }
        if (m_mqttPhase == MqttPhase::WaitingSubAck) {
            m_requestTimer->start(m_limits.mqttAckTimeoutMs);
        }
        return;
    }
    if (packet.event == NativeMqttProtocol::EventSubAck) {
        if (m_mqttPhase != MqttPhase::WaitingSubAck
                || packet.packetId != 1 || packet.reasonCodes.size() != 1
                || packet.reasonCodes.first() != 0) {
            finishFailure(ErrorKind::SchemaChanged,
                          QStringLiteral("qq.login.mqtt.suback"));
            return;
        }
        m_requestTimer->stop();
        m_mqttPhase = MqttPhase::Subscribed;
        m_mqttFailures = 0;
        m_keepAliveTimer->start(m_limits.mqttKeepAliveIntervalMs);
        qInfo().noquote().nospace()
            << "qqmusic_login component=native event=mqtt_subscribed attempt="
            << m_attemptId;
        return;
    }
    if (packet.event == NativeMqttProtocol::EventPublish) {
        const QByteArray expectedTopic =
            QByteArrayLiteral("management.qrcode_login/")
            + m_qrId.toLatin1();
        if (m_mqttPhase != MqttPhase::Subscribed
                || packet.topic != expectedTopic) {
            finishFailure(ErrorKind::SchemaChanged,
                          QStringLiteral("qq.login.mqtt.publish.topic"));
            return;
        }
        handleMqttEvent(packet.eventType, packet.payload);
        return;
    }
    if (packet.event == NativeMqttProtocol::EventPingReq) {
        if (m_mqttPhase != MqttPhase::Subscribed || !m_transport
                || !m_transport->sendBinaryMessage(
                    NativeMqttProtocol::pingResponsePacket())) {
            handleMqttFailure(QStringLiteral("qq.login.mqtt.ping-response"));
        }
        return;
    }
    if (packet.event == NativeMqttProtocol::EventPingResp) {
        return;
    }
    handleMqttFailure(QStringLiteral("qq.login.mqtt.disconnect"));
}

void NativeLoginManager::mqttDisconnected()
{
    if (!m_closingMqtt && m_mqttPhase != MqttPhase::None
            && (m_state == QrReady || m_state == Scanned)) {
        handleMqttFailure(QStringLiteral("qq.login.websocket.disconnected"));
    }
}

void NativeLoginManager::mqttFailed(const QString &diagnosticCode)
{
    if (m_mqttPhase != MqttPhase::None
            && (m_state == QrReady || m_state == Scanned)) {
        handleMqttFailure(diagnosticCode);
    }
}

void NativeLoginManager::mqttKeepAlive()
{
    if (m_mqttPhase != MqttPhase::Subscribed || !m_transport
            || !m_transport->sendBinaryMessage(
                NativeMqttProtocol::pingRequestPacket())) {
        handleMqttFailure(QStringLiteral("qq.login.mqtt.ping-request"));
    }
}

void NativeLoginManager::handleMqttEvent(const QString &eventType,
                                         const QByteArray &payload)
{
    const ServiceResult<NativeLoginEvent> parsed =
        NativeLoginProtocol::parseMqttEvent(eventType, payload, m_qrId);
    if (!parsed.ok()) {
        finishFailure(parsed.error.kind, parsed.error.diagnosticCode);
        return;
    }
    const NativeLoginEvent &event = parsed.value;
    if (event.kind == NativeLoginEvent::Scanned) {
        if (m_state == QrReady) {
            setState(Scanned);
        }
    } else if (event.kind == NativeLoginEvent::Candidate) {
        sendExchangeRequest(event);
    } else if (event.kind == NativeLoginEvent::Cancelled) {
        clearAttemptData();
        setState(Cancelled);
    } else if (event.kind == NativeLoginEvent::TimedOut) {
        finishTimedOut(event.diagnosticCode);
    } else {
        finishFailure(ErrorKind::AuthenticationExpired,
                      event.diagnosticCode.isEmpty()
                          ? QStringLiteral("qq.login.remote.failed")
                          : event.diagnosticCode);
    }
}

void NativeLoginManager::handleExchange(const QByteArray &body)
{
    const ServiceResult<QVector<CookieRecord> > parsed =
        NativeLoginProtocol::parseExchangeResponse(
            body, QDateTime::currentDateTimeUtc());
    if (!parsed.ok()) {
        finishFailure(parsed.error.kind, parsed.error.diagnosticCode);
        return;
    }
    m_attemptTimer->stop();
    m_retryTimer->stop();
    m_qrId.clear();
    setState(CandidateReady);
    qInfo().noquote().nospace()
        << "qqmusic_login component=native event=candidate_ready attempt="
        << m_attemptId << " cookie_count=" << parsed.value.size();
    emit candidateReady(m_attemptId, parsed.value);
}

void NativeLoginManager::handleMqttFailure(const QString &diagnosticCode)
{
    clearMqtt();
    ++m_mqttFailures;
    if (m_mqttFailures > m_limits.maximumMqttFailures) {
        finishFailure(ErrorKind::Offline, diagnosticCode);
        return;
    }
    qInfo().noquote().nospace()
        << "qqmusic_login component=native event=mqtt_retry attempt="
        << m_attemptId << " count=" << m_mqttFailures;
    scheduleMqttRetry();
}

void NativeLoginManager::scheduleMqttRetry()
{
    if (m_state != QrReady && m_state != Scanned) {
        return;
    }
    m_retryTimer->start(qMax(0, m_limits.mqttRetryDelayMs));
}

void NativeLoginManager::finishFailure(ErrorKind kind,
                                       const QString &diagnosticCode)
{
    clearReply();
    clearAttemptData();
    ServiceError error;
    error.kind = kind == ErrorKind::None ? ErrorKind::Internal : kind;
    error.diagnosticCode = diagnosticCode.isEmpty()
        ? QStringLiteral("qq.login.failed") : diagnosticCode;
    setState(Failed);
    qInfo().noquote().nospace()
        << "qqmusic_login component=native event=failed attempt="
        << m_attemptId << " diagnostic=" << error.diagnosticCode;
    emit attemptFailed(m_attemptId, error);
}

void NativeLoginManager::finishTimedOut(const QString &diagnosticCode)
{
    clearReply();
    clearAttemptData();
    ServiceError error;
    error.kind = ErrorKind::Timeout;
    error.diagnosticCode = diagnosticCode;
    setState(TimedOut);
    emit attemptFailed(m_attemptId, error);
}

void NativeLoginManager::clearReply()
{
    m_requestTimer->stop();
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = 0;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    m_requestKind = RequestKind::None;
    m_maximumResponseBytes = 0;
}

void NativeLoginManager::clearMqtt()
{
    if (m_requestKind == RequestKind::None) {
        m_requestTimer->stop();
    }
    m_retryTimer->stop();
    m_keepAliveTimer->stop();
    m_mqttPhase = MqttPhase::None;
    if (m_transport) {
        m_closingMqtt = true;
        m_transport->close();
        m_closingMqtt = false;
    }
}

void NativeLoginManager::clearAttemptData()
{
    m_attemptTimer->stop();
    clearMqtt();
    m_qrId.clear();
    m_serverReference.clear();
    m_generation = 0;
    m_mqttFailures = 0;
    m_mqttRedirects = 0;
}

bool NativeLoginManager::replyMatchesPolicy(
    QNetworkReply *reply,
    RequestKind kind) const
{
    if (!reply
            || reply->attribute(
                QNetworkRequest::RedirectionTargetAttribute).isValid()) {
        return false;
    }
    const QUrl url = reply->url();
    if (url.scheme() != QStringLiteral("https") || !url.userInfo().isEmpty()
            || url.port(-1) != -1 || !url.fragment().isEmpty()
            || url != reply->request().url()) {
        return false;
    }
    if (kind == RequestKind::Exchange) {
        return url.host() == QStringLiteral("u.y.qq.com")
            && url.path() == QStringLiteral("/cgi-bin/musicu.fcg")
            && !url.hasQuery();
    }
    if (kind != RequestKind::Create
            || url.host() != QStringLiteral("u6.y.qq.com")
            || url.path() != QStringLiteral("/cgi-bin/musics.fcg")) {
        return false;
    }
    const QList<QPair<QString, QString> > items = QUrlQuery(url).queryItems();
    if (items.size() != 3) {
        return false;
    }
    QString method;
    QString signature;
    QString timestamp;
    for (int index = 0; index < items.size(); ++index) {
        const QPair<QString, QString> &item = items.at(index);
        if (item.first == QStringLiteral("_webcgikey") && method.isEmpty()) {
            method = item.second;
        } else if (item.first == QStringLiteral("sign")
                   && signature.isEmpty()) {
            signature = item.second;
        } else if (item.first == QStringLiteral("_") && timestamp.isEmpty()) {
            timestamp = item.second;
        } else {
            return false;
        }
    }
    static const QRegularExpression signaturePattern(
        QStringLiteral("^zzc[a-z0-9]{32,64}$"));
    static const QRegularExpression timestampPattern(
        QStringLiteral("^[0-9]{10,16}$"));
    return method == QStringLiteral("CreateQRCode")
        && signaturePattern.match(signature).hasMatch()
        && timestampPattern.match(timestamp).hasMatch();
}

bool NativeLoginManager::isAttemptState(State state) const
{
    return state == Opening || state == QrReady || state == Scanned
        || state == Exchanging;
}
