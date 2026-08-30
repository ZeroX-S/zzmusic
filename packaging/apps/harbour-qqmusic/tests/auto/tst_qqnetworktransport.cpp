#include <QtTest>

#include "service/qqnetworktransport.h"

#include <QAbstractNetworkCache>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QNetworkAccessManager>
#include <QNetworkCacheMetaData>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QPointer>
#include <QSemaphore>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <functional>

struct ReplyScript
{
    QByteArray body;
    QList<QByteArray> chunks;
    int status = 200;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    QByteArray retryAfter;
    QByteArray date;
    QByteArray setCookie;
    QUrl redirect;
    QByteArray rawLocation;
    qint64 contentLength = -1;
    qint64 maxReadSize = 0;
    int delayMs = 0;
    bool neverFinish = false;
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
        , m_script(script)
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(operation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, script.status);
        if (!script.redirect.isEmpty()) {
            setAttribute(QNetworkRequest::RedirectionTargetAttribute,
                         script.redirect);
            setRawHeader("Location",
                         script.rawLocation.isEmpty()
                             ? script.redirect.toEncoded()
                             : script.rawLocation);
        }
        if (!script.retryAfter.isEmpty()) {
            setRawHeader("Retry-After", script.retryAfter);
        }
        if (!script.date.isEmpty()) {
            setRawHeader("Date", script.date);
        }
        if (!script.setCookie.isEmpty()) {
            setRawHeader("Set-Cookie", script.setCookie);
        }
        if (script.contentLength >= 0) {
            setHeader(QNetworkRequest::ContentLengthHeader,
                      script.contentLength);
        }
        m_chunks = script.chunks;
        if (m_chunks.isEmpty() && !script.body.isEmpty()) {
            m_chunks.append(script.body);
        }
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        if (!script.neverFinish) {
            QTimer::singleShot(script.delayMs, this, SLOT(deliver()));
        }
    }

    void abort() override
    {
        if (!m_finished) {
            m_aborted = true;
            finishNow();
        }
    }

    qint64 bytesAvailable() const override
    {
        return m_available.size() - m_offset + QIODevice::bytesAvailable();
    }

    qint64 totalBytesRead() const
    {
        return m_totalBytesRead;
    }

    bool wasAborted() const
    {
        return m_aborted;
    }

    void emitLateSignals()
    {
        emit readyRead();
        emit finished();
    }

    void setFirstReadHook(const std::function<void()> &hook)
    {
        m_firstReadHook = hook;
    }

private slots:
    void deliver()
    {
        if (m_finished) {
            return;
        }
        if (!m_metadataDelivered) {
            m_metadataDelivered = true;
            emit metaDataChanged();
        }
        if (m_nextChunk < m_chunks.size()) {
            m_available.append(m_chunks.at(m_nextChunk));
            ++m_nextChunk;
            emit readyRead();
            if (m_nextChunk < m_chunks.size()) {
                QTimer::singleShot(m_script.delayMs,
                                   this, SLOT(deliver()));
                return;
            }
        }
        finishNow();
    }

    void finishNow()
    {
        if (m_finished) {
            return;
        }
        m_finished = true;
        if (m_script.error != QNetworkReply::NoError) {
            setError(m_script.error, QStringLiteral("scripted error"));
            emit error(m_script.error);
        }
        setFinished(true);
        emit finished();
    }

protected:
    qint64 readData(char *data, qint64 maximum) override
    {
        if (m_firstReadHook) {
            const std::function<void()> hook = m_firstReadHook;
            m_firstReadHook = std::function<void()>();
            hook();
        }
        const qint64 remaining = m_available.size() - m_offset;
        qint64 count = qMin(maximum, remaining);
        if (m_script.maxReadSize > 0) {
            count = qMin(count, m_script.maxReadSize);
        }
        if (count <= 0) {
            return -1;
        }
        memcpy(data, m_available.constData() + m_offset, size_t(count));
        m_offset += count;
        m_totalBytesRead += count;
        if (m_offset == m_available.size()) {
            m_available.clear();
            m_offset = 0;
        }
        return count;
    }

private:
    ReplyScript m_script;
    QList<QByteArray> m_chunks;
    QByteArray m_available;
    qint64 m_offset = 0;
    qint64 m_totalBytesRead = 0;
    int m_nextChunk = 0;
    bool m_metadataDelivered = false;
    bool m_finished = false;
    bool m_aborted = false;
    std::function<void()> m_firstReadHook;
};

class LateSignalThread : public QThread
{
public:
    LateSignalThread(ScriptedReply *reply, QSemaphore *emitted)
        : m_reply(reply)
        , m_emitted(emitted)
    {
    }

protected:
    void run() override
    {
        if (m_reply) {
            m_reply->emitLateSignals();
        }
        m_emitted->release();
    }

private:
    QPointer<ScriptedReply> m_reply;
    QSemaphore *m_emitted;
};

class ScriptedNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT

public:
    QList<ReplyScript> scripts;
    QList<QNetworkRequest> requests;
    QList<Operation> operations;
    QList<QByteArray> outgoingBodies;
    QList<qint64> finishedReadCounts;
    QList<qint64> finishedReadBufferSizes;
    QList<QPointer<ScriptedReply> > replies;

protected:
    QNetworkReply *createRequest(Operation operation,
                                 const QNetworkRequest &request,
                                 QIODevice *outgoingData) override
    {
        QNetworkRequest effectiveRequest(request);
        if (cookieJar()
                && effectiveRequest.attribute(
                    QNetworkRequest::CookieLoadControlAttribute,
                    QNetworkRequest::Automatic).toInt()
                    == int(QNetworkRequest::Automatic)) {
            const QList<QNetworkCookie> cookies =
                cookieJar()->cookiesForUrl(effectiveRequest.url());
            QByteArray header;
            for (int index = 0; index < cookies.size(); ++index) {
                if (!header.isEmpty()) {
                    header.append("; ");
                }
                header.append(cookies.at(index).toRawForm(
                    QNetworkCookie::NameAndValueOnly));
            }
            if (!header.isEmpty()) {
                effectiveRequest.setRawHeader("Cookie", header);
            }
        }
        requests.append(effectiveRequest);
        operations.append(operation);
        outgoingBodies.append(outgoingData ? outgoingData->readAll()
                                           : QByteArray());
        ReplyScript script;
        if (scripts.isEmpty()) {
            script.error = QNetworkReply::UnknownNetworkError;
        } else {
            script = scripts.takeFirst();
        }
        ScriptedReply *reply = new ScriptedReply(
            effectiveRequest, operation, script, this);
        replies.append(reply);
        connect(reply, &QNetworkReply::metaDataChanged, this,
                [this, effectiveRequest, reply]() {
            if (cookieJar()
                    && effectiveRequest.attribute(
                        QNetworkRequest::CookieSaveControlAttribute,
                        QNetworkRequest::Automatic).toInt()
                        == int(QNetworkRequest::Automatic)
                    && reply->hasRawHeader("Set-Cookie")) {
                cookieJar()->setCookiesFromUrl(
                    QNetworkCookie::parseCookies(
                        reply->rawHeader("Set-Cookie")),
                    effectiveRequest.url());
            }
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            finishedReadCounts.append(reply->totalBytesRead());
            finishedReadBufferSizes.append(reply->readBufferSize());
        });
        return reply;
    }
};

class NetworkManagerThread : public QThread
{
public:
    ~NetworkManagerThread()
    {
        allowExit.release();
        wait();
    }

    ScriptedNetworkAccessManager *manager = 0;
    QSemaphore ready;
    QSemaphore allowExit;

protected:
    void run() override
    {
        ScriptedNetworkAccessManager threadManager;
        manager = &threadManager;
        ready.release();
        allowExit.acquire();
        manager = 0;
    }
};

class TrackingCookieJar : public QNetworkCookieJar
{
    Q_OBJECT

public:
    explicit TrackingCookieJar(QObject *parent = 0)
        : QNetworkCookieJar(parent)
    {
    }

    QList<QNetworkCookie> cookiesForUrl(const QUrl &url) const override
    {
        ++loadCount;
        return QNetworkCookieJar::cookiesForUrl(url);
    }

    bool setCookiesFromUrl(const QList<QNetworkCookie> &cookies,
                           const QUrl &url) override
    {
        ++saveCount;
        return QNetworkCookieJar::setCookiesFromUrl(cookies, url);
    }

    bool seed(const QNetworkCookie &cookie, const QUrl &url)
    {
        return QNetworkCookieJar::setCookiesFromUrl(
            QList<QNetworkCookie>() << cookie, url);
    }

    int cookieCount() const
    {
        return allCookies().size();
    }

    mutable int loadCount = 0;
    int saveCount = 0;
};

class TrackingCache : public QAbstractNetworkCache
{
    Q_OBJECT

public:
    QList<QUrl> removed;

    qint64 cacheSize() const override { return 0; }
    QIODevice *data(const QUrl &) override { return 0; }
    void insert(QIODevice *) override {}
    QNetworkCacheMetaData metaData(const QUrl &) override
    {
        return QNetworkCacheMetaData();
    }
    QIODevice *prepare(const QNetworkCacheMetaData &) override { return 0; }
    bool remove(const QUrl &url) override
    {
        removed.append(url);
        return true;
    }
    void updateMetaData(const QNetworkCacheMetaData &) override {}
    void clear() override {}
};

class QqNetworkTransportTest : public QObject
{
    Q_OBJECT

private:
    static ServiceError errorAt(const QSignalSpy &spy, int row = 0)
    {
        return qvariant_cast<ServiceError>(spy.at(row).at(0));
    }

private slots:
    void initTestCase()
    {
        qRegisterMetaType<ServiceError>("ServiceError");
    }

    void timesOutOnce()
    {
        ScriptedNetworkAccessManager manager;
        ReplyScript script;
        script.neverFinish = true;
        manager.scripts << script;
        QqTransportLimits limits;
        limits.timeoutMs = 15;
        QqNetworkTransport transport(&manager, limits);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(1, QUrl("https://u.y.qq.com/x"),
                              QqRequestKind::PublicMetadata));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(completed.count(), 0);
        QCOMPARE(errorAt(failed).kind, ErrorKind::Timeout);
        QTest::qWait(10);
        QCOMPARE(failed.count(), 1);
    }

    void mapsRateLimitWithoutRetryLoop_data()
    {
        QTest::addColumn<QByteArray>("retryAfter");
        QTest::addColumn<QByteArray>("date");
        QTest::addColumn<int>("expectedRetryAfterMs");

        const QByteArray reference = "Sun, 06 Nov 1994 08:49:37 GMT";
        QTest::newRow("numeric") << QByteArray("4") << QByteArray() << 4000;
        QTest::newRow("numeric-zero") << QByteArray("0") << QByteArray() << 1000;
        QTest::newRow("numeric-negative") << QByteArray("-4") << QByteArray() << 1000;
        QTest::newRow("numeric-malformed") << QByteArray("later") << QByteArray() << 1000;
        QTest::newRow("numeric-huge")
            << QByteArray("999999999999999999999999") << QByteArray() << 60000;
        QTest::newRow("http-date-future")
            << QByteArray("Sun, 06 Nov 1994 08:49:41 GMT") << reference << 4000;
        QTest::newRow("http-date-past")
            << QByteArray("Sun, 06 Nov 1994 08:49:36 GMT") << reference << 1000;
        QTest::newRow("http-date-clamped")
            << QByteArray("Sun, 06 Nov 1994 09:49:37 GMT") << reference << 60000;
        QTest::newRow("http-date-invalid")
            << QByteArray("Sun, 99 Nov 1994 08:49:41 GMT") << reference << 1000;
    }

    void mapsRateLimitWithoutRetryLoop()
    {
        QFETCH(QByteArray, retryAfter);
        QFETCH(QByteArray, date);
        QFETCH(int, expectedRetryAfterMs);
        ScriptedNetworkAccessManager manager;
        ReplyScript script;
        script.status = 429;
        script.retryAfter = retryAfter;
        script.date = date;
        manager.scripts << script;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(2, QUrl("https://u.y.qq.com/x"),
                              QqRequestKind::Search));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(completed.count(), 0);
        const ServiceError error = errorAt(failed);
        QCOMPARE(error.kind, ErrorKind::RateLimited);
        QCOMPARE(error.operationId, quint64(2));
        QCOMPARE(error.retryAfterMs, expectedRetryAfterMs);
        QCOMPARE(error.diagnosticCode,
                 QString("op=2 host=u.y.qq.com status=429 kind=RateLimited"));
        QCOMPARE(manager.requests.count(), 1);
        QTest::qWait(10);
        QCOMPARE(completed.count() + failed.count(), 1);
    }

    void rejectsOversizedBodyOnce()
    {
        ScriptedNetworkAccessManager manager;
        ReplyScript script;
        script.body = QByteArray(17, 'x');
        manager.scripts << script;
        QqTransportLimits limits;
        limits.maxResponseBytes = 16;
        QqNetworkTransport transport(&manager, limits);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(3, QUrl("https://u.y.qq.com/x"),
                              QqRequestKind::PublicMetadata));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(completed.count(), 0);
        QCOMPARE(errorAt(failed).kind, ErrorKind::Internal);
        QCOMPARE(QqTransportLimits().maxResponseBytes,
                 qint64(4 * 1024 * 1024));
    }

    void acceptsExactResponseLimitDeliveredInChunks()
    {
        const qint64 limit = QqTransportLimits().maxResponseBytes;
        ScriptedNetworkAccessManager manager;
        ReplyScript script;
        for (int index = 0; index < 64; ++index) {
            script.chunks.append(QByteArray(64 * 1024, 'x'));
        }
        script.contentLength = limit;
        manager.scripts << script;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(20, QUrl("https://u.y.qq.com/exact"),
                              QqRequestKind::PublicMetadata));
        QTRY_COMPARE(completed.count() + failed.count(), 1);
        QCOMPARE(completed.count(), 1);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(completed.at(0).at(1).toByteArray().size(), int(limit));
        QCOMPARE(manager.finishedReadCounts,
                 QList<qint64>() << limit);
        QCOMPARE(manager.finishedReadBufferSizes.count(), 1);
        QVERIFY(manager.finishedReadBufferSizes.at(0) > 0);
        QVERIFY(manager.finishedReadBufferSizes.at(0) <= 64 * 1024);
    }

    void rejectsOneByteOverLimitDeliveredInChunks()
    {
        const qint64 limit = QqTransportLimits().maxResponseBytes;
        ScriptedNetworkAccessManager manager;
        ReplyScript script;
        for (int index = 0; index < 64; ++index) {
            script.chunks.append(QByteArray(64 * 1024, 'x'));
        }
        script.chunks.append(QByteArray(1, 'x'));
        manager.scripts << script;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(21, QUrl("https://u.y.qq.com/over"),
                              QqRequestKind::PublicMetadata));
        QTRY_COMPARE(completed.count() + failed.count(), 1);
        QCOMPARE(completed.count(), 0);
        QCOMPARE(failed.count(), 1);
        QCOMPARE(errorAt(failed).kind, ErrorKind::Internal);
        QCOMPARE(manager.finishedReadCounts.count(), 1);
        QVERIFY(manager.finishedReadCounts.at(0) <= limit + 1);
    }

    void rejectsOversizedContentLengthBeforeReading()
    {
        const qint64 limit = QqTransportLimits().maxResponseBytes;
        ScriptedNetworkAccessManager manager;
        ReplyScript script;
        script.body = "must-not-read";
        script.contentLength = limit + 1;
        manager.scripts << script;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(22, QUrl("https://u.y.qq.com/length"),
                              QqRequestKind::PublicMetadata));
        QTRY_COMPARE(completed.count() + failed.count(), 1);
        QCOMPARE(completed.count(), 0);
        QCOMPARE(failed.count(), 1);
        QCOMPARE(manager.finishedReadCounts, QList<qint64>() << qint64(0));
    }

    void rejectsBodyThatExceedsClaimedContentLengthAndLimit()
    {
        ScriptedNetworkAccessManager manager;
        ReplyScript script;
        script.contentLength = 4;
        script.chunks << QByteArray("1234") << QByteArray(13, 'x');
        manager.scripts << script;
        QqTransportLimits limits;
        limits.maxResponseBytes = 16;
        QqNetworkTransport transport(&manager, limits);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(23, QUrl("https://u.y.qq.com/lying"),
                              QqRequestKind::PublicMetadata));
        QTRY_COMPARE(completed.count() + failed.count(), 1);
        QCOMPARE(completed.count(), 0);
        QCOMPARE(failed.count(), 1);
        QCOMPARE(manager.finishedReadCounts.count(), 1);
        QVERIFY(manager.finishedReadCounts.at(0) <= 17);
    }

    void drainsShortReadsWithinLimit()
    {
        ScriptedNetworkAccessManager manager;
        ReplyScript script;
        script.body = "short-read-success";
        script.contentLength = script.body.size();
        script.maxReadSize = 3;
        manager.scripts << script;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(24, QUrl("https://u.y.qq.com/short"),
                              QqRequestKind::PublicMetadata));
        QTRY_COMPARE(completed.count() + failed.count(), 1);
        QCOMPARE(completed.count(), 1);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(completed.at(0).at(1).toByteArray(), script.body);
        QCOMPARE(manager.finishedReadCounts,
                 QList<qint64>() << qint64(script.body.size()));
    }

    void mapsOfflineError_data()
    {
        QTest::addColumn<int>("networkError");
        QTest::addColumn<int>("expectedKind");

        const int offline = int(ErrorKind::Offline);
        const int internal = int(ErrorKind::Internal);
        QTest::newRow("connection-refused")
            << int(QNetworkReply::ConnectionRefusedError) << offline;
        QTest::newRow("remote-closed")
            << int(QNetworkReply::RemoteHostClosedError) << offline;
        QTest::newRow("host-not-found")
            << int(QNetworkReply::HostNotFoundError) << offline;
        QTest::newRow("timeout")
            << int(QNetworkReply::TimeoutError) << offline;
        QTest::newRow("temporary-network")
            << int(QNetworkReply::TemporaryNetworkFailureError) << offline;
        QTest::newRow("network-session")
            << int(QNetworkReply::NetworkSessionFailedError) << offline;
        QTest::newRow("proxy-connection-refused")
            << int(QNetworkReply::ProxyConnectionRefusedError) << offline;
        QTest::newRow("proxy-connection-closed")
            << int(QNetworkReply::ProxyConnectionClosedError) << offline;
        QTest::newRow("proxy-not-found")
            << int(QNetworkReply::ProxyNotFoundError) << offline;
        QTest::newRow("proxy-timeout")
            << int(QNetworkReply::ProxyTimeoutError) << offline;
        QTest::newRow("proxy-authentication")
            << int(QNetworkReply::ProxyAuthenticationRequiredError) << internal;
        QTest::newRow("operation-cancelled")
            << int(QNetworkReply::OperationCanceledError) << internal;
        QTest::newRow("ssl-handshake")
            << int(QNetworkReply::SslHandshakeFailedError) << internal;
        QTest::newRow("background-disallowed")
            << int(QNetworkReply::BackgroundRequestNotAllowedError) << internal;
        QTest::newRow("too-many-redirects")
            << int(QNetworkReply::TooManyRedirectsError) << internal;
        QTest::newRow("insecure-redirect")
            << int(QNetworkReply::InsecureRedirectError) << internal;
        QTest::newRow("unknown-network")
            << int(QNetworkReply::UnknownNetworkError) << internal;
        QTest::newRow("unknown-proxy")
            << int(QNetworkReply::UnknownProxyError) << internal;
    }

    void mapsOfflineError()
    {
        QFETCH(int, networkError);
        QFETCH(int, expectedKind);
        ScriptedNetworkAccessManager manager;
        ReplyScript script;
        script.error = static_cast<QNetworkReply::NetworkError>(networkError);
        manager.scripts << script;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(4, QUrl("https://u.y.qq.com/x"),
                              QqRequestKind::PublicMetadata));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(completed.count(), 0);
        const ServiceError error = errorAt(failed);
        QCOMPARE(int(error.kind), expectedKind);
        const QString kindName = expectedKind == int(ErrorKind::Offline)
            ? QStringLiteral("Offline") : QStringLiteral("Internal");
        QCOMPARE(error.diagnosticCode,
                 QString("op=4 host=u.y.qq.com status=200 kind=%1")
                     .arg(kindName));
    }

    void rejectsInvalidRedirect()
    {
        ScriptedNetworkAccessManager manager;
        ReplyScript redirect;
        redirect.status = 302;
        redirect.redirect = QUrl("http://ws.stream.qqmusic.qq.com/audio");
        manager.scripts << redirect;
        QqNetworkTransport transport(&manager);
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(5,
                              QUrl("https://ws.stream.qqmusic.qq.com/start"),
                              QqRequestKind::Media));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(errorAt(failed).kind, ErrorKind::Internal);
        QCOMPARE(manager.requests.count(), 1);
    }

    void followsValidRelativeRedirect()
    {
        ScriptedNetworkAccessManager manager;
        ReplyScript redirect;
        redirect.status = 302;
        redirect.redirect = QUrl("/next");
        ReplyScript success;
        success.body = "ok";
        manager.scripts << redirect << success;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(6, QUrl("https://u.y.qq.com/start"),
                              QqRequestKind::PublicMetadata));
        QTRY_COMPARE(completed.count(), 1);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(completed.at(0).at(0).toULongLong(), quint64(6));
        QCOMPARE(completed.at(0).at(1).toByteArray(), QByteArray("ok"));
        QCOMPARE(manager.requests.count(), 2);
        QCOMPARE(manager.requests.at(1).url(),
                 QUrl("https://u.y.qq.com/next"));
    }

    void followsRedirectWithExpectedMethod_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<bool>("post");
        QTest::addColumn<int>("expectedRedirectOperation");
        QTest::addColumn<QByteArray>("expectedRedirectBody");
        QTest::addColumn<bool>("expectedRedirectContentType");

        const int get = int(QNetworkAccessManager::GetOperation);
        const int post = int(QNetworkAccessManager::PostOperation);
        const QByteArray body("{\"query\":\"x\"}");
        const int statuses[] = { 301, 302, 303, 307, 308 };
        for (int index = 0; index < 5; ++index) {
            const int status = statuses[index];
            QTest::newRow(qPrintable(QString("get-%1").arg(status)))
                << status << false << get << QByteArray() << false;
            const bool preservesPost = status == 307 || status == 308;
            QTest::newRow(qPrintable(QString("post-%1").arg(status)))
                << status
                << true
                << (preservesPost ? post : get)
                << (preservesPost ? body : QByteArray())
                << preservesPost;
        }
    }

    void followsRedirectWithExpectedMethod()
    {
        QFETCH(int, status);
        QFETCH(bool, post);
        QFETCH(int, expectedRedirectOperation);
        QFETCH(QByteArray, expectedRedirectBody);
        QFETCH(bool, expectedRedirectContentType);
        const QByteArray body("{\"query\":\"x\"}");
        ScriptedNetworkAccessManager manager;
        ReplyScript redirect;
        redirect.status = status;
        redirect.redirect = QUrl("/next");
        ReplyScript success;
        success.body = "ok";
        manager.scripts << redirect << success;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        if (post) {
            QVERIFY(transport.post(18, QUrl("https://u.y.qq.com/start"),
                                   QqRequestKind::Search, body));
        } else {
            QVERIFY(transport.get(18, QUrl("https://u.y.qq.com/start"),
                                  QqRequestKind::Search));
        }
        QTRY_COMPARE(completed.count() + failed.count(), 1);
        QCOMPARE(completed.count(), 1);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(completed.at(0).at(1).toByteArray(), QByteArray("ok"));
        QCOMPARE(manager.requests.count(), 2);
        QCOMPARE(manager.operations.at(0),
                 post ? QNetworkAccessManager::PostOperation
                      : QNetworkAccessManager::GetOperation);
        QCOMPARE(manager.outgoingBodies.at(0),
                 post ? body : QByteArray());
        QCOMPARE(manager.operations.at(1),
                 static_cast<QNetworkAccessManager::Operation>(
                     expectedRedirectOperation));
        QCOMPARE(manager.outgoingBodies.at(1), expectedRedirectBody);
        QCOMPARE(manager.requests.at(1).url(),
                 QUrl("https://u.y.qq.com/next"));
        QCOMPARE(manager.requests.at(1).rawHeader("Content-Type"),
                 expectedRedirectContentType
                     ? QByteArray("application/json") : QByteArray());
        QTest::qWait(10);
        QCOMPARE(completed.count() + failed.count(), 1);
    }

    void rejectsUnsupportedRedirectStatus_data()
    {
        QTest::addColumn<int>("status");

        const int statuses[] = { 200, 300, 304, 305, 306, 309, 400, 500 };
        for (int index = 0; index < 8; ++index) {
            QTest::newRow(qPrintable(QString::number(statuses[index])))
                << statuses[index];
        }
    }

    void rejectsUnsupportedRedirectStatus()
    {
        QFETCH(int, status);
        ScriptedNetworkAccessManager manager;
        ReplyScript redirect;
        redirect.status = status;
        redirect.redirect = QUrl("/must-not-follow");
        ReplyScript unusedSuccess;
        unusedSuccess.body = "must-not-complete";
        manager.scripts << redirect << unusedSuccess;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(19, QUrl("https://u.y.qq.com/start"),
                              QqRequestKind::PublicMetadata));
        QTRY_COMPARE(completed.count() + failed.count(), 1);
        QCOMPARE(completed.count(), 0);
        QCOMPARE(failed.count(), 1);
        const ServiceError error = errorAt(failed);
        QCOMPARE(error.kind, ErrorKind::Internal);
        QCOMPARE(error.diagnosticCode,
                 QString("op=19 host=u.y.qq.com status=%1 kind=Internal")
                     .arg(status));
        QCOMPARE(manager.requests.count(), 1);
        QCOMPARE(manager.scripts.count(), 1);
    }

    void limitsEachOperationToFiveNetworkRequests()
    {
        ScriptedNetworkAccessManager manager;
        for (int index = 1; index <= 5; ++index) {
            ReplyScript redirect;
            redirect.status = 302;
            redirect.redirect = QUrl(QString("/hop-%1").arg(index));
            manager.scripts << redirect;
        }
        ReplyScript unusedSuccess;
        unusedSuccess.body = "must-not-be-requested";
        manager.scripts << unusedSuccess;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(15, QUrl("https://u.y.qq.com/start"),
                              QqRequestKind::PublicMetadata));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(completed.count(), 0);
        const ServiceError error = errorAt(failed);
        QCOMPARE(error.kind, ErrorKind::Internal);
        QCOMPARE(error.operationId, quint64(15));
        QCOMPARE(error.diagnosticCode,
                 QString("op=15 host=u.y.qq.com status=302 kind=Internal"));
        QCOMPARE(manager.requests.count(), 5);
        QCOMPARE(manager.scripts.count(), 1);
        QTest::qWait(10);
        QCOMPARE(completed.count() + failed.count(), 1);
        QCOMPARE(manager.requests.count(), 5);
    }

    void rejectsMalformedSchemeRelativeRedirect()
    {
        ScriptedNetworkAccessManager manager;
        ReplyScript redirect;
        redirect.status = 302;
        redirect.redirect = QUrl("//ws.stream.qqmusic.qq.com:/audio");
        redirect.rawLocation = "//ws.stream.qqmusic.qq.com:/audio";
        manager.scripts << redirect;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(61,
                              QUrl("https://ws.stream.qqmusic.qq.com/start"),
                              QqRequestKind::Media));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(completed.count(), 0);
        QCOMPARE(errorAt(failed).kind, ErrorKind::Internal);
        QCOMPARE(manager.requests.count(), 1);
    }

    void cancellationSuppressesLateCompletion()
    {
        ScriptedNetworkAccessManager manager;
        ReplyScript script;
        script.body = "late";
        script.delayMs = 1;
        manager.scripts << script;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(7, QUrl("https://u.y.qq.com/x"),
                              QqRequestKind::PublicMetadata));
        QVERIFY(transport.cancel(7));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(errorAt(failed).kind, ErrorKind::Cancelled);
        QTest::qWait(10);
        QCOMPARE(completed.count() + failed.count(), 1);
    }

    void reusedOperationIdIgnoresQueuedOldReplySignals()
    {
        ScriptedNetworkAccessManager manager;
        ReplyScript oldScript;
        oldScript.neverFinish = true;
        ReplyScript newScript;
        newScript.body = "new";
        newScript.delayMs = 20;
        manager.scripts << oldScript << newScript;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(25, QUrl("https://u.y.qq.com/old"),
                              QqRequestKind::PublicMetadata));
        QCOMPARE(manager.replies.count(), 1);
        QSemaphore emitted;
        LateSignalThread lateSignals(manager.replies.at(0), &emitted);
        lateSignals.start();
        QVERIFY(emitted.tryAcquire(1, 1000));
        QVERIFY(lateSignals.wait(1000));

        QVERIFY(transport.cancel(25));
        QCOMPARE(failed.count(), 1);
        QCOMPARE(errorAt(failed).kind, ErrorKind::Cancelled);
        QVERIFY(transport.get(25, QUrl("https://u.y.qq.com/new"),
                              QqRequestKind::PublicMetadata));
        QTRY_COMPARE(completed.count(), 1);
        QCOMPARE(completed.at(0).at(0).toULongLong(), quint64(25));
        QCOMPARE(completed.at(0).at(1).toByteArray(), QByteArray("new"));
        QCOMPARE(failed.count(), 1);
        QCOMPARE(manager.requests.count(), 2);
        QTest::qWait(30);
        QCOMPARE(completed.count() + failed.count(), 2);
    }

    void reusedOperationIdIgnoresReentrantOldReadCompletion()
    {
        ScriptedNetworkAccessManager manager;
        ReplyScript oldScript;
        oldScript.body = "old";
        ReplyScript newScript;
        newScript.body = "new";
        manager.scripts << oldScript << newScript;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(28, QUrl("https://u.y.qq.com/old"),
                              QqRequestKind::PublicMetadata));
        QCOMPARE(manager.replies.count(), 1);
        bool cancelled = false;
        bool reused = false;
        manager.replies.at(0)->setFirstReadHook([&]() {
            cancelled = transport.cancel(28);
            reused = transport.get(28, QUrl("https://u.y.qq.com/new"),
                                   QqRequestKind::PublicMetadata);
        });

        QTRY_COMPARE(completed.count(), 1);
        QVERIFY(cancelled);
        QVERIFY(reused);
        QCOMPARE(failed.count(), 1);
        QCOMPARE(errorAt(failed).kind, ErrorKind::Cancelled);
        QCOMPARE(completed.at(0).at(0).toULongLong(), quint64(28));
        QCOMPARE(completed.at(0).at(1).toByteArray(), QByteArray("new"));
        QCOMPARE(manager.requests.count(), 2);
        QTest::qWait(10);
        QCOMPARE(completed.count() + failed.count(), 2);
    }

    void guardsBorrowedManagerLifetime()
    {
        ScriptedNetworkAccessManager *manager =
            new ScriptedNetworkAccessManager;
        ReplyScript script;
        script.neverFinish = true;
        manager->scripts << script;
        QqNetworkTransport *transport = new QqNetworkTransport(manager);

        QVERIFY(transport->get(26, QUrl("https://u.y.qq.com/active"),
                               QqRequestKind::PublicMetadata));
        delete manager;
        const bool managerPointerCleared =
            transport->networkAccessManager() == 0;
        QSignalSpy failed(transport, SIGNAL(failed(ServiceError)));
        const bool rejectedWithoutManager = transport->get(
            29,
            QUrl("https://u.y.qq.com/after-manager-destruction"),
            QqRequestKind::PublicMetadata);
        const bool failedOnce = failed.count() == 1
            && errorAt(failed).kind == ErrorKind::Internal;
        delete transport;

        QVERIFY(managerPointerCleared);
        QVERIFY(rejectedWithoutManager);
        QVERIFY(failedOnce);
    }

    void rejectsBorrowedManagerFromDifferentThread()
    {
        NetworkManagerThread managerThread;
        managerThread.start();
        QVERIFY(managerThread.ready.tryAcquire(1, 1000));
        QVERIFY(managerThread.manager);

        QqNetworkTransport transport(managerThread.manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(30,
                              QUrl("https://u.y.qq.com/cross-thread"),
                              QqRequestKind::PublicMetadata));
        QCOMPARE(completed.count(), 0);
        QCOMPARE(failed.count(), 1);
        QCOMPARE(errorAt(failed).kind, ErrorKind::Internal);
        QVERIFY(!transport.cancel(30));
    }

    void destructorAbortsAndDeletesActiveBorrowedReply()
    {
        ScriptedNetworkAccessManager *manager =
            new ScriptedNetworkAccessManager;
        QPointer<ScriptedNetworkAccessManager> managerGuard(manager);
        ReplyScript script;
        script.neverFinish = true;
        manager->scripts << script;
        QqNetworkTransport *transport = new QqNetworkTransport(manager);

        QVERIFY(transport->get(27, QUrl("https://u.y.qq.com/active"),
                               QqRequestKind::PublicMetadata));
        QCOMPARE(manager->replies.count(), 1);
        QPointer<ScriptedReply> reply = manager->replies.at(0);
        delete transport;
        const bool managerSurvived = !managerGuard.isNull();
        const bool replyWasAborted = reply && reply->wasAborted();
        QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        const bool replyWasDeleted = reply.isNull();
        delete manager;

        QVERIFY(managerSurvived);
        QVERIFY(replyWasAborted);
        QVERIFY(replyWasDeleted);
    }

    void rejectsDuplicateActiveOperationId()
    {
        ScriptedNetworkAccessManager manager;
        ReplyScript script;
        script.neverFinish = true;
        manager.scripts << script;
        QqNetworkTransport transport(&manager);

        QVERIFY(transport.get(8, QUrl("https://u.y.qq.com/x"),
                              QqRequestKind::PublicMetadata));
        QVERIFY(!transport.get(8, QUrl("https://u.y.qq.com/other"),
                               QqRequestKind::PublicMetadata));
        QCOMPARE(manager.requests.count(), 1);
        QVERIFY(transport.cancel(8));
    }

    void appliesRequestHeadersAndCacheClassification()
    {
        ScriptedNetworkAccessManager manager;
        ReplyScript publicReply;
        publicReply.body = "public";
        ReplyScript searchReply;
        searchReply.body = "search";
        ReplyScript cookieReply;
        cookieReply.body = "cookie";
        manager.scripts << publicReply << searchReply << cookieReply;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));

        QVERIFY(transport.get(9, QUrl("https://u.y.qq.com/public"),
                              QqRequestKind::PublicMetadata));
        QTRY_COMPARE(completed.count(), 1);
        QVERIFY(transport.post(10, QUrl("https://u.y.qq.com/search"),
                               QqRequestKind::Search, QByteArray("body")));
        QTRY_COMPARE(completed.count(), 2);
        QVERIFY(transport.get(11, QUrl("https://u.y.qq.com/cookie"),
                              QqRequestKind::PublicMetadata, true));
        QTRY_COMPARE(completed.count(), 3);

        QCOMPARE(manager.requests.at(0).attribute(
                     QNetworkRequest::CacheLoadControlAttribute).toInt(),
                 int(QNetworkRequest::PreferCache));
        QCOMPARE(manager.requests.at(0).attribute(
                     QNetworkRequest::CookieLoadControlAttribute).toInt(),
                 int(QNetworkRequest::Manual));
        QCOMPARE(manager.requests.at(0).attribute(
                     QNetworkRequest::CookieSaveControlAttribute).toInt(),
                 int(QNetworkRequest::Manual));
        QVERIFY(!manager.requests.at(0).hasRawHeader("Content-Type"));
        QCOMPARE(manager.requests.at(0).rawHeader("Origin"),
                 QByteArray("https://y.qq.com"));
        QCOMPARE(manager.operations.at(0), QNetworkAccessManager::GetOperation);
        QVERIFY(manager.outgoingBodies.at(0).isEmpty());
        QCOMPARE(manager.requests.at(1).attribute(
                     QNetworkRequest::CacheLoadControlAttribute).toInt(),
                 int(QNetworkRequest::AlwaysNetwork));
        QCOMPARE(manager.requests.at(1).rawHeader("Content-Type"),
                 QByteArray("application/json"));
        QCOMPARE(manager.operations.at(1), QNetworkAccessManager::PostOperation);
        QCOMPARE(manager.outgoingBodies.at(1), QByteArray("body"));
        QCOMPARE(manager.requests.at(2).attribute(
                     QNetworkRequest::CacheSaveControlAttribute).toBool(),
                 false);
        QCOMPARE(manager.requests.at(2).attribute(
                     QNetworkRequest::CookieLoadControlAttribute).toInt(),
                 int(QNetworkRequest::Automatic));
        QCOMPARE(manager.requests.at(2).attribute(
                     QNetworkRequest::CookieSaveControlAttribute).toInt(),
                 int(QNetworkRequest::Automatic));
    }

    void isolatesAnonymousRequestsFromInstalledCookieJar()
    {
        ScriptedNetworkAccessManager manager;
        TrackingCookieJar *jar = new TrackingCookieJar(&manager);
        manager.setCookieJar(jar);
        QNetworkCookie existing("session", "seed");
        existing.setDomain(".y.qq.com");
        existing.setPath("/");
        existing.setSecure(true);
        QVERIFY(jar->seed(existing, QUrl("https://u.y.qq.com/")));

        ReplyScript authenticated;
        authenticated.body = "account";
        authenticated.setCookie =
            "session=updated; Domain=.y.qq.com; Path=/; Secure";
        ReplyScript anonymous;
        anonymous.body = "public";
        anonymous.setCookie =
            "anonymous=blocked; Domain=.y.qq.com; Path=/; Secure";
        manager.scripts << authenticated << anonymous;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(16, QUrl("https://u.y.qq.com/account"),
                              QqRequestKind::AuthenticatedAccount, true));
        QTRY_COMPARE(completed.count(), 1);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(manager.requests.at(0).attribute(
                     QNetworkRequest::CookieLoadControlAttribute).toInt(),
                 int(QNetworkRequest::Automatic));
        QCOMPARE(manager.requests.at(0).attribute(
                     QNetworkRequest::CookieSaveControlAttribute).toInt(),
                 int(QNetworkRequest::Automatic));
        QVERIFY(manager.requests.at(0).hasRawHeader("Cookie"));
        QVERIFY(jar->loadCount > 0);
        const int savesAfterAuthenticated = jar->saveCount;
        const int cookiesAfterAuthenticated = jar->cookieCount();

        QVERIFY(transport.get(17, QUrl("https://u.y.qq.com/public"),
                              QqRequestKind::PublicMetadata, false));
        QTRY_COMPARE(completed.count(), 2);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(manager.requests.at(1).attribute(
                     QNetworkRequest::CookieLoadControlAttribute).toInt(),
                 int(QNetworkRequest::Manual));
        QCOMPARE(manager.requests.at(1).attribute(
                     QNetworkRequest::CookieSaveControlAttribute).toInt(),
                 int(QNetworkRequest::Manual));
        QCOMPARE(manager.requests.at(1).attribute(
                     QNetworkRequest::CacheLoadControlAttribute).toInt(),
                 int(QNetworkRequest::PreferCache));
        QCOMPARE(manager.requests.at(1).attribute(
                     QNetworkRequest::CacheSaveControlAttribute).toBool(),
                 true);
        QVERIFY(!manager.requests.at(1).hasRawHeader("Cookie"));
        QCOMPARE(jar->saveCount, savesAfterAuthenticated);
        QCOMPARE(jar->cookieCount(), cookiesAfterAuthenticated);
    }

    void mapsOnlyExplicitAuthenticatedAccountChallengesToExpiry_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<int>("networkError");
        QTest::addColumn<int>("requestKind");
        QTest::addColumn<bool>("containsCookies");
        QTest::addColumn<int>("expectedKind");

        QTest::newRow("account-http-401")
            << 401 << int(QNetworkReply::NoError)
            << int(QqRequestKind::AuthenticatedAccount) << true
            << int(ErrorKind::AuthenticationExpired);
        QTest::newRow("account-http-403")
            << 403 << int(QNetworkReply::NoError)
            << int(QqRequestKind::AuthenticatedAccount) << true
            << int(ErrorKind::Internal);
        QTest::newRow("account-authentication-error")
            << 401 << int(QNetworkReply::AuthenticationRequiredError)
            << int(QqRequestKind::AuthenticatedAccount) << true
            << int(ErrorKind::AuthenticationExpired);
        QTest::newRow("account-content-access-denied")
            << 403 << int(QNetworkReply::ContentAccessDenied)
            << int(QqRequestKind::AuthenticatedAccount) << true
            << int(ErrorKind::Internal);
        QTest::newRow("public-http-401")
            << 401 << int(QNetworkReply::NoError)
            << int(QqRequestKind::PublicMetadata) << false
            << int(ErrorKind::Internal);
        QTest::newRow("public-content-access-denied")
            << 403 << int(QNetworkReply::ContentAccessDenied)
            << int(QqRequestKind::PublicMetadata) << false
            << int(ErrorKind::Internal);
        QTest::newRow("vkey-content-access-denied")
            << 403 << int(QNetworkReply::ContentAccessDenied)
            << int(QqRequestKind::Vkey) << true
            << int(ErrorKind::Internal);
        QTest::newRow("vkey-authentication-error")
            << 401 << int(QNetworkReply::AuthenticationRequiredError)
            << int(QqRequestKind::Vkey) << true
            << int(ErrorKind::AuthenticationExpired);
        QTest::newRow("search-http-401-with-session")
            << 401 << int(QNetworkReply::NoError)
            << int(QqRequestKind::Search) << true
            << int(ErrorKind::AuthenticationExpired);
        QTest::newRow("search-http-401-anonymous")
            << 401 << int(QNetworkReply::NoError)
            << int(QqRequestKind::Search) << false
            << int(ErrorKind::Internal);
    }

    void mapsOnlyExplicitAuthenticatedAccountChallengesToExpiry()
    {
        QFETCH(int, status);
        QFETCH(int, networkError);
        QFETCH(int, requestKind);
        QFETCH(bool, containsCookies);
        QFETCH(int, expectedKind);

        ScriptedNetworkAccessManager manager;
        ReplyScript script;
        script.status = status;
        script.error = QNetworkReply::NetworkError(networkError);
        manager.scripts << script;
        QqNetworkTransport transport(&manager);
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(
            62,
            QUrl("https://u.y.qq.com/account"),
            QqRequestKind(requestKind),
            containsCookies));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(int(errorAt(failed).kind), expectedKind);
    }

    void removesSetCookieResponsesFromCache()
    {
        ScriptedNetworkAccessManager manager;
        TrackingCache *cache = new TrackingCache;
        manager.setCache(cache);
        ReplyScript script;
        script.body = "private";
        script.setCookie = "session=secret; Secure";
        manager.scripts << script;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));

        QVERIFY(transport.get(12, QUrl("https://u.y.qq.com/public"),
                              QqRequestKind::PublicMetadata));
        QTRY_COMPARE(completed.count(), 1);
        QCOMPARE(cache->removed,
                 QList<QUrl>() << QUrl("https://u.y.qq.com/public"));
    }

    void setCookieRedirectsRemainNonCacheable()
    {
        ScriptedNetworkAccessManager manager;
        TrackingCookieJar *jar = new TrackingCookieJar(&manager);
        manager.setCookieJar(jar);
        QNetworkCookie existing("login", "seed");
        existing.setDomain(".y.qq.com");
        existing.setPath("/");
        existing.setSecure(true);
        QVERIFY(jar->seed(existing, QUrl("https://u.y.qq.com/")));
        const int initialCookieCount = jar->cookieCount();
        const int initialLoadCount = jar->loadCount;
        const int initialSaveCount = jar->saveCount;
        TrackingCache *cache = new TrackingCache;
        manager.setCache(cache);
        ReplyScript cookieRedirect;
        cookieRedirect.status = 302;
        cookieRedirect.redirect = QUrl("/cookie-hop");
        cookieRedirect.setCookie = "session=secret; Secure";
        ReplyScript secondRedirect;
        secondRedirect.status = 302;
        secondRedirect.redirect = QUrl("/final");
        ReplyScript success;
        success.body = "ok";
        manager.scripts << cookieRedirect << secondRedirect << success;
        QqNetworkTransport transport(&manager);
        QSignalSpy completed(&transport, SIGNAL(completed(quint64,QByteArray)));
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(14, QUrl("https://u.y.qq.com/start"),
                              QqRequestKind::PublicMetadata));
        QTRY_COMPARE(completed.count(), 1);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(manager.requests.count(), 3);
        QCOMPARE(manager.requests.at(0).attribute(
                     QNetworkRequest::CacheLoadControlAttribute).toInt(),
                 int(QNetworkRequest::PreferCache));
        QCOMPARE(manager.requests.at(0).attribute(
                     QNetworkRequest::CacheSaveControlAttribute).toBool(),
                 true);
        for (int index = 0; index < manager.requests.size(); ++index) {
            QCOMPARE(manager.requests.at(index).attribute(
                         QNetworkRequest::CookieLoadControlAttribute).toInt(),
                     int(QNetworkRequest::Manual));
            QCOMPARE(manager.requests.at(index).attribute(
                         QNetworkRequest::CookieSaveControlAttribute).toInt(),
                     int(QNetworkRequest::Manual));
            QVERIFY(!manager.requests.at(index).hasRawHeader("Cookie"));
        }
        for (int index = 1; index < manager.requests.size(); ++index) {
            QCOMPARE(manager.requests.at(index).attribute(
                         QNetworkRequest::CacheLoadControlAttribute).toInt(),
                     int(QNetworkRequest::AlwaysNetwork));
            QCOMPARE(manager.requests.at(index).attribute(
                         QNetworkRequest::CacheSaveControlAttribute).toBool(),
                     false);
        }
        QCOMPARE(jar->loadCount, initialLoadCount);
        QCOMPARE(jar->saveCount, initialSaveCount);
        QCOMPARE(jar->cookieCount(), initialCookieCount);
        QCOMPARE(cache->removed,
                 QList<QUrl>() << QUrl("https://u.y.qq.com/start"));
        QTest::qWait(10);
        QCOMPARE(completed.count() + failed.count(), 1);
    }

    void configuresBoundedDiskCache()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ScriptedNetworkAccessManager manager;
        QqNetworkTransport transport(&manager);

        QNetworkDiskCache *cache = transport.configureDiskCache(directory.path());
        QVERIFY(cache);
        QCOMPARE(manager.cache(), static_cast<QAbstractNetworkCache *>(cache));
        QCOMPARE(cache->maximumCacheSize(), qint64(32 * 1024 * 1024));
        QCOMPARE(QDir::cleanPath(cache->cacheDirectory()), directory.path());
        QCOMPARE(transport.diskCacheSize(), qint64(0));
        transport.setDiskCacheMaximumBytes(qint64(16) * 1024 * 1024);
        QCOMPARE(cache->maximumCacheSize(), qint64(16) * 1024 * 1024);
        transport.setDiskCacheMaximumBytes(qint64(64) * 1024 * 1024);
        QCOMPARE(cache->maximumCacheSize(), qint64(32) * 1024 * 1024);
        QVERIFY(transport.clearDiskCache());
    }

    void diagnosticDoesNotExposeSensitiveRequestData()
    {
        ScriptedNetworkAccessManager manager;
        ReplyScript script;
        script.status = 500;
        manager.scripts << script;
        QqNetworkTransport transport(&manager);
        QSignalSpy failed(&transport, SIGNAL(failed(ServiceError)));

        QVERIFY(transport.get(
            13,
            QUrl("https://u.y.qq.com/private/path?cookie=topsecret"),
            QqRequestKind::AuthenticatedAccount, true));
        QTRY_COMPARE(failed.count(), 1);
        const QString diagnostic = errorAt(failed).diagnosticCode;
        QVERIFY(diagnostic.contains("13"));
        QVERIFY(diagnostic.contains("u.y.qq.com"));
        QVERIFY(diagnostic.contains("500"));
        QVERIFY(!diagnostic.contains("private"));
        QVERIFY(!diagnostic.contains("cookie"));
        QVERIFY(!diagnostic.contains("topsecret"));
    }
};

QTEST_GUILESS_MAIN(QqNetworkTransportTest)
#include "tst_qqnetworktransport.moc"
