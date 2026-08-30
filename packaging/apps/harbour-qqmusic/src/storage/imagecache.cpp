#include "imagecache.h"

#include "service/qqrequestpolicy.h"
#include "storage/settingsstore.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegExp>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QVariant>

#include <limits>

namespace {

const int IndexVersion = 1;
const qint64 MaximumImageBytes = 16 * 1024 * 1024;
const int MaximumConcurrentFetches = 4;
const int MaximumQueuedFetches = 128;
const int FetchTimeoutMs = 15000;
const int RetryDelayMs = 30000;

bool exactEntryKeys(const QJsonObject &object)
{
    const QStringList keys = QStringList()
        << QStringLiteral("key") << QStringLiteral("file")
        << QStringLiteral("url") << QStringLiteral("mime")
        << QStringLiteral("size") << QStringLiteral("lastAccess");
    if (object.size() != keys.size()) {
        return false;
    }
    for (const QString &key : keys) {
        if (!object.contains(key)) {
            return false;
        }
    }
    return true;
}

}

class ImageCacheDiskWorker : public QObject
{
    Q_OBJECT

public slots:
    void storeImage(const QUrl &remoteUrl,
                    const QByteArray &canonicalUrl,
                    const QString &key,
                    const QString &fileName,
                    const QString &path,
                    const QByteArray &bytes,
                    const QByteArray &mimeType,
                    quint64 epoch)
    {
        const QString directory = QFileInfo(path).absolutePath();
        const bool directoryReady = QDir().mkpath(directory)
            && QFile::setPermissions(directory,
                QFileDevice::ReadOwner | QFileDevice::WriteOwner
                | QFileDevice::ExeOwner);
        bool success = false;
        if (directoryReady) {
            QSaveFile file(path);
            success = file.open(QIODevice::WriteOnly)
                && file.setPermissions(QFileDevice::ReadOwner
                                       | QFileDevice::WriteOwner)
                && file.write(bytes) == bytes.size()
                && file.commit()
                && QFile::setPermissions(path, QFileDevice::ReadOwner
                                                | QFileDevice::WriteOwner);
        }
        emit imageStored(remoteUrl, canonicalUrl, key, fileName, mimeType,
                         bytes.size(), epoch, success);
    }

    void persistIndex(const QString &directory,
                      const QByteArray &indexBytes,
                      const QStringList &removals)
    {
        const QString absoluteDirectory = QDir(directory).absolutePath();
        if (!QDir().mkpath(absoluteDirectory)
                || !QFile::setPermissions(absoluteDirectory,
                    QFileDevice::ReadOwner | QFileDevice::WriteOwner
                    | QFileDevice::ExeOwner)) {
            return;
        }
        for (const QString &path : removals) {
            const QFileInfo info(path);
            if (info.absolutePath() == absoluteDirectory) {
                QFile::remove(info.absoluteFilePath());
            }
        }
        const QString indexPath = QDir(absoluteDirectory).filePath(
            QStringLiteral("index-v1.json"));
        if (indexBytes.isEmpty()) {
            QFile::remove(indexPath);
            return;
        }
        QSaveFile file(indexPath);
        if (file.open(QIODevice::WriteOnly)
                && file.setPermissions(QFileDevice::ReadOwner
                                       | QFileDevice::WriteOwner)
                && file.write(indexBytes) == indexBytes.size()
                && file.commit()) {
            QFile::setPermissions(indexPath, QFileDevice::ReadOwner
                                             | QFileDevice::WriteOwner);
        }
    }

    void analyzeImage(const QUrl &remoteUrl,
                      const QByteArray &canonicalUrl,
                      const QString &path,
                      quint64 epoch)
    {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QSize targetSize = reader.size();
        if (targetSize.isValid()) {
            targetSize.scale(QSize(32, 32), Qt::KeepAspectRatio);
            reader.setScaledSize(targetSize);
        }
        const QImage image = reader.read().convertToFormat(
            QImage::Format_ARGB32);
        quint64 luminanceTotal = 0;
        quint64 alphaTotal = 0;
        if (!image.isNull()) {
            const int firstRow = qBound(0, image.height() * 40 / 100,
                                        image.height() - 1);
            const int lastRow = qBound(firstRow + 1,
                                       image.height() * 88 / 100,
                                       image.height());
            for (int y = firstRow; y < lastRow; ++y) {
                const QRgb *line = reinterpret_cast<const QRgb *>(
                    image.constScanLine(y));
                for (int x = 0; x < image.width(); ++x) {
                    const int alpha = qAlpha(line[x]);
                    if (alpha < 16) {
                        continue;
                    }
                    const int luminance = (299 * qRed(line[x])
                        + 587 * qGreen(line[x])
                        + 114 * qBlue(line[x])) / 1000;
                    luminanceTotal += quint64(luminance * alpha);
                    alphaTotal += quint64(alpha);
                }
            }
        }
        const bool success = alphaTotal > 0;
        const int averageLuminance = success
            ? int(luminanceTotal / alphaTotal) : 0;
        emit imageAnalyzed(remoteUrl, canonicalUrl, epoch, success,
                           averageLuminance < 150);
    }

signals:
    void imageStored(const QUrl &remoteUrl,
                     const QByteArray &canonicalUrl,
                     const QString &key,
                     const QString &fileName,
                     const QByteArray &mimeType,
                     qint64 size,
                     quint64 epoch,
                     bool success);
    void imageAnalyzed(const QUrl &remoteUrl,
                       const QByteArray &canonicalUrl,
                       quint64 epoch,
                       bool success,
                       bool lightText);
};

ImageCache::ImageCache(const QString &directory, qint64 maximumBytes,
                       QObject *parent, QNetworkAccessManager *networkManager)
    : QObject(parent)
    , m_directory(directory.isEmpty()
          ? QDir(QStandardPaths::writableLocation(
                    QStandardPaths::CacheLocation))
                .filePath(QStringLiteral("harbour-qqmusic/artwork"))
          : directory)
    , m_maximumBytes(maximumBytes > 0
          ? qMin(maximumBytes, SettingsStore::defaultImageCacheBytes())
          : SettingsStore::defaultImageCacheBytes())
    , m_revision(0)
    , m_networkManager(networkManager
          ? networkManager : new QNetworkAccessManager(this))
    , m_diskThread(new QThread(this))
    , m_diskWorker(new ImageCacheDiskWorker)
    , m_indexTimer(new QTimer(this))
    , m_storeEpoch(1)
{
    ImageCacheDiskWorker *worker =
        static_cast<ImageCacheDiskWorker *>(m_diskWorker);
    worker->moveToThread(m_diskThread);
    connect(m_diskThread, &QThread::finished,
            worker, &QObject::deleteLater);
    connect(this, &ImageCache::storeImageRequested,
            worker, &ImageCacheDiskWorker::storeImage);
    connect(this, &ImageCache::persistIndexRequested,
            worker, &ImageCacheDiskWorker::persistIndex);
    connect(this, &ImageCache::analyzeImageRequested,
            worker, &ImageCacheDiskWorker::analyzeImage);
    connect(worker, &ImageCacheDiskWorker::imageStored,
            this, &ImageCache::asyncStoreFinished);
    connect(worker, &ImageCacheDiskWorker::imageAnalyzed,
            this, &ImageCache::asyncAnalysisFinished);
    m_diskThread->start();
    m_indexTimer->setSingleShot(true);
    m_indexTimer->setInterval(250);
    connect(m_indexTimer, &QTimer::timeout,
            this, &ImageCache::flushIndexUpdate);
    connect(m_networkManager.data(), &QObject::destroyed, this, [this]() {
        m_pendingFetches.clear();
        m_pendingUrls.clear();
        m_fetchQueue.clear();
        m_queuedUrls.clear();
    });
    ensurePrivateDirectory();
    loadIndex();
    evictToBudgetAsync();
}

ImageCache::~ImageCache()
{
    ++m_storeEpoch;
    m_indexTimer->stop();
    const QByteArray indexBytes = serializedIndex();
    const QStringList removals = m_pendingRemovals;
    m_pendingRemovals.clear();
    QMetaObject::invokeMethod(
        m_diskWorker, "persistIndex", Qt::BlockingQueuedConnection,
        Q_ARG(QString, m_directory), Q_ARG(QByteArray, indexBytes),
        Q_ARG(QStringList, removals));
    m_diskThread->quit();
    m_diskThread->wait();
}

int ImageCache::revision() const
{
    return m_revision;
}

qint64 ImageCache::usageBytes() const
{
    return imageBytes() + serializedIndex().size();
}

qint64 ImageCache::maximumBytes() const
{
    return m_maximumBytes;
}

void ImageCache::setMaximumBytes(qint64 bytes)
{
    const qint64 bounded = qBound<qint64>(1, bytes,
        SettingsStore::defaultImageCacheBytes());
    if (bounded == m_maximumBytes) {
        return;
    }
    m_maximumBytes = bounded;
    evictToBudgetAsync();
    emit maximumBytesChanged();
}

QString ImageCache::directory() const
{
    return m_directory;
}

QString ImageCache::indexPath() const
{
    return QDir(m_directory).filePath(QStringLiteral("index-v1.json"));
}

QUrl ImageCache::source(const QUrl &url)
{
    const QUrl localUrl = lookup(url);
    if (localUrl.isValid()) {
        return localUrl;
    }
    const QByteArray canonical = canonicalArtworkUrl(url);
    if (!canonical.isEmpty()) {
        scheduleFetch(url, canonical);
    }
    return QUrl();
}

QUrl ImageCache::lookup(const QUrl &url)
{
    const QByteArray canonical = canonicalArtworkUrl(url);
    if (canonical.isEmpty()) {
        return QUrl();
    }
    const QString key = QString::fromLatin1(
        QCryptographicHash::hash(canonical, QCryptographicHash::Sha256).toHex());
    if (!m_entries.contains(key)) {
        return QUrl();
    }
    Entry &entry = m_entries[key];
    const QString path = QDir(m_directory).filePath(entry.fileName);
    if (!QFileInfo(path).isFile()) {
        m_pendingAnalyses.remove(canonical);
        m_lightTextResults.remove(canonical);
        m_entries.remove(key);
        scheduleIndexUpdate();
        emit usageChanged();
        return QUrl();
    }
    entry.lastAccess = QDateTime::currentMSecsSinceEpoch();
    scheduleIndexUpdate();
    return QUrl::fromLocalFile(path);
}

bool ImageCache::requestForeground(const QUrl &url)
{
    const QByteArray canonical = canonicalArtworkUrl(url);
    if (canonical.isEmpty()) {
        return false;
    }
    if (m_lightTextResults.contains(canonical)) {
        emit artworkForegroundAvailable(url,
                                        m_lightTextResults.value(canonical));
        return true;
    }
    if (m_pendingAnalyses.contains(canonical)) {
        return true;
    }
    const QString key = QString::fromLatin1(
        QCryptographicHash::hash(canonical, QCryptographicHash::Sha256).toHex());
    if (!m_entries.contains(key)) {
        return false;
    }
    const QString path = QDir(m_directory).filePath(
        m_entries.value(key).fileName);
    if (!QFileInfo(path).isFile()) {
        return false;
    }
    m_pendingAnalyses.insert(canonical);
    emit analyzeImageRequested(url, canonical, path, m_storeEpoch);
    return true;
}

bool ImageCache::store(const QUrl &url, const QByteArray &bytes,
                       const QByteArray &mimeType,
                       bool authenticated, bool avatar)
{
    const QByteArray canonical = canonicalArtworkUrl(url);
    const QString extension = extensionFor(bytes, mimeType.toLower());
    if (authenticated || avatar || canonical.isEmpty() || extension.isEmpty()
            || bytes.isEmpty() || bytes.size() > MaximumImageBytes
            || m_pendingStores.contains(canonical)) {
        return false;
    }
    const QString key = QString::fromLatin1(
        QCryptographicHash::hash(canonical, QCryptographicHash::Sha256).toHex());
    const QString fileName = key + QLatin1Char('.') + extension;
    const QString path = QDir(m_directory).filePath(fileName);
    m_pendingStores.insert(canonical);
    emit storeImageRequested(url, canonical, key, fileName, path, bytes,
                             mimeType.toLower(), m_storeEpoch);
    return true;
}

bool ImageCache::clear()
{
    const QList<QNetworkReply *> replies = m_pendingFetches.keys();
    for (QNetworkReply *reply : replies) {
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    m_pendingFetches.clear();
    m_pendingUrls.clear();
    m_fetchQueue.clear();
    m_queuedUrls.clear();
    ++m_storeEpoch;
    m_pendingStores.clear();
    m_pendingAnalyses.clear();
    m_lightTextResults.clear();
    m_retryAfter.clear();
    QStringList removals;
    for (const Entry &entry : m_entries) {
        removals.append(QDir(m_directory).filePath(entry.fileName));
    }
    m_entries.clear();
    scheduleIndexUpdate(removals);
    emit usageChanged();
    advanceRevision();
    emit cacheReset();
    return true;
}

QByteArray ImageCache::canonicalArtworkUrl(const QUrl &url)
{
    if (!QqRequestPolicy::allowsArtwork(url)) {
        return QByteArray();
    }
    return url.adjusted(QUrl::RemoveFragment).toEncoded(QUrl::FullyEncoded);
}

QString ImageCache::extensionFor(const QByteArray &bytes,
                                 const QByteArray &mimeType)
{
    if (mimeType == QByteArrayLiteral("image/png")
            && bytes.startsWith(QByteArray::fromHex("89504e470d0a1a0a"))) {
        return QStringLiteral("png");
    }
    if (mimeType == QByteArrayLiteral("image/jpeg")
            && bytes.size() >= 3
            && uchar(bytes.at(0)) == 0xff
            && uchar(bytes.at(1)) == 0xd8
            && uchar(bytes.at(2)) == 0xff) {
        return QStringLiteral("jpg");
    }
    if (mimeType == QByteArrayLiteral("image/webp")
            && bytes.size() >= 12 && bytes.startsWith("RIFF")
            && bytes.mid(8, 4) == QByteArrayLiteral("WEBP")) {
        return QStringLiteral("webp");
    }
    return QString();
}

bool ImageCache::ensurePrivateDirectory() const
{
    return QDir().mkpath(m_directory)
        && QFile::setPermissions(m_directory,
            QFileDevice::ReadOwner | QFileDevice::WriteOwner
            | QFileDevice::ExeOwner);
}

bool ImageCache::loadIndex()
{
    m_entries.clear();
    QFile file(indexPath());
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) {
        recoverCorruptIndex();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        recoverCorruptIndex();
        return false;
    }
    const QJsonObject root = document.object();
    if (root.size() != 2
            || root.value(QStringLiteral("version")).toInt(-1) != IndexVersion
            || !root.value(QStringLiteral("entries")).isArray()) {
        recoverCorruptIndex();
        return false;
    }
    const QRegExp safeFile(QStringLiteral(
        "^[0-9a-f]{64}\\.(png|jpg|webp)$"));
    const QJsonArray entries = root.value(QStringLiteral("entries")).toArray();
    for (const QJsonValue &value : entries) {
        if (!value.isObject() || !exactEntryKeys(value.toObject())) {
            recoverCorruptIndex();
            return false;
        }
        const QJsonObject object = value.toObject();
        Entry entry;
        entry.key = object.value(QStringLiteral("key")).toString();
        entry.fileName = object.value(QStringLiteral("file")).toString();
        entry.canonicalUrl = QByteArray::fromBase64(
            object.value(QStringLiteral("url")).toString().toLatin1());
        entry.mimeType = object.value(QStringLiteral("mime")).toString().toLatin1();
        entry.size = qint64(object.value(QStringLiteral("size")).toDouble(-1));
        entry.lastAccess = qint64(
            object.value(QStringLiteral("lastAccess")).toDouble(-1));
        const QString path = QDir(m_directory).filePath(entry.fileName);
        if (entry.key.size() != 64 || safeFile.exactMatch(entry.fileName) == false
                || !entry.fileName.startsWith(entry.key)
                || entry.canonicalUrl.isEmpty()
                || entry.size <= 0 || entry.lastAccess < 0
                || QFileInfo(path).size() != entry.size
                || m_entries.contains(entry.key)) {
            recoverCorruptIndex();
            return false;
        }
        m_entries.insert(entry.key, entry);
    }
    QStringList referencedFiles;
    for (const Entry &entry : m_entries) {
        referencedFiles.append(entry.fileName);
    }
    QStringList orphanedFiles;
    const QStringList files = QDir(m_directory).entryList(
        QStringList() << QStringLiteral("*.png") << QStringLiteral("*.jpg")
                      << QStringLiteral("*.webp"),
        QDir::Files | QDir::NoDotAndDotDot);
    for (const QString &fileName : files) {
        if (safeFile.exactMatch(fileName)
                && !referencedFiles.contains(fileName)) {
            orphanedFiles.append(QDir(m_directory).filePath(fileName));
        }
    }
    if (!orphanedFiles.isEmpty()) {
        scheduleIndexUpdate(orphanedFiles);
    }
    return true;
}

QByteArray ImageCache::serializedIndex() const
{
    if (m_entries.isEmpty()) {
        return QByteArray();
    }
    QJsonArray entries;
    QStringList keys = m_entries.keys();
    keys.sort();
    for (const QString &key : keys) {
        const Entry &entry = m_entries.value(key);
        QJsonObject object;
        object.insert(QStringLiteral("key"), entry.key);
        object.insert(QStringLiteral("file"), entry.fileName);
        object.insert(QStringLiteral("url"), QString::fromLatin1(
            entry.canonicalUrl.toBase64()));
        object.insert(QStringLiteral("mime"),
                      QString::fromLatin1(entry.mimeType));
        object.insert(QStringLiteral("size"), double(entry.size));
        object.insert(QStringLiteral("lastAccess"), double(entry.lastAccess));
        entries.append(object);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), IndexVersion);
    root.insert(QStringLiteral("entries"), entries);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

void ImageCache::recoverCorruptIndex()
{
    m_entries.clear();
    const QRegExp safeFile(QStringLiteral(
        "^[0-9a-f]{64}\\.(png|jpg|webp)$"));
    const QStringList files = QDir(m_directory).entryList(
        QDir::Files | QDir::NoDotAndDotDot);
    QStringList removals;
    for (const QString &fileName : files) {
        if (safeFile.exactMatch(fileName)
                || fileName == QStringLiteral("index-v1.json")) {
            removals.append(QDir(m_directory).filePath(fileName));
        }
    }
    scheduleIndexUpdate(removals);
}

void ImageCache::evictToBudgetAsync(const QStringList &additionalRemovals)
{
    QStringList removals = additionalRemovals;
    while (!m_entries.isEmpty() && usageBytes() > m_maximumBytes) {
        QString oldestKey;
        qint64 oldestAccess = std::numeric_limits<qint64>::max();
        for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
            if (it.value().lastAccess < oldestAccess
                    || (it.value().lastAccess == oldestAccess
                        && (oldestKey.isEmpty() || it.key() < oldestKey))) {
                oldestKey = it.key();
                oldestAccess = it.value().lastAccess;
            }
        }
        const Entry entry = m_entries.take(oldestKey);
        m_pendingAnalyses.remove(entry.canonicalUrl);
        m_lightTextResults.remove(entry.canonicalUrl);
        removals.append(QDir(m_directory).filePath(entry.fileName));
    }
    scheduleIndexUpdate(removals);
    emit usageChanged();
}

void ImageCache::scheduleIndexUpdate(const QStringList &removals)
{
    for (const QString &path : removals) {
        if (!m_pendingRemovals.contains(path)) {
            m_pendingRemovals.append(path);
        }
    }
    m_indexTimer->start();
}

void ImageCache::flushIndexUpdate()
{
    const QStringList removals = m_pendingRemovals;
    m_pendingRemovals.clear();
    emit persistIndexRequested(m_directory, serializedIndex(), removals);
}

qint64 ImageCache::imageBytes() const
{
    qint64 total = 0;
    for (const Entry &entry : m_entries) {
        total += entry.size;
    }
    return total;
}

void ImageCache::asyncStoreFinished(const QUrl &remoteUrl,
                                    const QByteArray &canonicalUrl,
                                    const QString &key,
                                    const QString &fileName,
                                    const QByteArray &mimeType,
                                    qint64 size,
                                    quint64 epoch,
                                    bool success)
{
    m_pendingStores.remove(canonicalUrl);
    const QString path = QDir(m_directory).filePath(fileName);
    if (epoch != m_storeEpoch) {
        if (!m_pendingStores.contains(canonicalUrl)
                && !m_entries.contains(key)) {
            scheduleIndexUpdate(QStringList() << path);
        }
        return;
    }
    if (!success) {
        m_retryAfter.insert(canonicalUrl,
                            QDateTime::currentMSecsSinceEpoch()
                                + RetryDelayMs);
        return;
    }

    QStringList removals;
    if (m_entries.contains(key)
            && m_entries.value(key).fileName != fileName) {
        removals.append(QDir(m_directory).filePath(
            m_entries.value(key).fileName));
    }
    Entry entry;
    entry.key = key;
    entry.fileName = fileName;
    entry.canonicalUrl = canonicalUrl;
    entry.mimeType = mimeType;
    entry.size = size;
    entry.lastAccess = QDateTime::currentMSecsSinceEpoch();
    m_entries.insert(key, entry);
    m_retryAfter.remove(canonicalUrl);
    evictToBudgetAsync(removals);
    if (!m_entries.contains(key)) {
        return;
    }
    advanceRevision();
    emit artworkAvailable(remoteUrl, QUrl::fromLocalFile(path));
}

void ImageCache::asyncAnalysisFinished(const QUrl &remoteUrl,
                                       const QByteArray &canonicalUrl,
                                       quint64 epoch,
                                       bool success,
                                       bool lightText)
{
    m_pendingAnalyses.remove(canonicalUrl);
    if (epoch != m_storeEpoch || !success) {
        return;
    }
    const QString key = QString::fromLatin1(
        QCryptographicHash::hash(canonicalUrl,
                                 QCryptographicHash::Sha256).toHex());
    if (!m_entries.contains(key)
            || m_entries.value(key).canonicalUrl != canonicalUrl) {
        return;
    }
    m_lightTextResults.insert(canonicalUrl, lightText);
    emit artworkForegroundAvailable(remoteUrl, lightText);
}

void ImageCache::scheduleFetch(const QUrl &url,
                               const QByteArray &canonicalUrl)
{
    if (!m_networkManager || m_pendingUrls.contains(canonicalUrl)
            || m_pendingStores.contains(canonicalUrl)
            || m_queuedUrls.contains(canonicalUrl)
            || m_fetchQueue.size() >= MaximumQueuedFetches) {
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_retryAfter.value(canonicalUrl, 0) > now) {
        return;
    }
    m_retryAfter.remove(canonicalUrl);
    m_fetchQueue.enqueue(url);
    m_queuedUrls.insert(canonicalUrl);
    startQueuedFetches();
}

void ImageCache::startQueuedFetches()
{
    while (m_networkManager
            && m_pendingFetches.size() < MaximumConcurrentFetches
            && !m_fetchQueue.isEmpty()) {
        const QUrl url = m_fetchQueue.dequeue();
        const QByteArray canonical = canonicalArtworkUrl(url);
        m_queuedUrls.remove(canonical);
        if (canonical.isEmpty() || m_pendingUrls.contains(canonical)) {
            continue;
        }
        QNetworkRequest request = QqRequestPolicy::prepareRequest(
            QqRequestKind::Artwork, url, true, false);
        request.setRawHeader("Accept", "image/png, image/jpeg, image/webp");
        QNetworkReply *reply = m_networkManager->get(request);
        if (!reply) {
            m_retryAfter.insert(canonical,
                                QDateTime::currentMSecsSinceEpoch()
                                    + RetryDelayMs);
            continue;
        }
        PendingFetch pending;
        pending.url = url;
        pending.canonicalUrl = canonical;
        m_pendingFetches.insert(reply, pending);
        m_pendingUrls.insert(canonical);
        connect(reply, &QIODevice::readyRead, this, [this, reply]() {
            readReply(reply);
        });
        connect(reply, &QNetworkReply::metaDataChanged,
                this, [this, reply]() {
            inspectReplyMetadata(reply);
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            finishReply(reply);
        });
        QTimer *timer = new QTimer(reply);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, reply, [this, reply]() {
            if (m_pendingFetches.contains(reply)) {
                m_pendingFetches[reply].rejected = true;
                reply->abort();
            }
        });
        timer->start(FetchTimeoutMs);
    }
}

void ImageCache::inspectReplyMetadata(QNetworkReply *reply)
{
    if (!reply || !m_pendingFetches.contains(reply)) {
        return;
    }
    const QVariant lengthHeader = reply->header(
        QNetworkRequest::ContentLengthHeader);
    if (!lengthHeader.isValid()) {
        return;
    }
    bool ok = false;
    const qint64 declaredSize = lengthHeader.toLongLong(&ok);
    if (!ok || declaredSize < 0 || declaredSize > MaximumImageBytes) {
        m_pendingFetches[reply].rejected = true;
        reply->abort();
    }
}

void ImageCache::readReply(QNetworkReply *reply)
{
    if (!reply || !m_pendingFetches.contains(reply)) {
        return;
    }
    PendingFetch &pending = m_pendingFetches[reply];
    while (reply->bytesAvailable() > 0 && !pending.rejected) {
        const qint64 remaining = MaximumImageBytes - pending.bytes.size();
        const QByteArray chunk = reply->read(qMin<qint64>(65536,
                                                          remaining + 1));
        if (chunk.isEmpty()) {
            break;
        }
        pending.bytes.append(chunk);
        if (pending.bytes.size() > MaximumImageBytes) {
            pending.rejected = true;
            reply->abort();
        }
    }
}

void ImageCache::finishReply(QNetworkReply *reply)
{
    if (!reply || !m_pendingFetches.contains(reply)) {
        return;
    }
    readReply(reply);
    const PendingFetch pending = m_pendingFetches.take(reply);
    m_pendingUrls.remove(pending.canonicalUrl);

    const int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool redirected = reply->attribute(
        QNetworkRequest::RedirectionTargetAttribute).isValid();
    const QVariant lengthHeader = reply->header(
        QNetworkRequest::ContentLengthHeader);
    bool declaredSizeOk = true;
    qint64 declaredSize = 0;
    if (lengthHeader.isValid()) {
        declaredSize = lengthHeader.toLongLong(&declaredSizeOk);
        declaredSizeOk = declaredSizeOk && declaredSize >= 0;
    }
    const QByteArray mimeType = reply->header(
        QNetworkRequest::ContentTypeHeader).toByteArray()
            .split(';').value(0).trimmed().toLower();
    const bool accepted = !pending.rejected
        && reply->error() == QNetworkReply::NoError
        && status == 200
        && !redirected
        && declaredSizeOk
        && declaredSize <= MaximumImageBytes
        && QqRequestPolicy::allowsResponseCache(reply->rawHeaderPairs());
    const bool queued = accepted
        && store(pending.url, pending.bytes, mimeType);
    if (!queued) {
        m_retryAfter.insert(pending.canonicalUrl,
                            QDateTime::currentMSecsSinceEpoch()
                                + RetryDelayMs);
    }
    reply->deleteLater();
    startQueuedFetches();
}

void ImageCache::advanceRevision()
{
    m_revision = m_revision == std::numeric_limits<int>::max()
        ? 1 : m_revision + 1;
    emit revisionChanged();
}

#include "imagecache.moc"
