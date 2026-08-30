#include <QtTest>

#include "models/lyriclistmodel.h"
#include "models/playlistlistmodel.h"
#include "models/tracklistmodel.h"

class ModelsTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesStablePlaylistRoles()
    {
        PlaylistListModel model;
        const QHash<int, QByteArray> roles = model.roleNames();
        QCOMPARE(roles.value(PlaylistListModel::PlaylistIdRole),
                 QByteArray("playlistId"));
        QCOMPARE(roles.value(PlaylistListModel::TitleRole), QByteArray("title"));
        QCOMPARE(roles.value(PlaylistListModel::AuthorRole), QByteArray("author"));
        QCOMPARE(roles.value(PlaylistListModel::ArtworkRole),
                 QByteArray("artwork"));
        QCOMPARE(roles.value(PlaylistListModel::TrackCountRole),
                 QByteArray("trackCount"));
        QCOMPARE(roles.value(PlaylistListModel::SourceUrlRole),
                 QByteArray("sourceUrl"));

        PlaylistSummary item;
        item.id = "9451963602";
        item.title = "Playlist";
        item.author = "Author";
        item.artworkUrl = QUrl("https://y.gtimg.cn/cover.jpg");
        item.trackCount = 8;
        item.sourceUrl = QUrl("https://y.qq.com/n/ryqq/playlist/9451963602");
        model.replace(QVector<PlaylistSummary>() << item);

        QCOMPARE(model.rowCount(), 1);
        const QModelIndex index = model.index(0, 0);
        QCOMPARE(model.data(index, PlaylistListModel::PlaylistIdRole).toString(),
                 item.id);
        QCOMPARE(model.data(index, PlaylistListModel::TrackCountRole).toInt(), 8);
        model.append(QVector<PlaylistSummary>() << item);
        QCOMPARE(model.rowCount(), 2);
        model.clear();
        QCOMPARE(model.rowCount(), 0);
    }

    void exposesStableTrackRoles()
    {
        TrackListModel model;
        const QHash<int, QByteArray> roles = model.roleNames();
        QCOMPARE(roles.value(TrackListModel::TrackMidRole),
                 QByteArray("trackMid"));
        QCOMPARE(roles.value(TrackListModel::TitleRole), QByteArray("title"));
        QCOMPARE(roles.value(TrackListModel::ArtistRole), QByteArray("artist"));
        QCOMPARE(roles.value(TrackListModel::AlbumRole), QByteArray("album"));
        QCOMPARE(roles.value(TrackListModel::ArtworkRole), QByteArray("artwork"));
        QCOMPARE(roles.value(TrackListModel::DurationMsRole),
                 QByteArray("durationMs"));
        QCOMPARE(roles.value(TrackListModel::PlayableRole),
                 QByteArray("playable"));
        QCOMPARE(roles.value(TrackListModel::SourceUrlRole),
                 QByteArray("sourceUrl"));

        Track track;
        track.mid = "001SANITIZED01";
        track.title = "Track";
        track.artist = "Artist";
        track.album = "Album";
        track.durationMs = 180000;
        track.playable = false;
        model.replace(QVector<Track>() << track);

        const QModelIndex index = model.index(0, 0);
        QCOMPARE(model.data(index, TrackListModel::TrackMidRole).toString(),
                 track.mid);
        QCOMPARE(model.data(index, TrackListModel::DurationMsRole).toLongLong(),
                 qint64(180000));
        QCOMPARE(model.data(index, TrackListModel::PlayableRole).toBool(), false);
    }

    void tracksCurrentLyricWithoutChangingRows()
    {
        LyricListModel model;
        const QHash<int, QByteArray> roles = model.roleNames();
        QCOMPARE(roles.value(LyricListModel::TimeMsRole), QByteArray("timeMs"));
        QCOMPARE(roles.value(LyricListModel::TextRole), QByteArray("text"));
        QCOMPARE(roles.value(LyricListModel::TranslationRole),
                 QByteArray("translation"));
        QCOMPARE(roles.value(LyricListModel::CurrentRole), QByteArray("current"));

        LyricDocument document;
        LyricLine first;
        first.timeMs = 0;
        first.text = "First";
        LyricLine second;
        second.timeMs = 1000;
        second.text = "Second";
        second.translation = "Translation";
        document.lines << first << second;
        model.replace(document);

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.currentIndex(), -1);
        model.setPosition(1200);
        QCOMPARE(model.currentIndex(), 1);
        QCOMPARE(model.data(model.index(1, 0),
                            LyricListModel::CurrentRole).toBool(), true);
        QCOMPARE(model.data(model.index(0, 0),
                            LyricListModel::CurrentRole).toBool(), false);
        model.clear();
        QCOMPARE(model.currentIndex(), -1);
    }

    void updatesEqualSizedModelsWithoutResettingViews()
    {
        TrackListModel tracksModel;
        Track first;
        first.mid = QStringLiteral("first");
        first.title = QStringLiteral("Before");
        Track second;
        second.mid = QStringLiteral("second");
        second.title = QStringLiteral("Second");
        tracksModel.replace(QVector<Track>() << first << second);
        QSignalSpy trackResets(&tracksModel, SIGNAL(modelReset()));
        QSignalSpy trackChanges(&tracksModel,
                                SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));

        first.title = QStringLiteral("After");
        tracksModel.replace(QVector<Track>() << first << second);
        QCOMPARE(trackResets.count(), 0);
        QCOMPARE(trackChanges.count(), 1);
        Track selected;
        QVERIFY(tracksModel.itemAt(0, &selected));
        QCOMPARE(selected.title, QStringLiteral("After"));
        tracksModel.replace(QVector<Track>() << first << second);
        QCOMPARE(trackChanges.count(), 1);

        PlaylistListModel playlistsModel;
        PlaylistSummary playlist;
        playlist.id = QStringLiteral("playlist");
        playlist.title = QStringLiteral("Before");
        playlistsModel.replace(QVector<PlaylistSummary>() << playlist);
        QSignalSpy playlistResets(&playlistsModel, SIGNAL(modelReset()));
        QSignalSpy playlistChanges(
            &playlistsModel,
            SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        playlist.title = QStringLiteral("After");
        playlistsModel.replace(QVector<PlaylistSummary>() << playlist);
        QCOMPARE(playlistResets.count(), 0);
        QCOMPARE(playlistChanges.count(), 1);
    }

    void findsLyricsAcrossLargeForwardAndBackwardSeeks()
    {
        LyricDocument document;
        for (int index = 0; index < 4096; ++index) {
            LyricLine line;
            line.timeMs = qint64(index) * 1000;
            line.text = QStringLiteral("Line %1").arg(index);
            document.lines.append(line);
        }
        LyricListModel model;
        model.replace(document);
        model.setPosition(4095000);
        QCOMPARE(model.currentIndex(), 4095);
        model.setPosition(125000);
        QCOMPARE(model.currentIndex(), 125);
        model.setPosition(-1);
        QCOMPARE(model.currentIndex(), -1);
    }
};

QTEST_GUILESS_MAIN(ModelsTest)
#include "tst_models.moc"
