#include "qqnetworktransport.h"

#include <QAbstractNetworkCache>
#include <QDateTime>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkCacheMetaData>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>
#include <QtGlobal>

namespace {

const qint64 PublicCacheLimit = 32 * 1024 * 1024;
const qint64 ReplyReadChunkSize = 64 * 1024;

class QqPublicNetworkDiskCache : public QNetworkDiskCache
{
public:
    explicit QqPublicNetworkDiskCache(QObject *parent = 0)
        : QNetworkDiskCache(parent)
    {
    }

    QIODevice *prepare(const QNetworkCacheMetaData &metaData) override
    {
        if (!QqRequestPolicy::allowsResponseCache(metaData.rawHeaders())) {
            return 0;
        }
        return QNetworkDiskCache::prepare(metaData);
    }
};

QString errorKindName(ErrorKind kind)
{
    switch (kind) {
    case ErrorKind::Offline:
        return QStringLiteral("Offline");
    case ErrorKind::RateLimited:
        return QStringLiteral("RateLimited");
    case ErrorKind::Cancelled:
        return QStringLiteral("Cancelled");
    case ErrorKind::Timeout:
        return QStringLiteral("Timeout");
    case ErrorKind::AuthenticationExpired:
        return QStringLiteral("AuthenticationExpired");
    case ErrorKind::Internal:
        return QStringLiteral("Internal");
    default:
        return QStringLiteral("Internal");
    }
}

bool isOfflineError(QNetworkReply::NetworkError error)
{
    return error == QNetworkReply::ConnectionRefusedError
        || error == QNetworkReply::RemoteHostClosedError
        || error == QNetworkReply::HostNotFoundError
        || error == QNetworkReply::TimeoutError
        || error == QNetworkReply::TemporaryNetworkFailureError
        || error == QNetworkReply::NetworkSessionFailedError
        || error == QNetworkReply::ProxyConnectionRefusedError
        || error == QNetworkReply::ProxyConnectionClosedError
        || error == QNetworkReply::ProxyNotFoundError
        || error == QNetworkReply::ProxyTimeoutError;
}

bool isFollowedRedirectStatus(int status)
{
    return status == 301
        || status == 302
        || status == 303
        || status == 307
        || status == 308;
}

QDateTime parseHttpDate(const QByteArray &value)
{
    static const QString format =
        QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'");
    const QString text = QString::fromLatin1(value.trimmed());
    QDateTime result = QLocale::c().toDateTime(text, format);
    result.setTimeSpec(Qt::UTC);
    if (!result.isValid() || QLocale::c().toString(result, format) != text) {
        return QDateTime();
    }
    return result;
}

int retryAfterMs(const QByteArray &header, const QByteArray &dateHeader)
{
    const QByteArray value = header.trimmed();
    bool decimal = !value.isEmpty();
    qint64 seconds = 0;
    for (int index = 0; index < value.size(); ++index) {
        const char character = value.at(index);
        if (character < '0' || character > '9') {
            decimal = false;
            break;
        }
        if (seconds < 60) {
            seconds = qMin<qint64>(60,
                                   seconds * 10 + character - '0');
        }
    }
    if (!decimal) {
        const QDateTime retryAt = parseHttpDate(value);
        QDateTime reference = parseHttpDate(dateHeader);
        if (!retryAt.isValid()) {
            return 1000;
        }
        if (!reference.isValid()) {
            reference = QDateTime::currentDateTimeUtc();
        }
        const qint64 delayMs = reference.msecsTo(retryAt);
        seconds = delayMs > 0 ? (delayMs + 999) / 1000 : 1;
    }
    seconds = qBound<qint64>(1, seconds, 60);
    return int(seconds * 1000);
}

}

struct QqNetworkTransport::Operation
{
    quint64 id = 0;
    quint64 generation = 0;
    QqRequestKind kind = QqRequestKind::PublicMetadata;
    bool isGet = true;
    bool allowsCookieJar = false;
    bool cacheSensitive = false;
    QByteArray requestBody;
    QByteArray responseBody;
    QUrl currentUrl;
    int redirectCount = 0;
    QPointer<QNetworkReply> reply;
    QTimer *timer = 0;
};

QqNetworkTransport::QqNetworkTransport(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_nextGeneration(0)
{
}

QqNetworkTransport::QqNetworkTransport(QNetworkAccessManager *manager,
                                       QObject *parent)
    : QObject(parent)
    , m_manager(manager ? manager : new QNetworkAccessManager(this))
    , m_nextGeneration(0)
{
}

QqNetworkTransport::QqNetworkTransport(QNetworkAccessManager *manager,
                                       const QqTransportLimits &limits,
                                       QObject *parent)
    : QObject(parent)
    , m_manager(manager ? manager : new QNetworkAccessManager(this))
    , m_limits(limits)
    , m_nextGeneration(0)
{
    if (m_limits.timeoutMs < 1) {
        m_limits.timeoutMs = 1;
    }
    if (m_limits.maxResponseBytes < 1) {
        m_limits.maxResponseBytes = 1;
    }
}

QqNetworkTransport::~QqNetworkTransport()
{
    while (!m_operations.isEmpty()) {
        Operation *operation = m_operations.begin().value();
        m_operations.remove(operation->id);
        QTimer *timer = operation->timer;
        QPointer<QNetworkReply> reply = operation->reply;
        operation->timer = 0;
        operation->reply = 0;
        delete operation;
        if (timer) {
            timer->stop();
            delete timer;
        }
        if (reply) {
            reply->disconnect(this);
            reply->abort();
            if (reply) {
                reply->deleteLater();
            }
        }
    }
}

bool QqNetworkTransport::get(quint64 operationId,
                             const QUrl &url,
                             QqRequestKind kind,
                             bool containsCookies,
                             bool forceNetwork)
{
    return start(operationId, url, kind, true, QByteArray(), containsCookies,
                 forceNetwork);
}

bool QqNetworkTransport::post(quint64 operationId,
                              const QUrl &url,
                              QqRequestKind kind,
                              const QByteArray &body,
                              bool containsCookies)
{
    return start(operationId, url, kind, false, body, containsCookies, false);
}

bool QqNetworkTransport::start(quint64 operationId,
                               const QUrl &url,
                               QqRequestKind kind,
                               bool isGet,
                               const QByteArray &body,
                               bool containsCookies,
                               bool forceNetwork)
{
    if (operationId == 0 || m_operations.contains(operationId)) {
        return false;
    }

    Operation *operation = new Operation;
    operation->id = operationId;
    ++m_nextGeneration;
    if (m_nextGeneration == 0) {
        ++m_nextGeneration;
    }
    operation->generation = m_nextGeneration;
    operation->kind = kind;
    operation->isGet = isGet;
    operation->allowsCookieJar = containsCookies;
    operation->cacheSensitive = containsCookies || forceNetwork;
    operation->requestBody = body;
    operation->currentUrl = url;
    operation->timer = new QTimer;
    operation->timer->setSingleShot(true);
    const quint64 generation = operation->generation;
    connect(operation->timer, &QTimer::timeout, this,
            [this, operationId, generation]() {
        Operation *active = activeOperation(operationId, generation);
        if (active) {
            finishError(operationId,
                        generation,
                        active->reply.data(),
                        ErrorKind::Timeout,
                        0,
                        0,
                        true);
        }
    });
    m_operations.insert(operationId, operation);

    if (!QqRequestPolicy::allows(kind, url)) {
        finishError(operationId, generation, 0,
                    ErrorKind::Internal, 0, 0, false);
        return true;
    }
    if (!m_manager || m_manager->thread() != thread()) {
        finishError(operationId, generation, 0,
                    ErrorKind::Internal, 0, 0, false);
        return true;
    }

    operation->timer->start(m_limits.timeoutMs);
    issue(operationId, generation);
    return true;
}

void QqNetworkTransport::issue(quint64 operationId, quint64 generation)
{
    Operation *operation = activeOperation(operationId, generation);
    QPointer<QNetworkAccessManager> manager = m_manager;
    if (!operation) {
        return;
    }
    if (!manager || manager->thread() != thread()) {
        finishError(operationId, generation, operation->reply.data(),
                    ErrorKind::Internal, 0, 0, false);
        return;
    }
    QNetworkRequest request = QqRequestPolicy::prepareRequest(
        operation->kind,
        operation->currentUrl,
        operation->isGet,
        operation->cacheSensitive);
    const QNetworkRequest::LoadControl cookieControl =
        operation->allowsCookieJar ? QNetworkRequest::Automatic
                                   : QNetworkRequest::Manual;
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,
                         cookieControl);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,
                         cookieControl);
    QNetworkReply *createdReply = operation->isGet
        ? manager->get(request)
        : manager->post(request, operation->requestBody);
    QPointer<QNetworkReply> reply = createdReply;
    operation = activeOperation(operationId, generation);
    if (!operation) {
        if (reply) {
            reply->abort();
            if (reply) {
                reply->deleteLater();
            }
        }
        return;
    }
    if (!reply) {
        finishError(operationId, generation, 0,
                    ErrorKind::Internal, 0, 0, false);
        return;
    }
    operation->reply = reply;
    operation->responseBody.clear();
    reply->setReadBufferSize(
        qMin(m_limits.maxResponseBytes, ReplyReadChunkSize - 1) + 1);

    connect(reply, &QNetworkReply::metaDataChanged, this,
            [this, operationId, generation, reply]() {
        if (reply) {
            checkResponseSize(operationId, generation, reply.data());
        }
    });
    connect(reply, &QNetworkReply::readyRead, this,
            [this, operationId, generation, reply]() {
        if (reply) {
            readAvailable(operationId, generation, reply.data());
        }
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, operationId, generation, reply]() {
        if (reply) {
            finishReply(operationId, generation, reply.data());
        }
    });
    checkResponseSize(operationId, generation, reply.data());
}

void QqNetworkTransport::readAvailable(quint64 operationId,
                                       quint64 generation,
                                       QNetworkReply *expectedReply)
{
    QPointer<QNetworkReply> reply = expectedReply;
    Operation *operation = activeReplyOperation(
        operationId, generation, reply.data());
    if (!operation) {
        return;
    }
    if (!checkResponseSize(operationId, generation, reply.data())) {
        return;
    }

    char buffer[16 * 1024];
    while (reply) {
        operation = activeReplyOperation(
            operationId, generation, reply.data());
        if (!operation || reply->bytesAvailable() <= 0) {
            return;
        }
        operation = activeReplyOperation(
            operationId, generation, reply.data());
        if (!operation) {
            return;
        }
        const qint64 allowance = m_limits.maxResponseBytes
            - operation->responseBody.size() + 1;
        if (allowance <= 0) {
            finishError(operationId, generation, reply.data(),
                        ErrorKind::Internal, 0, 0, true);
            return;
        }
        const qint64 readLimit = qMin<qint64>(allowance, sizeof(buffer));
        const qint64 count = reply->read(buffer, readLimit);
        operation = activeReplyOperation(
            operationId, generation, reply.data());
        if (!operation) {
            return;
        }
        if (count <= 0) {
            return;
        }
        operation->responseBody.append(buffer, int(count));
        if (operation->responseBody.size() > m_limits.maxResponseBytes) {
            finishError(operationId, generation, reply.data(),
                        ErrorKind::Internal, 0, 0, true);
            return;
        }
    }
}

bool QqNetworkTransport::checkResponseSize(quint64 operationId,
                                           quint64 generation,
                                           QNetworkReply *expectedReply)
{
    QPointer<QNetworkReply> reply = expectedReply;
    Operation *operation = activeReplyOperation(
        operationId, generation, reply.data());
    if (!operation) {
        return false;
    }
    bool ok = false;
    const qint64 contentLength = reply->header(
        QNetworkRequest::ContentLengthHeader).toLongLong(&ok);
    if (ok && contentLength > m_limits.maxResponseBytes) {
        finishError(operationId, generation, reply.data(),
                    ErrorKind::Internal, 0, 0, true);
        return false;
    }
    return true;
}

void QqNetworkTransport::finishReply(quint64 operationId,
                                     quint64 generation,
                                     QNetworkReply *expectedReply)
{
    QPointer<QNetworkReply> reply = expectedReply;
    Operation *operation = activeReplyOperation(
        operationId, generation, reply.data());
    if (!operation) {
        return;
    }
    readAvailable(operationId, generation, reply.data());
    operation = activeReplyOperation(
        operationId, generation, reply.data());
    if (!operation) {
        return;
    }

    const int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->hasRawHeader("Set-Cookie")) {
        operation->cacheSensitive = true;
        QPointer<QNetworkAccessManager> manager = m_manager;
        if (manager && manager->thread() == thread() && manager->cache()) {
            manager->cache()->remove(operation->currentUrl);
            operation = activeReplyOperation(
                operationId, generation, reply.data());
            if (!operation) {
                return;
            }
        }
    }

    if (status == 429) {
        finishError(operationId,
                    generation,
                    reply.data(),
                    ErrorKind::RateLimited,
                    status,
                    retryAfterMs(reply->rawHeader("Retry-After"),
                                 reply->rawHeader("Date")),
                    false);
        return;
    }

    const bool authenticatedRequest = operation->allowsCookieJar;
    const QNetworkReply::NetworkError networkError = reply->error();
    if (authenticatedRequest
            && (status == 401
                || networkError == QNetworkReply::AuthenticationRequiredError)) {
        finishError(operationId,
                    generation,
                    reply.data(),
                    ErrorKind::AuthenticationExpired,
                    status,
                    0,
                    false);
        return;
    }

    const QVariant redirect = reply->attribute(
        QNetworkRequest::RedirectionTargetAttribute);
    if (redirect.isValid() && !redirect.toUrl().isEmpty()) {
        if (!isFollowedRedirectStatus(status)) {
            finishError(operationId, generation, reply.data(),
                        ErrorKind::Internal, status, 0, false);
            return;
        }
        const QByteArray locationHeader = reply->rawHeader("Location");
        const QUrl rawTarget = redirect.toUrl();
        if (!locationHeader.isEmpty()) {
            QString rawAbsolute;
            if (!rawTarget.isRelative()) {
                rawAbsolute = QString::fromLatin1(locationHeader);
            } else if (locationHeader.startsWith("//")) {
                rawAbsolute = QStringLiteral("https:")
                    + QString::fromLatin1(locationHeader);
            }
            if (!rawAbsolute.isEmpty()
                    && !QqRequestPolicy::allows(operation->kind,
                                                rawAbsolute)) {
                finishError(operationId, generation, reply.data(),
                            ErrorKind::Internal, status, 0, false);
                return;
            }
        }
        const QUrl target = operation->currentUrl.resolved(rawTarget);
        ++operation->redirectCount;
        if (!QqRequestPolicy::allowsRedirect(operation->kind,
                                             target,
                                             operation->redirectCount)) {
            finishError(operationId, generation, reply.data(),
                        ErrorKind::Internal, status, 0, false);
            return;
        }
        reply->disconnect(this);
        reply->deleteLater();
        operation->reply = 0;
        operation->currentUrl = target;
        if (!operation->isGet
                && (status == 301 || status == 302 || status == 303)) {
            operation->isGet = true;
            operation->requestBody.clear();
        }
        issue(operationId, generation);
        return;
    }

    if (networkError != QNetworkReply::NoError) {
        finishError(operationId,
                    generation,
                    reply.data(),
                    isOfflineError(networkError) ? ErrorKind::Offline
                                                 : ErrorKind::Internal,
                    status,
                    0,
                    false);
        return;
    }
    if (status < 200 || status >= 300) {
        finishError(operationId, generation, reply.data(),
                    ErrorKind::Internal, status, 0, false);
        return;
    }
    finishSuccess(operationId, generation, reply.data());
}

void QqNetworkTransport::finishSuccess(quint64 operationId,
                                       quint64 generation,
                                       QNetworkReply *expectedReply)
{
    Operation *operation = activeReplyOperation(
        operationId, generation, expectedReply);
    if (!operation) {
        return;
    }
    const QByteArray body = operation->responseBody;
    m_operations.remove(operationId);
    QTimer *timer = operation->timer;
    QPointer<QNetworkReply> reply = operation->reply;
    operation->timer = 0;
    operation->reply = 0;
    delete operation;
    timer->stop();
    delete timer;
    if (reply) {
        reply->disconnect(this);
        reply->deleteLater();
    }
    emit completed(operationId, body);
}

void QqNetworkTransport::finishError(quint64 operationId,
                                     quint64 generation,
                                     QNetworkReply *expectedReply,
                                     ErrorKind kind,
                                     int status,
                                     int retryAfter,
                                     bool abortReply)
{
    Operation *operation = expectedReply
        ? activeReplyOperation(operationId, generation, expectedReply)
        : activeOperation(operationId, generation);
    if (!operation) {
        return;
    }
    ServiceError error;
    error.kind = kind;
    error.operationId = operationId;
    error.retryAfterMs = retryAfter;
    error.diagnosticCode = QStringLiteral("op=%1 host=%2 status=%3 kind=%4")
        .arg(operationId)
        .arg(operation->currentUrl.host())
        .arg(status)
        .arg(errorKindName(kind));

    m_operations.remove(operationId);
    QTimer *timer = operation->timer;
    QPointer<QNetworkReply> reply = operation->reply;
    operation->timer = 0;
    operation->reply = 0;
    delete operation;
    timer->stop();
    delete timer;
    if (reply) {
        reply->disconnect(this);
        if (abortReply) {
            reply->abort();
        }
        if (reply) {
            reply->deleteLater();
        }
    }
    emit failed(error);
}

bool QqNetworkTransport::cancel(quint64 operationId)
{
    Operation *operation = m_operations.value(operationId, 0);
    if (!operation) {
        return false;
    }
    finishError(operationId,
                operation->generation,
                operation->reply.data(),
                ErrorKind::Cancelled,
                0,
                0,
                true);
    return true;
}

QqNetworkTransport::Operation *QqNetworkTransport::activeOperation(
    quint64 operationId, quint64 generation) const
{
    Operation *operation = m_operations.value(operationId, 0);
    return operation && operation->generation == generation ? operation : 0;
}

QqNetworkTransport::Operation *QqNetworkTransport::activeReplyOperation(
    quint64 operationId,
    quint64 generation,
    QNetworkReply *reply) const
{
    Operation *operation = activeOperation(operationId, generation);
    return operation && reply && operation->reply.data() == reply
        ? operation
        : 0;
}

QNetworkAccessManager *QqNetworkTransport::networkAccessManager() const
{
    return m_manager;
}

QNetworkDiskCache *QqNetworkTransport::configureDiskCache(
    const QString &directory, qint64 maximumBytes)
{
    if (!m_manager || m_manager->thread() != thread()
            || maximumBytes <= 0 || maximumBytes > PublicCacheLimit) {
        return 0;
    }
    QqPublicNetworkDiskCache *cache = new QqPublicNetworkDiskCache(m_manager);
    cache->setCacheDirectory(directory);
    cache->setMaximumCacheSize(maximumBytes);
    m_manager->setCache(cache);
    return cache;
}

qint64 QqNetworkTransport::diskCacheSize() const
{
    QNetworkDiskCache *cache = m_manager
        ? qobject_cast<QNetworkDiskCache *>(m_manager->cache()) : 0;
    return cache ? cache->cacheSize() : 0;
}

void QqNetworkTransport::setDiskCacheMaximumBytes(qint64 maximumBytes)
{
    QNetworkDiskCache *cache = m_manager
        ? qobject_cast<QNetworkDiskCache *>(m_manager->cache()) : 0;
    if (cache) {
        cache->setMaximumCacheSize(qBound<qint64>(1, maximumBytes,
                                                  PublicCacheLimit));
    }
}

bool QqNetworkTransport::clearDiskCache()
{
    QNetworkDiskCache *cache = m_manager
        ? qobject_cast<QNetworkDiskCache *>(m_manager->cache()) : 0;
    if (!cache) {
        return true;
    }
    cache->clear();
    return cache->cacheSize() == 0;
}
