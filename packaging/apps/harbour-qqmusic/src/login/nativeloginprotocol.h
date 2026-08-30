#ifndef NATIVELOGINPROTOCOL_H
#define NATIVELOGINPROTOCOL_H

#include "core/types.h"
#include "session/cookiepolicy.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QUrl>
#include <QVector>

struct NativeQrChallenge
{
    QString qrId;
    QByteArray png;
    int width = 0;
    int height = 0;
    int expiresSeconds = 0;
};

struct NativeLoginEvent
{
    enum Kind {
        Scanned,
        Candidate,
        Cancelled,
        TimedOut,
        Failed
    };

    Kind kind = Failed;
    quint64 messageId = 0;
    QString musicUin;
    QString qrId;
    QByteArray token;
    QString diagnosticCode;
};

class NativeLoginProtocol
{
public:
    static int maximumCreateResponseBytes();
    static int maximumMqttPayloadBytes();
    static int maximumExchangeResponseBytes();

    static QByteArray signBody(const QByteArray &body);
    static QByteArray createRequestBody();
    static QByteArray exchangeRequestBody(const QString &musicUin,
                                          const QString &qrId,
                                          const QByteArray &token);
    static QUrl serviceUrl(const QString &method,
                           const QByteArray &body,
                           qint64 timestampMs);
    static QUrl mqttWebSocketUrl(const QString &serverReference = QString());

    static ServiceResult<NativeQrChallenge> parseCreateResponse(
        const QByteArray &body);
    static ServiceResult<NativeLoginEvent> parseMqttEvent(
        const QString &eventType,
        const QByteArray &payload,
        const QString &expectedQrId);
    static ServiceResult<QVector<CookieRecord> > parseExchangeResponse(
        const QByteArray &body,
        const QDateTime &nowUtc);

private:
    static QByteArray requestBody(const QString &method,
                                  const QJsonObject &parameters,
                                  const QJsonObject &communicationFields =
                                      QJsonObject());
};

#endif
