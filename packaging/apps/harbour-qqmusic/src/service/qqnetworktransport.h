#ifndef QQNETWORKTRANSPORT_H
#define QQNETWORKTRANSPORT_H

#include "core/types.h"
#include "qqrequestpolicy.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkDiskCache;
class QNetworkReply;

struct QqTransportLimits
{
    int timeoutMs = 20000;
    qint64 maxResponseBytes = 4 * 1024 * 1024;
};

class QqNetworkTransport : public QObject
{
    Q_OBJECT

public:
    explicit QqNetworkTransport(QObject *parent = 0);
    explicit QqNetworkTransport(QNetworkAccessManager *manager,
                                QObject *parent = 0);
    QqNetworkTransport(QNetworkAccessManager *manager,
                       const QqTransportLimits &limits,
                       QObject *parent = 0);
    ~QqNetworkTransport();

    bool get(quint64 operationId,
             const QUrl &url,
             QqRequestKind kind,
             bool containsCookies = false,
             bool forceNetwork = false);
    bool post(quint64 operationId,
              const QUrl &url,
              QqRequestKind kind,
              const QByteArray &body,
              bool containsCookies = false);
    bool cancel(quint64 operationId);

    QNetworkAccessManager *networkAccessManager() const;
    QNetworkDiskCache *configureDiskCache(const QString &directory,
                                          qint64 maximumBytes = 32 * 1024 * 1024);
    qint64 diskCacheSize() const;
    void setDiskCacheMaximumBytes(qint64 maximumBytes);
    bool clearDiskCache();

signals:
    void completed(quint64 operationId, const QByteArray &body);
    void failed(const ServiceError &error);

private:
    struct Operation;

    bool start(quint64 operationId,
               const QUrl &url,
               QqRequestKind kind,
               bool isGet,
               const QByteArray &body,
               bool containsCookies,
               bool forceNetwork);
    void issue(quint64 operationId, quint64 generation);
    void readAvailable(quint64 operationId,
                       quint64 generation,
                       QNetworkReply *reply);
    bool checkResponseSize(quint64 operationId,
                           quint64 generation,
                           QNetworkReply *reply);
    void finishReply(quint64 operationId,
                     quint64 generation,
                     QNetworkReply *reply);
    void finishSuccess(quint64 operationId,
                       quint64 generation,
                       QNetworkReply *reply);
    void finishError(quint64 operationId,
                     quint64 generation,
                     QNetworkReply *reply,
                     ErrorKind kind,
                     int status,
                     int retryAfterMs,
                     bool abortReply);
    Operation *activeOperation(quint64 operationId,
                               quint64 generation) const;
    Operation *activeReplyOperation(quint64 operationId,
                                    quint64 generation,
                                    QNetworkReply *reply) const;

    QPointer<QNetworkAccessManager> m_manager;
    QqTransportLimits m_limits;
    QHash<quint64, Operation *> m_operations;
    quint64 m_nextGeneration;
};

#endif
