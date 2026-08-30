#include <QtTest>

#include "storage/imagecache.h"

#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QColor>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>

#include <cstring>

namespace {

QUrl artwork(const QString &suffix)
{
    return QUrl(QStringLiteral(
        "https://y.gtimg.cn/music/photo_new/%1.png").arg(suffix));
}

QByteArray png(int payloadBytes = 16)
{
    return QByteArray::fromHex("89504e470d0a1a0a")
        + QByteArray(payloadBytes, 'p');
}

QByteArray solidPng(const QColor &color)
{
    QImage image(32, 32, QImage::Format_ARGB32);
    image.fill(color);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        return QByteArray();
    }
    return bytes;
}

}

struct FetchScript
{
    QByteArray body;
    QByteArray mimeType = QByteArrayLiteral("image/png");
    int status = 200;
    qint64 contentLength = -1;
    QUrl redirect;
    QByteArray setCookie;
    bool autoDeliver = true;
};

class FetchReply : public QNetworkReply
{
    Q_OBJECT

public:
    FetchReply(const QNetworkRequest &request,
               QNetworkAccessManager::Operation operation,
               const FetchScript &script, QObject *parent)
        : QNetworkReply(parent)
        , m_script(script)
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(operation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, script.status);
        setHeader(QNetworkRequest::ContentTypeHeader, script.mimeType);
        setHeader(QNetworkRequest::ContentLengthHeader,
                  script.contentLength >= 0
                      ? script.contentLength : script.body.size());
        if (script.redirect.isValid()) {
            setAttribute(QNetworkRequest::RedirectionTargetAttribute,
                         script.redirect);
        }
        if (!script.setCookie.isEmpty()) {
            setRawHeader("Set-Cookie", script.setCookie);
        }
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        if (script.autoDeliver) {
            QTimer::singleShot(0, this, SLOT(deliver()));
        }
    }

    void abort() override
    {
        if (isFinished()) {
            return;
        }
        m_aborted = true;
        setError(QNetworkReply::OperationCanceledError,
                 QStringLiteral("cancelled"));
        finishNow();
    }

    qint64 bytesAvailable() const override
    {
        return m_available
            ? m_script.body.size() - m_offset + QIODevice::bytesAvailable()
            : QIODevice::bytesAvailable();
    }

    bool wasAborted() const
    {
        return m_aborted;
    }

    qint64 totalBytesRead() const
    {
        return m_totalBytesRead;
    }

protected:
    qint64 readData(char *data, qint64 maximum) override
    {
        if (!m_available || m_offset >= m_script.body.size()) {
            return -1;
        }
        const qint64 count = qMin(maximum,
                                  m_script.body.size() - m_offset);
        std::memcpy(data, m_script.body.constData() + m_offset,
                    size_t(count));
        m_offset += count;
        m_totalBytesRead += count;
        return count;
    }

public slots:
    void deliver()
    {
        if (isFinished()) {
            return;
        }
        emit metaDataChanged();
        if (isFinished()) {
            return;
        }
        m_available = true;
        emit readyRead();
        if (!isFinished()) {
            finishNow();
        }
    }

private:
    void finishNow()
    {
        setFinished(true);
        emit finished();
    }

    FetchScript m_script;
    qint64 m_offset = 0;
    qint64 m_totalBytesRead = 0;
    bool m_available = false;
    bool m_aborted = false;
};

class FetchNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT

public:
    QList<FetchScript> scripts;
    QList<QNetworkRequest> requests;
    QList<Operation> operations;
    QList<bool> aborted;
    QList<qint64> readCounts;
    QList<QPointer<FetchReply> > replies;
    int completed = 0;

protected:
    QNetworkReply *createRequest(Operation operation,
                                 const QNetworkRequest &request,
                                 QIODevice *) override
    {
        requests.append(request);
        operations.append(operation);
        FetchScript script;
        if (!scripts.isEmpty()) {
            script = scripts.takeFirst();
        }
        FetchReply *reply = new FetchReply(request, operation, script, this);
        replies.append(reply);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            aborted.append(reply->wasAborted());
            readCounts.append(reply->totalBytesRead());
            ++completed;
        });
        return reply;
    }
};

class ImageCacheTest : public QObject
{
    Q_OBJECT

private slots:
    void storesOnlyReviewedPublicImages()
    {
        QTemporaryDir directory;
        ImageCache cache(directory.path(), 4096);
        QSignalSpy available(&cache,
                             SIGNAL(artworkAvailable(QUrl,QUrl)));
        QVERIFY(cache.store(artwork(QStringLiteral("cover")),
                            png(), "image/png"));
        QTRY_COMPARE(available.count(), 1);
        const QUrl local = available.first().at(1).toUrl();
        QVERIFY(local.isLocalFile());
        QCOMPARE(cache.lookup(artwork(QStringLiteral("cover"))), local);
        QVERIFY(QFileInfo(local.toLocalFile()).isFile());
        QVERIFY(!cache.store(artwork(QStringLiteral("bad")),
                             QByteArray("not-an-image"), "image/png"));
        QVERIFY(!cache.store(QUrl(QStringLiteral(
                                 "https://evil.invalid/x.png")),
                             png(), "image/png"));
        QVERIFY(!cache.store(artwork(QStringLiteral("auth")),
                             png(), "image/png", true, false));
        QVERIFY(!cache.store(QUrl(QStringLiteral(
                                 "https://thirdqq.qlogo.cn/avatar.png")),
                             png(), "image/png", false, true));
    }

    void evictsLeastRecentlyUsedWithinBudget()
    {
        QTemporaryDir directory;
        ImageCache cache(directory.path(), 1000);
        QSignalSpy available(&cache,
                             SIGNAL(artworkAvailable(QUrl,QUrl)));
        const QUrl first = artwork(QStringLiteral("first"));
        const QUrl second = artwork(QStringLiteral("second"));
        const QUrl third = artwork(QStringLiteral("third"));
        QVERIFY(cache.store(first, png(160), "image/png"));
        QTRY_COMPARE(available.count(), 1);
        QTest::qWait(2);
        QVERIFY(cache.store(second, png(160), "image/png"));
        QTRY_COMPARE(available.count(), 2);
        QVERIFY(!cache.lookup(first).isEmpty());
        QTest::qWait(2);
        QVERIFY(cache.store(third, png(160), "image/png"));
        QTRY_COMPARE(available.count(), 3);
        QVERIFY(cache.usageBytes() <= cache.maximumBytes());
        QVERIFY(!cache.lookup(first).isEmpty());
        QVERIFY(cache.lookup(second).isEmpty());
        QVERIFY(!cache.lookup(third).isEmpty());
    }

    void recoversCorruptIndexAndKeepsOwnerOnlyModes()
    {
        QTemporaryDir directory;
        QString indexPath;
        {
            ImageCache cache(directory.path(), 4096);
            indexPath = cache.indexPath();
            QSignalSpy available(&cache,
                                 SIGNAL(artworkAvailable(QUrl,QUrl)));
            QVERIFY(cache.store(artwork(QStringLiteral("cover")),
                                png(), "image/png"));
            QTRY_COMPARE(available.count(), 1);
            const QUrl local = available.first().at(1).toUrl();
            QVERIFY(local.isLocalFile());
            const QFileDevice::Permissions permissions =
                QFileInfo(local.toLocalFile()).permissions();
            QVERIFY(!(permissions & (QFileDevice::ReadGroup
                                     | QFileDevice::WriteGroup
                                     | QFileDevice::ReadOther
                                     | QFileDevice::WriteOther)));
        }
        QFile index(indexPath);
        QVERIFY(index.open(QIODevice::WriteOnly | QIODevice::Truncate));
        index.write("corrupt");
        index.close();
        ImageCache recovered(directory.path(), 4096);
        QVERIFY(recovered.lookup(artwork(QStringLiteral("cover"))).isEmpty());
        QCOMPARE(recovered.usageBytes(), qint64(0));
        QVERIFY(recovered.clear());
        QCOMPARE(recovered.usageBytes(), qint64(0));
    }

    void fetchesAllowedArtworkAndCoalescesPendingRequests()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        FetchNetworkAccessManager manager;
        FetchScript script;
        script.body = png();
        manager.scripts << script;
        ImageCache cache(directory.path(), 4096, 0, &manager);
        QSignalSpy available(&cache,
                             SIGNAL(artworkAvailable(QUrl,QUrl)));
        QSignalSpy revisionChanged(&cache, SIGNAL(revisionChanged()));
        const QUrl remote = artwork(QStringLiteral("network-cover"));

        QVERIFY(cache.source(remote).isEmpty());
        QVERIFY(cache.source(remote).isEmpty());
        QCOMPARE(manager.requests.size(), 1);
        QCOMPARE(manager.operations.first(),
                 QNetworkAccessManager::GetOperation);
        QCOMPARE(manager.requests.first().attribute(
                     QNetworkRequest::CookieLoadControlAttribute).toInt(),
                 int(QNetworkRequest::Manual));
        QCOMPARE(manager.requests.first().attribute(
                     QNetworkRequest::CookieSaveControlAttribute).toInt(),
                 int(QNetworkRequest::Manual));
        QCOMPARE(manager.requests.first().rawHeader("Accept"),
                 QByteArray("image/png, image/jpeg, image/webp"));

        QTRY_COMPARE(available.count(), 1);
        QCOMPARE(revisionChanged.count(), 1);
        QCOMPARE(cache.revision(), 1);
        const QUrl local = available.first().at(1).toUrl();
        QVERIFY(local.isLocalFile());
        QVERIFY(QFileInfo(local.toLocalFile()).isFile());
        QCOMPARE(cache.source(remote), local);
        QCOMPARE(manager.requests.size(), 1);
    }

    void writesArtworkAsynchronouslyAndKeepsEventLoopResponsive()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ImageCache cache(directory.path(), 4 * 1024 * 1024);
        QSignalSpy available(&cache,
                             SIGNAL(artworkAvailable(QUrl,QUrl)));
        bool eventLoopAdvanced = false;
        QTimer eventLoopProbe;
        eventLoopProbe.setSingleShot(true);
        connect(&eventLoopProbe, &QTimer::timeout, this, [&eventLoopAdvanced]() {
            eventLoopAdvanced = true;
        });
        eventLoopProbe.start(0);

        QVERIFY(cache.store(artwork(QStringLiteral("large-cover")),
                            png(1024 * 1024), "image/png"));
        QCOMPARE(available.count(), 0);
        QTRY_VERIFY(eventLoopAdvanced);
        QTRY_COMPARE(available.count(), 1);
        QVERIFY(available.first().at(1).toUrl().isLocalFile());
    }

    void analysesArtworkContrastAsynchronously()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ImageCache cache(directory.path(), 4096);
        QSignalSpy available(&cache,
                             SIGNAL(artworkAvailable(QUrl,QUrl)));
        QSignalSpy foreground(&cache,
                              SIGNAL(artworkForegroundAvailable(QUrl,bool)));
        const QUrl bright = artwork(QStringLiteral("bright-cover"));
        const QUrl dark = artwork(QStringLiteral("dark-cover"));

        QVERIFY(cache.store(bright, solidPng(Qt::white), "image/png"));
        QTRY_COMPARE(available.count(), 1);
        bool eventLoopAdvanced = false;
        QTimer::singleShot(0, this, [&eventLoopAdvanced]() {
            eventLoopAdvanced = true;
        });
        QVERIFY(cache.requestForeground(bright));
        QCOMPARE(foreground.count(), 0);
        QTRY_VERIFY(eventLoopAdvanced);
        QTRY_COMPARE(foreground.count(), 1);
        QCOMPARE(foreground.at(0).at(0).toUrl(), bright);
        QCOMPARE(foreground.at(0).at(1).toBool(), false);

        QVERIFY(cache.store(dark, solidPng(Qt::black), "image/png"));
        QTRY_COMPARE(available.count(), 2);
        QVERIFY(cache.requestForeground(dark));
        QTRY_COMPARE(foreground.count(), 2);
        QCOMPARE(foreground.at(1).at(0).toUrl(), dark);
        QCOMPARE(foreground.at(1).at(1).toBool(), true);

        QVERIFY(cache.requestForeground(bright));
        QCOMPARE(foreground.count(), 3);
        QCOMPARE(foreground.at(2).at(1).toBool(), false);
    }

    void boundsConcurrentFetchesAndKeepsEventLoopResponsive()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        FetchNetworkAccessManager manager;
        FetchScript script;
        script.body = png();
        script.autoDeliver = false;
        for (int index = 0; index < 6; ++index) {
            manager.scripts << script;
        }
        ImageCache cache(directory.path(), 4096, 0, &manager);

        bool eventLoopAdvanced = false;
        QTimer eventLoopProbe;
        eventLoopProbe.setSingleShot(true);
        connect(&eventLoopProbe, &QTimer::timeout, this, [&eventLoopAdvanced]() {
            eventLoopAdvanced = true;
        });
        eventLoopProbe.start(0);

        for (int index = 0; index < 6; ++index) {
            QVERIFY(cache.source(artwork(QStringLiteral("queued-%1")
                                         .arg(index))).isEmpty());
        }
        QCOMPARE(manager.requests.size(), 4);
        QTRY_VERIFY(eventLoopAdvanced);

        QVERIFY(manager.replies.first());
        manager.replies.first()->deliver();
        QTRY_COMPARE(manager.completed, 1);
        QTRY_COMPARE(manager.requests.size(), 5);
    }

    void rejectsUnsafeNetworkResponses_data()
    {
        QTest::addColumn<int>("failure");
        QTest::newRow("redirect") << 0;
        QTest::newRow("oversized-content-length") << 1;
        QTest::newRow("invalid-mime") << 2;
        QTest::newRow("invalid-magic") << 3;
        QTest::newRow("set-cookie") << 4;
    }

    void rejectsUnsafeNetworkResponses()
    {
        QFETCH(int, failure);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        FetchNetworkAccessManager manager;
        FetchScript script;
        script.body = png();
        if (failure == 0) {
            script.redirect = QUrl(QStringLiteral(
                "https://y.gtimg.cn/music/photo_new/redirect.png"));
        } else if (failure == 1) {
            script.contentLength = qint64(16) * 1024 * 1024 + 1;
        } else if (failure == 2) {
            script.mimeType = QByteArrayLiteral("text/html");
        } else if (failure == 3) {
            script.body = QByteArrayLiteral("not-an-image");
        } else {
            script.setCookie = QByteArrayLiteral("uin=secret; Secure");
        }
        manager.scripts << script;
        ImageCache cache(directory.path(), 4096, 0, &manager);
        QSignalSpy available(&cache,
                             SIGNAL(artworkAvailable(QUrl,QUrl)));
        const QUrl remote = artwork(QStringLiteral("unsafe-%1").arg(failure));

        QVERIFY(cache.source(remote).isEmpty());
        QTRY_COMPARE(manager.completed, 1);
        QCOMPARE(available.count(), 0);
        QVERIFY(cache.lookup(remote).isEmpty());
        QCOMPARE(cache.revision(), 0);
        if (failure == 1) {
            QCOMPARE(manager.aborted.first(), true);
            QCOMPARE(manager.readCounts.first(), qint64(0));
        }
    }
};

QTEST_GUILESS_MAIN(ImageCacheTest)
#include "tst_imagecache.moc"
