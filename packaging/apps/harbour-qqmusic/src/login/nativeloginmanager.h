#ifndef NATIVELOGINMANAGER_H
#define NATIVELOGINMANAGER_H

#include "nativeloginprotocol.h"
#include "nativelogintransport.h"

#include <QObject>
#include <QPointer>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

struct NativeLoginLimits
{
    int createTimeoutMs = 20000;
    int exchangeTimeoutMs = 20000;
    int mqttConnectTimeoutMs = 15000;
    int mqttAckTimeoutMs = 10000;
    int mqttRetryDelayMs = 500;
    int mqttKeepAliveIntervalMs = 30000;
    int maximumMqttFailures = 3;
    int maximumMqttRedirects = 2;
    int attemptTimeoutCapMs = 15 * 60 * 1000;
};

class NativeLoginManager : public QObject
{
    Q_OBJECT

public:
    enum State {
        Idle,
        Opening,
        QrReady,
        Scanned,
        Exchanging,
        CandidateReady,
        Cancelled,
        TimedOut,
        Failed
    };
    Q_ENUMS(State)

    explicit NativeLoginManager(QObject *parent = 0);
    NativeLoginManager(QNetworkAccessManager *networkManager,
                       const NativeLoginLimits &limits,
                       QObject *parent = 0);
    NativeLoginManager(QNetworkAccessManager *networkManager,
                       NativeLoginTransport *transport,
                       const NativeLoginLimits &limits,
                       QObject *parent = 0);
    ~NativeLoginManager() override;

    State state() const;
    QString attemptId() const;
    bool active() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void cancel();

signals:
    void stateChanged(NativeLoginManager::State state,
                      const QString &attemptId);
    void qrReady(const QString &attemptId,
                 const QByteArray &png,
                 int width,
                 int height,
                 int generation);
    void candidateReady(const QString &attemptId,
                        const QVector<CookieRecord> &records);
    void attemptFailed(const QString &attemptId,
                       const ServiceError &error);

private slots:
    void requestFinished();
    void responseReadyRead();
    void requestTimedOut();
    void attemptTimedOut();
    void retryMqtt();
    void mqttConnected();
    void mqttBinaryMessageReceived(const QByteArray &message);
    void mqttDisconnected();
    void mqttFailed(const QString &diagnosticCode);
    void mqttKeepAlive();

private:
    enum class RequestKind {
        None,
        Create,
        Exchange
    };

    enum class MqttPhase {
        None,
        Opening,
        WaitingConnAck,
        WaitingSubAck,
        Subscribed
    };

    void setState(State state);
    QString stateName(State state) const;
    void sendCreateRequest();
    void startMqtt(const QString &serverReference = QString());
    void sendExchangeRequest(const NativeLoginEvent &event);
    void beginPost(RequestKind kind,
                   const QUrl &url,
                   const QByteArray &body,
                   const QByteArray &contentType,
                   int timeoutMs,
                   int maximumResponseBytes);
    void handleCreate(const QByteArray &body);
    void handleMqttEvent(const QString &eventType,
                         const QByteArray &payload);
    void handleExchange(const QByteArray &body);
    void handleMqttFailure(const QString &diagnosticCode);
    void scheduleMqttRetry();
    void finishFailure(ErrorKind kind, const QString &diagnosticCode);
    void finishTimedOut(const QString &diagnosticCode);
    void clearReply();
    void clearMqtt();
    void clearAttemptData();
    bool replyMatchesPolicy(QNetworkReply *reply, RequestKind kind) const;
    bool isAttemptState(State state) const;

    QPointer<QNetworkAccessManager> m_networkManager;
    QPointer<NativeLoginTransport> m_transport;
    QPointer<QNetworkReply> m_reply;
    NativeLoginLimits m_limits;
    QTimer *m_requestTimer;
    QTimer *m_attemptTimer;
    QTimer *m_retryTimer;
    QTimer *m_keepAliveTimer;
    State m_state;
    RequestKind m_requestKind;
    MqttPhase m_mqttPhase;
    int m_maximumResponseBytes;
    QString m_attemptId;
    QString m_qrId;
    QString m_serverReference;
    int m_generation;
    int m_mqttFailures;
    int m_mqttRedirects;
    bool m_closingMqtt;
};

#endif
