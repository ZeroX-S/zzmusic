#ifndef QQMUSIC_IMAGECACHE_H
#define QQMUSIC_IMAGECACHE_H

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QSet>
#include <QStringList>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class QTimer;

class ImageCache : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qint64 usageBytes READ usageBytes NOTIFY usageChanged)
    Q_PROPERTY(qint64 maximumBytes READ maximumBytes WRITE setMaximumBytes
               NOTIFY maximumBytesChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit ImageCache(const QString &directory = QString(),
                        qint64 maximumBytes = -1,
                        QObject *parent = 0,
                        QNetworkAccessManager *networkManager = 0);
    ~ImageCache() override;

    qint64 usageBytes() const;
    qint64 maximumBytes() const;
    void setMaximumBytes(qint64 bytes);
    int revision() const;
    QString directory() const;
    QString indexPath() const;

    Q_INVOKABLE QUrl source(const QUrl &url);
    Q_INVOKABLE QUrl lookup(const QUrl &url);
    Q_INVOKABLE bool requestForeground(const QUrl &url);
    bool store(const QUrl &url, const QByteArray &bytes,
               const QByteArray &mimeType,
               bool authenticated = false,
               bool avatar = false);
    Q_INVOKABLE bool clear();

signals:
    void usageChanged();
    void maximumBytesChanged();
    void revisionChanged();
    void cacheReset();
    void artworkAvailable(const QUrl &remoteUrl, const QUrl &localUrl);
    void artworkForegroundAvailable(const QUrl &remoteUrl, bool lightText);
    void storeImageRequested(const QUrl &remoteUrl,
                             const QByteArray &canonicalUrl,
                             const QString &key,
                             const QString &fileName,
                             const QString &path,
                             const QByteArray &bytes,
                             const QByteArray &mimeType,
                             quint64 epoch);
    void persistIndexRequested(const QString &directory,
                               const QByteArray &indexBytes,
                               const QStringList &removals);
    void analyzeImageRequested(const QUrl &remoteUrl,
                               const QByteArray &canonicalUrl,
                               const QString &path,
                               quint64 epoch);

private slots:
    void asyncStoreFinished(const QUrl &remoteUrl,
                            const QByteArray &canonicalUrl,
                            const QString &key,
                            const QString &fileName,
                            const QByteArray &mimeType,
                            qint64 size,
                            quint64 epoch,
                            bool success);
    void flushIndexUpdate();
    void asyncAnalysisFinished(const QUrl &remoteUrl,
                               const QByteArray &canonicalUrl,
                               quint64 epoch,
                               bool success,
                               bool lightText);

private:
    struct Entry {
        QString key;
        QString fileName;
        QByteArray canonicalUrl;
        QByteArray mimeType;
        qint64 size = 0;
        qint64 lastAccess = 0;
    };

    struct PendingFetch {
        QUrl url;
        QByteArray canonicalUrl;
        QByteArray bytes;
        bool rejected = false;
    };

    static QByteArray canonicalArtworkUrl(const QUrl &url);
    static QString extensionFor(const QByteArray &bytes,
                                const QByteArray &mimeType);
    bool ensurePrivateDirectory() const;
    bool loadIndex();
    QByteArray serializedIndex() const;
    void recoverCorruptIndex();
    void evictToBudgetAsync(const QStringList &additionalRemovals = QStringList());
    void scheduleIndexUpdate(const QStringList &removals = QStringList());
    qint64 imageBytes() const;
    void scheduleFetch(const QUrl &url, const QByteArray &canonicalUrl);
    void startQueuedFetches();
    void inspectReplyMetadata(QNetworkReply *reply);
    void readReply(QNetworkReply *reply);
    void finishReply(QNetworkReply *reply);
    void advanceRevision();

    QString m_directory;
    qint64 m_maximumBytes;
    int m_revision;
    QHash<QString, Entry> m_entries;
    QPointer<QNetworkAccessManager> m_networkManager;
    QThread *m_diskThread;
    QObject *m_diskWorker;
    QTimer *m_indexTimer;
    QStringList m_pendingRemovals;
    quint64 m_storeEpoch;
    QQueue<QUrl> m_fetchQueue;
    QSet<QByteArray> m_queuedUrls;
    QSet<QByteArray> m_pendingUrls;
    QSet<QByteArray> m_pendingStores;
    QHash<QByteArray, qint64> m_retryAfter;
    QHash<QNetworkReply *, PendingFetch> m_pendingFetches;
    QHash<QByteArray, bool> m_lightTextResults;
    QSet<QByteArray> m_pendingAnalyses;
};

#endif
