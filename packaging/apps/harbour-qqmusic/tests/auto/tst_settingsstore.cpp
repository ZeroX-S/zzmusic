#include <QtTest>

#include "storage/settingsstore.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

namespace {

Track track(const QString &mid)
{
    Track value;
    value.mid = mid;
    value.title = QStringLiteral("Long title ") + mid;
    value.artist = QStringLiteral("Artist");
    value.artistMid = QStringLiteral("artist-mid");
    value.album = QStringLiteral("Album");
    value.albumMid = QStringLiteral("album-mid");
    value.artworkUrl = QUrl(QStringLiteral(
        "https://y.gtimg.cn/music/photo_new/T002R300x300M000cover.jpg"));
    value.sourceUrl = QUrl(QStringLiteral("https://y.qq.com/n/ryqq/songDetail/")
                           + mid);
    value.durationMs = 180000;
    return value;
}

}

class SettingsStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsQueueWithoutSensitiveOrMediaData()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SettingsStore store(directory.path());
        QueueSnapshot snapshot;
        snapshot.tracks << track(QStringLiteral("001AAA"))
                        << track(QStringLiteral("002BBB"));
        snapshot.currentMid = QStringLiteral("002BBB");
        snapshot.repeatMode = 3;
        snapshot.positionMs = 42000;
        QVERIFY(store.saveQueue(snapshot));

        QFile file(store.queuePath());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray bytes = file.readAll();
        QVERIFY(!bytes.contains(".stream.qqmusic.qq.com"));
        QVERIFY(!bytes.contains("qqmusic_key"));
        QVERIFY(!bytes.contains("mediaUrl"));
        QVERIFY(!bytes.contains("userUin"));

        bool ok = false;
        const QueueSnapshot loaded = store.loadQueue(&ok);
        QVERIFY(ok);
        QCOMPARE(loaded.tracks.size(), 2);
        QCOMPARE(loaded.currentMid, snapshot.currentMid);
        QCOMPARE(loaded.repeatMode, snapshot.repeatMode);
        QCOMPARE(loaded.positionMs, snapshot.positionMs);
    }

    void boundsSettingsAndUsesOwnerOnlyModes()
    {
        QTemporaryDir directory;
        SettingsStore store(directory.path());
        QCOMPARE(SettingsStore::maximumCombinedCacheBytes(),
                 qint64(134217728));
        QCOMPARE(SettingsStore::networkCacheBytes()
                     + SettingsStore::defaultImageCacheBytes(),
                 SettingsStore::maximumCombinedCacheBytes());
        store.setCacheLimitBytes(SettingsStore::maximumCombinedCacheBytes() * 2);
        QCOMPARE(store.cacheLimitBytes(),
                 SettingsStore::maximumCombinedCacheBytes());
        store.setResumeEnabled(false);
        QVERIFY(!store.resumeEnabled());

        const QFileDevice::Permissions directoryPermissions =
            QFileInfo(directory.path()).permissions();
        QVERIFY(directoryPermissions & QFileDevice::ReadOwner);
        QVERIFY(directoryPermissions & QFileDevice::WriteOwner);
        QVERIFY(directoryPermissions & QFileDevice::ExeOwner);
        QVERIFY(!(directoryPermissions & (QFileDevice::ReadGroup
                                          | QFileDevice::WriteGroup
                                          | QFileDevice::ReadOther
                                          | QFileDevice::WriteOther)));
        const QFileDevice::Permissions filePermissions =
            QFileInfo(store.settingsPath()).permissions();
        QVERIFY(!(filePermissions & (QFileDevice::ReadGroup
                                     | QFileDevice::WriteGroup
                                     | QFileDevice::ReadOther
                                     | QFileDevice::WriteOther)));
    }

    void rejectsCorruptOrUnknownQueueSchema()
    {
        QTemporaryDir directory;
        SettingsStore store(directory.path());
        QFile file(store.queuePath());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"version\":2,\"tracks\":[]}");
        file.close();
        bool ok = true;
        QVERIFY(store.loadQueue(&ok).tracks.isEmpty());
        QVERIFY(!ok);
        QVERIFY(store.clearQueue());
        QVERIFY(!QFile::exists(store.queuePath()));
    }
};

QTEST_GUILESS_MAIN(SettingsStoreTest)
#include "tst_settingsstore.moc"
