#include "nativeloginprotocol.h"

#include "core/qqidentifier.h"

#include <QCryptographicHash>
#include <QImage>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QtEndian>

namespace {

const int MaximumQrPngBytes = 512 * 1024;
const int MaximumQrDimension = 1024;
const int MinimumQrDimension = 64;

ServiceError protocolError(ErrorKind kind, const QString &diagnosticCode)
{
    ServiceError error;
    error.kind = kind;
    error.diagnosticCode = diagnosticCode;
    return error;
}

bool isBoundedToken(const QString &value, int maximum = 256)
{
    static const QRegularExpression safe(
        QStringLiteral("^[A-Za-z0-9_-]+$"));
    return !value.isEmpty() && value.size() <= maximum
        && safe.match(value).hasMatch();
}

bool isBoundedServerReference(const QString &value)
{
    static const QRegularExpression safe(
        QStringLiteral("^[A-Za-z0-9.:-]+$"));
    return !value.isEmpty() && value.size() <= 128
        && safe.match(value).hasMatch();
}

bool hasControlBytes(const QByteArray &value)
{
    for (int index = 0; index < value.size(); ++index) {
        const uchar byte = static_cast<uchar>(value.at(index));
        if (byte < 0x20 || byte == 0x7f) {
            return true;
        }
    }
    return false;
}

bool latin1RoundTrips(const QString &value)
{
    return QString::fromLatin1(value.toLatin1()) == value;
}

bool jsonIntegerText(const QJsonValue &value, QString *text)
{
    if (value.isString()) {
        *text = value.toString();
        return true;
    }
    if (!value.isDouble()) {
        return false;
    }
    const double numeric = value.toDouble(-1.0);
    // QJson stores numbers as doubles, so reject values that cannot be
    // represented as exact positive integers before converting them.
    if (numeric < 1.0 || numeric > 9007199254740991.0) {
        return false;
    }
    const qulonglong integer = static_cast<qulonglong>(numeric);
    if (static_cast<double>(integer) != numeric) {
        return false;
    }
    *text = QString::number(integer);
    return true;
}

bool hasBoundedPngHeader(const QByteArray &png, int *width, int *height)
{
    static const QByteArray signature =
        QByteArray::fromHex("89504e470d0a1a0a");
    if (png.size() < 24 || !png.startsWith(signature)
            || qFromBigEndian<quint32>(
                reinterpret_cast<const uchar *>(png.constData() + 8)) != 13
            || png.mid(12, 4) != QByteArrayLiteral("IHDR")) {
        return false;
    }
    const quint32 parsedWidth = qFromBigEndian<quint32>(
        reinterpret_cast<const uchar *>(png.constData() + 16));
    const quint32 parsedHeight = qFromBigEndian<quint32>(
        reinterpret_cast<const uchar *>(png.constData() + 20));
    if (parsedWidth < quint32(MinimumQrDimension)
            || parsedHeight < quint32(MinimumQrDimension)
            || parsedWidth > quint32(MaximumQrDimension)
            || parsedHeight > quint32(MaximumQrDimension)) {
        return false;
    }
    *width = int(parsedWidth);
    *height = int(parsedHeight);
    return true;
}

bool parseRoot(const QByteArray &body,
               int maximumBytes,
               QJsonObject *root,
               ServiceError *error,
               const QString &diagnosticPrefix)
{
    if (body.isEmpty() || body.size() > maximumBytes) {
        *error = protocolError(ErrorKind::DecodeFailed,
                               diagnosticPrefix + QStringLiteral(".size"));
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error = protocolError(ErrorKind::DecodeFailed,
                               diagnosticPrefix + QStringLiteral(".json"));
        return false;
    }
    *root = document.object();
    return true;
}

bool successfulRequestData(const QJsonObject &root,
                           QJsonObject *data,
                           ServiceError *error,
                           const QString &diagnosticPrefix)
{
    if (!root.value(QStringLiteral("code")).isDouble()
            || root.value(QStringLiteral("code")).toInt(-1) != 0) {
        *error = protocolError(ErrorKind::SchemaChanged,
                               diagnosticPrefix
                                   + QStringLiteral(".root.code"));
        return false;
    }
    if (!root.value(QStringLiteral("req_0")).isObject()) {
        *error = protocolError(ErrorKind::SchemaChanged,
                               diagnosticPrefix
                                   + QStringLiteral(".root.request"));
        return false;
    }
    const QJsonObject request = root.value(QStringLiteral("req_0")).toObject();
    if (!request.value(QStringLiteral("code")).isDouble()
            || request.value(QStringLiteral("code")).toInt(-1) != 0) {
        *error = protocolError(ErrorKind::SchemaChanged,
                               diagnosticPrefix
                                   + QStringLiteral(".request.code"));
        return false;
    }
    if (!request.value(QStringLiteral("data")).isObject()) {
        *error = protocolError(ErrorKind::SchemaChanged,
                               diagnosticPrefix
                                   + QStringLiteral(".request.data"));
        return false;
    }
    *data = request.value(QStringLiteral("data")).toObject();
    return true;
}

QString cookieValue(const QJsonObject &cookies, const QString &name)
{
    const QJsonValue value = cookies.value(name);
    if (value.isString()) {
        return value.toString();
    }
    if (!value.isObject()) {
        return QString();
    }
    return value.toObject().value(QStringLiteral("value")).toString();
}

CookieRecord cookie(const QByteArray &name,
                    const QByteArray &value,
                    const QDateTime &expiresAt)
{
    CookieRecord record;
    record.name = name;
    record.value = value;
    record.domain = QStringLiteral(".qq.com");
    record.path = QStringLiteral("/");
    record.secure = true;
    record.expiresAt = expiresAt;
    return record;
}

}

int NativeLoginProtocol::maximumCreateResponseBytes()
{
    return 1024 * 1024;
}

int NativeLoginProtocol::maximumMqttPayloadBytes()
{
    return 96 * 1024;
}

int NativeLoginProtocol::maximumExchangeResponseBytes()
{
    return 256 * 1024;
}

QByteArray NativeLoginProtocol::signBody(const QByteArray &body)
{
    static const int firstIndices[] = { 23, 14, 6, 36, 16, 40, 7, 19 };
    static const int secondIndices[] = { 16, 1, 32, 12, 19, 27, 8, 5 };
    static const uchar scramble[] = {
        89, 39, 179, 150, 218, 82, 58, 252, 177, 52,
        186, 123, 120, 64, 242, 133, 143, 161, 121, 179
    };

    const QByteArray hex = QCryptographicHash::hash(
        body, QCryptographicHash::Sha1).toHex().toUpper();
    QByteArray first;
    for (uint index = 0;
         index < sizeof(firstIndices) / sizeof(firstIndices[0]); ++index) {
        const int offset = firstIndices[index];
        if (offset >= 0 && offset < hex.size()) {
            first.append(hex.at(offset));
        }
    }
    QByteArray second;
    for (uint index = 0;
         index < sizeof(secondIndices) / sizeof(secondIndices[0]); ++index) {
        const int offset = secondIndices[index];
        if (offset >= 0 && offset < hex.size()) {
            second.append(hex.at(offset));
        }
    }
    QByteArray mixed(20, '\0');
    for (int index = 0; index < mixed.size(); ++index) {
        bool ok = false;
        const int byte = hex.mid(index * 2, 2).toInt(&ok, 16);
        if (!ok) {
            return QByteArray();
        }
        mixed[index] = char(scramble[index] ^ byte);
    }
    QByteArray encoded = mixed.toBase64();
    encoded.replace(QByteArrayLiteral("/"), QByteArray());
    encoded.replace(QByteArrayLiteral("+"), QByteArray());
    encoded.replace(QByteArrayLiteral("="), QByteArray());
    return (QByteArrayLiteral("zzc") + first + encoded + second).toLower();
}

QByteArray NativeLoginProtocol::createRequestBody()
{
    QJsonObject parameters;
    parameters.insert(QStringLiteral("tmeAppID"), QStringLiteral("qqmusic"));
    parameters.insert(QStringLiteral("ct"), 11);
    parameters.insert(QStringLiteral("cv"), 20030508);
    return requestBody(QStringLiteral("CreateQRCode"), parameters);
}

QByteArray NativeLoginProtocol::exchangeRequestBody(
    const QString &musicUin,
    const QString &qrId,
    const QByteArray &token)
{
    QString numericUin = musicUin;
    while (numericUin.startsWith(QLatin1Char('o'))) {
        numericUin.remove(0, 1);
    }
    bool uinOk = false;
    const qulonglong uin = numericUin.toULongLong(&uinOk);
    if (!uinOk || uin == 0 || !QqIdentifier::isNumericPlaylistId(numericUin)
            || !isBoundedToken(qrId)
            || token.isEmpty()
            || token.size() > CookiePolicy::maximumCookieBytes()
            || hasControlBytes(token)) {
        return QByteArray();
    }
    QJsonObject parameters;
    parameters.insert(QStringLiteral("musicid"), double(uin));
    parameters.insert(QStringLiteral("qrCodeID"), qrId);
    parameters.insert(QStringLiteral("token"), QString::fromLatin1(token));
    QJsonObject communication;
    communication.insert(QStringLiteral("ct"), 11);
    communication.insert(QStringLiteral("cv"), 14090008);
    communication.insert(QStringLiteral("v"), 14090008);
    communication.insert(QStringLiteral("chid"), QStringLiteral("10003505"));
    communication.insert(QStringLiteral("tmeAppID"), QStringLiteral("qqmusic"));
    communication.insert(QStringLiteral("tmeLoginType"), 6);
    communication.insert(QStringLiteral("format"), QStringLiteral("json"));
    communication.insert(QStringLiteral("inCharset"), QStringLiteral("utf-8"));
    communication.insert(QStringLiteral("outCharset"), QStringLiteral("utf-8"));
    communication.insert(
        QStringLiteral("QIMEI36"),
        QStringLiteral("6c9d3cd110abca9b16311cee10001e717614"));
    return requestBody(QStringLiteral("Login"), parameters, communication);
}

QUrl NativeLoginProtocol::serviceUrl(const QString &method,
                                     const QByteArray &body,
                                     qint64 timestampMs)
{
    if (method != QStringLiteral("CreateQRCode")
            && method != QStringLiteral("Login")) {
        return QUrl();
    }
    if (body.isEmpty() || timestampMs <= 0) {
        return QUrl();
    }
    if (method == QStringLiteral("Login")) {
        return QUrl(QStringLiteral("https://u.y.qq.com/cgi-bin/musicu.fcg"));
    }
    const QByteArray signature = signBody(body);
    if (signature.isEmpty()) {
        return QUrl();
    }
    QUrl url(QStringLiteral("https://u6.y.qq.com/cgi-bin/musics.fcg"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("_webcgikey"), method);
    query.addQueryItem(QStringLiteral("sign"),
                       QString::fromLatin1(signature));
    query.addQueryItem(QStringLiteral("_"), QString::number(timestampMs));
    url.setQuery(query);
    return url;
}

QUrl NativeLoginProtocol::mqttWebSocketUrl(const QString &serverReference)
{
    if (!serverReference.isEmpty()
            && !isBoundedServerReference(serverReference)) {
        return QUrl();
    }
    QUrl url(QStringLiteral("wss://mu.y.qq.com/ws/handshake"));
    if (!serverReference.isEmpty()) {
        url.setPath(url.path() + QLatin1Char('/') + serverReference);
    }
    return url;
}

ServiceResult<NativeQrChallenge> NativeLoginProtocol::parseCreateResponse(
    const QByteArray &body)
{
    ServiceResult<NativeQrChallenge> result;
    QJsonObject root;
    if (!parseRoot(body, maximumCreateResponseBytes(), &root, &result.error,
                   QStringLiteral("qq.login.create"))) {
        return result;
    }
    QJsonObject data;
    if (!successfulRequestData(root, &data, &result.error,
                               QStringLiteral("qq.login.create"))) {
        return result;
    }
    const QJsonValue qrIdValue = data.value(QStringLiteral("qrcodeID"));
    const QJsonValue qrValue = data.value(QStringLiteral("qrcode"));
    const QJsonValue expiresValue = data.value(QStringLiteral("expiresIn"));
    if (!qrIdValue.isString() || !qrValue.isString()
            || !expiresValue.isDouble()) {
        result.error = protocolError(ErrorKind::SchemaChanged,
                                     QStringLiteral("qq.login.create.fields"));
        return result;
    }
    const QString qrId = qrIdValue.toString();
    const QString dataUri = qrValue.toString();
    const int expiresSeconds = expiresValue.toInt(-1);
    const QString prefix = QStringLiteral("data:image/png;base64,");
    if (!isBoundedToken(qrId)
            || !dataUri.startsWith(prefix)
            || expiresSeconds < 30 || expiresSeconds > 15 * 60) {
        result.error = protocolError(ErrorKind::SchemaChanged,
                                     QStringLiteral("qq.login.create.bounds"));
        return result;
    }
    const QByteArray encoded = dataUri.mid(prefix.size()).toLatin1();
    if (encoded.isEmpty() || encoded.size() > (MaximumQrPngBytes * 4 / 3 + 8)) {
        result.error = protocolError(ErrorKind::DecodeFailed,
                                     QStringLiteral("qq.login.create.png.size"));
        return result;
    }
    const QByteArray png = QByteArray::fromBase64(encoded);
    if (png.isEmpty() || png.size() > MaximumQrPngBytes
            || png.toBase64() != encoded) {
        result.error = protocolError(ErrorKind::DecodeFailed,
                                     QStringLiteral("qq.login.create.png.base64"));
        return result;
    }
    int pngWidth = 0;
    int pngHeight = 0;
    if (!hasBoundedPngHeader(png, &pngWidth, &pngHeight)) {
        result.error = protocolError(ErrorKind::DecodeFailed,
                                     QStringLiteral("qq.login.create.png.header"));
        return result;
    }
    const QImage image = QImage::fromData(png, "PNG");
    if (image.isNull() || image.width() != pngWidth
            || image.height() != pngHeight) {
        result.error = protocolError(ErrorKind::DecodeFailed,
                                     QStringLiteral("qq.login.create.png.image"));
        return result;
    }
    result.value.qrId = qrId;
    result.value.png = png;
    result.value.width = image.width();
    result.value.height = image.height();
    result.value.expiresSeconds = expiresSeconds;
    return result;
}

ServiceResult<NativeLoginEvent> NativeLoginProtocol::parseMqttEvent(
    const QString &eventType,
    const QByteArray &payload,
    const QString &expectedQrId)
{
    ServiceResult<NativeLoginEvent> result;
    if (!isBoundedToken(expectedQrId)) {
        result.error = protocolError(ErrorKind::Internal,
                                     QStringLiteral("qq.login.mqtt.qr-id"));
        return result;
    }
    if (eventType == QStringLiteral("scanned")) {
        result.value.kind = NativeLoginEvent::Scanned;
        return result;
    }
    if (eventType == QStringLiteral("canceled")) {
        result.value.kind = NativeLoginEvent::Cancelled;
        return result;
    }
    if (eventType == QStringLiteral("timeout")) {
        result.value.kind = NativeLoginEvent::TimedOut;
        result.value.diagnosticCode = QStringLiteral("qq.login.remote.timeout");
        return result;
    }
    if (eventType == QStringLiteral("loginFailed")) {
        result.value.kind = NativeLoginEvent::Failed;
        result.value.diagnosticCode = QStringLiteral("qq.login.remote.failed");
        return result;
    }
    if (eventType != QStringLiteral("cookies")) {
        result.error = protocolError(ErrorKind::SchemaChanged,
                                     QStringLiteral("qq.login.mqtt.event"));
        return result;
    }

    QJsonObject root;
    if (!parseRoot(payload, maximumMqttPayloadBytes(), &root, &result.error,
                   QStringLiteral("qq.login.mqtt.payload"))) {
        return result;
    }
    if (!root.value(QStringLiteral("cookies")).isObject()) {
        result.error = protocolError(
            ErrorKind::SchemaChanged,
            QStringLiteral("qq.login.mqtt.candidate.cookies"));
        return result;
    }
    const QJsonObject cookies =
        root.value(QStringLiteral("cookies")).toObject();
    result.value.musicUin = cookieValue(
        cookies, QStringLiteral("qqmusic_uin"));
    const QString payloadQrId = cookieValue(
        cookies, QStringLiteral("qrcode_id"));
    result.value.qrId = expectedQrId;
    const QString tokenText = cookieValue(
        cookies, QStringLiteral("qqmusic_key"));
    result.value.token = tokenText.toLatin1();
    QString numericUin = result.value.musicUin;
    while (numericUin.startsWith(QLatin1Char('o'))) {
        numericUin.remove(0, 1);
    }
    if (!QqIdentifier::isNumericPlaylistId(numericUin)
            || (!payloadQrId.isEmpty() && payloadQrId != expectedQrId)
            || !latin1RoundTrips(tokenText)
            || result.value.token.isEmpty()
            || result.value.token.size() > CookiePolicy::maximumCookieBytes()
            || hasControlBytes(result.value.token)) {
        result.error = protocolError(
            ErrorKind::SchemaChanged,
            QStringLiteral("qq.login.mqtt.candidate.invalid"));
        result.value = NativeLoginEvent();
        return result;
    }
    result.value.kind = NativeLoginEvent::Candidate;
    return result;
}

ServiceResult<QVector<CookieRecord> >
NativeLoginProtocol::parseExchangeResponse(
    const QByteArray &body,
    const QDateTime &nowUtc)
{
    ServiceResult<QVector<CookieRecord> > result;
    QJsonObject root;
    if (!parseRoot(body, maximumExchangeResponseBytes(), &root, &result.error,
                   QStringLiteral("qq.login.exchange"))) {
        return result;
    }
    QJsonObject data;
    if (!successfulRequestData(root, &data, &result.error,
                               QStringLiteral("qq.login.exchange"))) {
        return result;
    }
    QString uin;
    if (!jsonIntegerText(data.value(QStringLiteral("musicid")), &uin)
            || !data.value(QStringLiteral("musickey")).isString()) {
        result.error = protocolError(ErrorKind::SchemaChanged,
                                     QStringLiteral("qq.login.exchange.fields"));
        return result;
    }
    while (uin.startsWith(QLatin1Char('o'))) {
        uin.remove(0, 1);
    }
    const QString keyText =
        data.value(QStringLiteral("musickey")).toString();
    const QByteArray key = keyText.toLatin1();
    const QDateTime now = nowUtc.toUTC();
    if (!now.isValid() || !QqIdentifier::isNumericPlaylistId(uin)
            || !latin1RoundTrips(keyText) || key.isEmpty()
            || key.size() > CookiePolicy::maximumCookieBytes()
            || hasControlBytes(key)) {
        result.error = protocolError(ErrorKind::SchemaChanged,
                                     QStringLiteral("qq.login.exchange.invalid"));
        return result;
    }
    const QDateTime expiresAt = now.addDays(30);
    const QByteArray identity = QByteArrayLiteral("o") + uin.toLatin1();
    QVector<CookieRecord> records;
    records << cookie(QByteArrayLiteral("uin"), identity, expiresAt)
            << cookie(QByteArrayLiteral("wxuin"), identity, expiresAt)
            << cookie(QByteArrayLiteral("qm_keyst"), key, expiresAt)
            << cookie(QByteArrayLiteral("qqmusic_key"), key, expiresAt);
    return CookiePolicy::validateAll(records, now);
}

QByteArray NativeLoginProtocol::requestBody(
    const QString &method,
    const QJsonObject &parameters,
    const QJsonObject &communicationFields)
{
    QJsonObject communication;
    communication.insert(QStringLiteral("g_tk"), 0);
    communication.insert(QStringLiteral("uin"), 0);
    communication.insert(QStringLiteral("format"), QStringLiteral("json"));
    communication.insert(QStringLiteral("inCharset"),
                         QStringLiteral("utf-8"));
    communication.insert(QStringLiteral("outCharset"),
                         QStringLiteral("utf-8"));
    communication.insert(QStringLiteral("notice"), 0);
    communication.insert(QStringLiteral("platform"), QStringLiteral("h5"));
    communication.insert(QStringLiteral("needNewCode"), 1);
    communication.insert(QStringLiteral("ct"), 23);
    communication.insert(QStringLiteral("cv"), 0);
    for (QJsonObject::const_iterator item = communicationFields.constBegin();
         item != communicationFields.constEnd(); ++item) {
        communication.insert(item.key(), item.value());
    }

    QJsonObject request;
    request.insert(QStringLiteral("module"),
                   QStringLiteral("music.login.LoginServer"));
    request.insert(QStringLiteral("method"), method);
    request.insert(QStringLiteral("param"), parameters);

    QJsonObject root;
    root.insert(QStringLiteral("comm"), communication);
    root.insert(QStringLiteral("req_0"), request);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}
