#include <QtTest>

#include "applicationcontroller.h"
#include "login/nativeloginmanager.h"
#include "models/lyriclistmodel.h"
#include "models/playlistlistmodel.h"
#include "models/tracklistmodel.h"
#include "playback/playbackcontroller.h"
#include "service/qqmusicservice.h"
#include "session/qqcookiejar.h"
#include "session/sessionstore.h"
#include "storage/imagecache.h"
#include "storage/settingsstore.h"

#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>

class FakeControllerSecretsBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString stagedStorePayloadBase64
               READ stagedPayload WRITE setStagedPayload)

public:
    int deleteCalls = 0;
    QString stagedStorePayloadBase64;

    QString stagedPayload() const { return stagedStorePayloadBase64; }
    void setStagedPayload(const QString &payload)
    {
        stagedStorePayloadBase64 = payload;
        if (!payload.isEmpty()) {
            stagedStorePayloadBase64.clear();
        }
    }

public slots:
    void loadSecret() {}
    void deleteSecret() { ++deleteCalls; }

signals:
    void loadFinished(bool succeeded, int errorCode, const QString &payloadBase64);
    void storeFinished(bool succeeded, int errorCode);
    void deleteFinished(bool succeeded, int errorCode);
};

class FakeMusicService : public QqMusicService
{
    Q_OBJECT

public:
    QList<quint64> recommendationIds;
    QList<quint64> dailyThirtyIds;
    QList<quint64> guessYouLikeIds;
    QList<quint64> categoryIds;
    QList<quint64> publicPlaylistIds;
    QStringList requestedCategoryIds;
    QList<quint64> toplistIds;
    QList<quint64> toplistDetailIds;
    QStringList requestedToplistIds;
    QList<quint64> searchIds;
    QList<quint64> playlistSearchIds;
    QList<int> searchPages;
    QList<int> playlistSearchPages;
    QStringList searchAccountUins;
    QStringList playlistSearchAccountUins;
    QList<quint64> playlistIds;
    QList<quint64> createdLibraryIds;
    QList<quint64> collectedLibraryIds;
    QList<quint64> lyricIds;
    QStringList lyricMids;
    QList<quint64> cancelledIds;

    bool requestRecommendations(quint64 operationId) override
    {
        recommendationIds.append(operationId);
        return true;
    }

    bool requestDailyThirty(quint64 operationId) override
    {
        dailyThirtyIds.append(operationId);
        return true;
    }

    bool requestGuessYouLike(quint64 operationId) override
    {
        guessYouLikeIds.append(operationId);
        return true;
    }

    bool requestCategories(quint64 operationId) override
    {
        categoryIds.append(operationId);
        return true;
    }

    bool requestPlaylists(quint64 operationId,
                          const QString &categoryId,
                          int) override
    {
        publicPlaylistIds.append(operationId);
        requestedCategoryIds.append(categoryId);
        return true;
    }

    bool requestToplists(quint64 operationId) override
    {
        toplistIds.append(operationId);
        return true;
    }

    bool requestToplist(quint64 operationId,
                        const QString &toplistId) override
    {
        toplistDetailIds.append(operationId);
        requestedToplistIds.append(toplistId);
        return true;
    }

    bool searchTracks(quint64 operationId,
                      const QString &,
                      int page,
                      const QString &accountUin) override
    {
        searchIds.append(operationId);
        searchPages.append(page);
        searchAccountUins.append(accountUin);
        return true;
    }

    bool searchPlaylists(quint64 operationId,
                         const QString &,
                         int page,
                         const QString &accountUin) override
    {
        playlistSearchIds.append(operationId);
        playlistSearchPages.append(page);
        playlistSearchAccountUins.append(accountUin);
        return true;
    }

    bool requestPlaylist(quint64 operationId,
                         const QString &,
                         bool) override
    {
        playlistIds.append(operationId);
        return true;
    }

    bool requestCreatedLibrary(quint64 operationId,
                               const QString &) override
    {
        createdLibraryIds.append(operationId);
        return true;
    }

    bool requestCollectedLibrary(quint64 operationId,
                                 const QString &) override
    {
        collectedLibraryIds.append(operationId);
        return true;
    }

    bool requestLyrics(quint64 operationId, const QString &songMid) override
    {
        lyricIds.append(operationId);
        lyricMids.append(songMid);
        return true;
    }

    bool cancel(quint64 operationId) override
    {
        cancelledIds.append(operationId);
        return true;
    }

    void completeRecommendations(
        quint64 operationId,
        const QVector<PlaylistSummary> &items)
    {
        emit recommendationsReady(operationId, items);
    }

    void completeDailyThirty(quint64 operationId,
                             const QVector<Track> &items)
    {
        emit dailyThirtyReady(operationId, items);
    }

    void completeGuessYouLike(quint64 operationId,
                              const QVector<Track> &items)
    {
        emit guessYouLikeReady(operationId, items);
    }

    void completeCategories(quint64 operationId,
                            const QVector<CategoryGroup> &groups)
    {
        emit categoriesReady(operationId, groups);
    }

    void completePlaylists(quint64 operationId,
                           const QVector<PlaylistSummary> &items,
                           qint64 total)
    {
        emit playlistsReady(operationId, items, total);
    }

    void completeToplists(quint64 operationId,
                          const QVector<PlaylistSummary> &items)
    {
        emit toplistsReady(operationId, items);
    }

    void completeTracks(quint64 operationId,
                        const QVector<Track> &items,
                        qint64 total)
    {
        emit tracksReady(operationId, items, total);
    }

    void completePlaylist(quint64 operationId,
                          const PlaylistDetail &playlist)
    {
        emit playlistReady(operationId, playlist);
    }

    void completeCreatedLibrary(
        quint64 operationId,
        const QVector<PlaylistSummary> &liked,
        const QVector<PlaylistSummary> &created)
    {
        emit createdLibraryReady(operationId, liked, created);
    }

    void completeCollectedLibrary(
        quint64 operationId,
        const QVector<PlaylistSummary> &collected)
    {
        emit collectedLibraryReady(operationId, collected);
    }

    void completeLyrics(quint64 operationId, const LyricDocument &lyrics)
    {
        emit lyricsReady(operationId, lyrics);
    }

    void fail(quint64 operationId, ErrorKind kind)
    {
        ServiceError error;
        error.operationId = operationId;
        error.kind = kind;
        emit failed(error);
    }
};

static PlaylistSummary playlist(const QString &id, const QString &title)
{
    PlaylistSummary item;
    item.id = id;
    item.title = title;
    return item;
}

static void completeHome(FakeMusicService *service,
                         ApplicationController *controller,
                         const QVector<PlaylistSummary> &items)
{
    const quint64 operationId = controller->homeOperationId();
    service->completeRecommendations(operationId, items);
}

class ApplicationControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesDailyThirtyAndGuessYouLikeOnLoggedInHome()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        UserProfile profile;
        profile.uin = QStringLiteral("1234567890");
        profile.nickname = QStringLiteral("Listener");
        QVERIFY(QMetaObject::invokeMethod(
            &controller, "sessionActivated", Qt::DirectConnection,
            Q_ARG(UserProfile, profile)));
        QCOMPARE(service.dailyThirtyIds.size(), 1);
        const quint64 operationId = controller.homeOperationId();
        QCOMPARE(service.dailyThirtyIds.last(), operationId);

        Track dailyOne;
        dailyOne.mid = QStringLiteral("001DAILYTRK001");
        dailyOne.title = QStringLiteral("Daily One");
        dailyOne.artworkUrl = QUrl(QStringLiteral(
            "https://y.gtimg.cn/music/photo_new/daily.jpg"));
        Track dailyTwo = dailyOne;
        dailyTwo.mid = QStringLiteral("001DAILYTRK002");
        service.completeDailyThirty(
            operationId, QVector<Track>() << dailyOne << dailyTwo);
        QCOMPARE(service.guessYouLikeIds.size(), 1);
        QCOMPARE(service.guessYouLikeIds.last(), operationId);

        Track guess;
        guess.mid = QStringLiteral("001GUESSLIK001");
        guess.title = QStringLiteral("Guess One");
        guess.artworkUrl = QUrl(QStringLiteral(
            "https://y.gtimg.cn/music/photo_new/guess.jpg"));
        service.completeGuessYouLike(
            operationId, QVector<Track>() << guess);
        QCOMPARE(service.recommendationIds.size(), 1);
        QCOMPARE(service.recommendationIds.last(), operationId);
        service.completeRecommendations(
            operationId,
            QVector<PlaylistSummary>() << playlist("10", "Public One")
                                       << playlist("11", "Public Two"));

        QCOMPARE(controller.homeState(), int(PageState::Content));
        QCOMPARE(controller.recommendations()->rowCount(), 4);
        QCOMPARE(controller.recommendations()->data(
                     controller.recommendations()->index(0, 0),
                     PlaylistListModel::PlaylistIdRole).toString(),
                 QStringLiteral("qq-personal-daily30"));
        QCOMPARE(controller.recommendations()->data(
                     controller.recommendations()->index(0, 0),
                     PlaylistListModel::TitleRole).toString(),
                 QStringLiteral("Daily 30"));
        QCOMPARE(controller.recommendations()->data(
                     controller.recommendations()->index(1, 0),
                     PlaylistListModel::PlaylistIdRole).toString(),
                 QStringLiteral("qq-personal-guess-you-like"));

        controller.openPlaylist(QStringLiteral("qq-personal-daily30"));
        QCOMPARE(controller.playlistState(), int(PageState::Content));
        QCOMPARE(controller.playlistTracks()->rowCount(), 2);
        QCOMPARE(controller.playlistTitle(), QStringLiteral("Daily 30"));
        QCOMPARE(service.playlistIds.size(), 0);
    }

    void registersQmlPointerProperties()
    {
        registerApplicationControllerTypes();

        const char *types[] = {
            "PlaylistListModel*",
            "TrackListModel*",
            "LyricListModel*",
            "PlaybackController*",
            "SettingsStore*",
            "ImageCache*"
        };
        for (const char *type : types) {
            QVERIFY2(QMetaType::type(type) != QMetaType::UnknownType, type);
        }

        FakeMusicService service;
        ApplicationController controller(&service);
        QVERIFY(controller.property("recommendations").isValid());
        QVERIFY(controller.property("publicPlaylists").isValid());
        QVERIFY(controller.property("toplists").isValid());
        QVERIFY(controller.property("searchTracks").isValid());
        QVERIFY(controller.property("lyrics").isValid());
        QVERIFY(controller.property("lyricState").isValid());
        QVERIFY(controller.property("selectedCategoryId").isValid());
        QVERIFY(controller.property("discoverState").isValid());
        QVERIFY(controller.property("discoverOperationId").isValid());
        QVERIFY(controller.property("playlistPeriod").isValid());
        QVERIFY(controller.property("createdPlaylistsState").isValid());
        QVERIFY(controller.property("collectedPlaylistsState").isValid());
        QVERIFY(controller.property("playback").isValid());
        QVERIFY(controller.property("settings").isValid());
        QVERIFY(controller.property("imageCache").isValid());
    }

    void ignoresLateHomeResponse()
    {
        FakeMusicService service;
        ApplicationController controller(&service);

        controller.refreshHome();
        const quint64 oldId = controller.homeOperationId();
        controller.refreshHome();
        const quint64 currentId = controller.homeOperationId();
        QVERIFY(currentId > oldId);
        QCOMPARE(controller.homeState(), int(PageState::Loading));

        completeHome(&service, &controller,
                     QVector<PlaylistSummary>() << playlist("2", "Current"));
        QCOMPARE(controller.homeState(), int(PageState::Content));
        QCOMPARE(controller.recommendations()->rowCount(), 1);
        QCOMPARE(controller.recommendations()->data(
                     controller.recommendations()->index(0, 0),
                     PlaylistListModel::TitleRole).toString(),
                 QStringLiteral("Current"));

        service.completeRecommendations(
            oldId,
            QVector<PlaylistSummary>() << playlist("1", "Stale"));
        QCOMPARE(controller.recommendations()->rowCount(), 1);
        QCOMPARE(controller.recommendations()->data(
                     controller.recommendations()->index(0, 0),
                     PlaylistListModel::TitleRole).toString(),
                 QStringLiteral("Current"));
    }

    void ignoresLateHomeFailure()
    {
        FakeMusicService service;
        ApplicationController controller(&service);

        controller.refreshHome();
        const quint64 oldId = controller.homeOperationId();
        controller.refreshHome();
        completeHome(&service, &controller,
                     QVector<PlaylistSummary>() << playlist("2", "Current"));

        service.fail(oldId, ErrorKind::Offline);
        QCOMPARE(controller.homeState(), int(PageState::Content));
        QCOMPARE(controller.recommendations()->rowCount(), 1);
    }

    void changesRecommendationWindowOnRefresh()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        QVector<PlaylistSummary> items;
        for (int index = 0; index < 12; ++index) {
            items.append(playlist(QString::number(index + 1),
                                  QStringLiteral("Item %1").arg(index + 1)));
        }

        controller.refreshHome();
        completeHome(&service, &controller, items);
        QCOMPARE(controller.recommendations()->rowCount(), 8);
        QCOMPARE(controller.recommendations()->data(
                     controller.recommendations()->index(0, 0),
                     PlaylistListModel::PlaylistIdRole).toString(),
                 QStringLiteral("1"));

        controller.refreshHome();
        completeHome(&service, &controller, items);
        QCOMPARE(controller.recommendations()->rowCount(), 8);
        QCOMPARE(controller.recommendations()->data(
                     controller.recommendations()->index(0, 0),
                     PlaylistListModel::PlaylistIdRole).toString(),
                 QStringLiteral("5"));

        controller.refreshHome();
        completeHome(&service, &controller, items);
        QCOMPARE(controller.recommendations()->rowCount(), 8);
        QCOMPARE(controller.recommendations()->data(
                     controller.recommendations()->index(0, 0),
                     PlaylistListModel::PlaylistIdRole).toString(),
                 QStringLiteral("9"));
    }

    void reachesAllTerminalPageStates()
    {
        FakeMusicService service;
        ApplicationController controller(&service);

        controller.refreshHome();
        QCOMPARE(controller.homeState(), int(PageState::Loading));
        completeHome(&service, &controller, QVector<PlaylistSummary>());
        QCOMPARE(controller.homeState(), int(PageState::Empty));

        controller.refreshHome();
        completeHome(&service, &controller,
                     QVector<PlaylistSummary>() << playlist("3", "Cached"));
        QCOMPARE(controller.homeState(), int(PageState::Content));

        controller.refreshHome();
        service.fail(controller.homeOperationId(), ErrorKind::Offline);
        QCOMPARE(controller.homeState(), int(PageState::OfflineWithCache));
        QCOMPARE(controller.recommendations()->rowCount(), 1);

        controller.refreshHome();
        service.fail(controller.homeOperationId(), ErrorKind::SchemaChanged);
        QCOMPARE(controller.homeState(), int(PageState::Error));
    }

    void recommendationFailureDoesNotBlockDiscover()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        Category category;
        category.id = "165";
        category.title = "Pop";
        CategoryGroup group;
        group.title = "Style";
        group.items << category;

        controller.refreshHome();
        const quint64 homeOperationId = controller.homeOperationId();
        controller.refreshDiscover();
        const quint64 discoverOperationId = controller.discoverOperationId();
        QVERIFY(discoverOperationId > homeOperationId);
        QCOMPARE(service.recommendationIds.last(), homeOperationId);
        QCOMPARE(service.categoryIds.last(), discoverOperationId);
        QCOMPARE(controller.homeState(), int(PageState::Loading));
        QCOMPARE(controller.discoverState(), int(PageState::Loading));

        service.fail(homeOperationId, ErrorKind::SchemaChanged);
        QCOMPARE(controller.homeState(), int(PageState::Error));
        QCOMPARE(controller.discoverState(), int(PageState::Loading));

        service.completeCategories(discoverOperationId,
                                   QVector<CategoryGroup>() << group);
        QCOMPARE(controller.selectedCategoryId(), QStringLiteral("165"));
        QCOMPARE(service.publicPlaylistIds.last(), discoverOperationId);
        QCOMPARE(service.requestedCategoryIds.last(), QStringLiteral("165"));
        QCOMPARE(controller.categoryGroups().size(), 1);
        service.completePlaylists(
            discoverOperationId,
            QVector<PlaylistSummary>() << playlist("2", "Filtered"), 1);
        QCOMPARE(service.toplistIds.last(), discoverOperationId);
        service.completeToplists(
            discoverOperationId,
            QVector<PlaylistSummary>() << playlist("26", "Chart"));

        QCOMPARE(controller.homeState(), int(PageState::Error));
        QCOMPARE(controller.discoverState(), int(PageState::Content));
        QCOMPARE(controller.recommendations()->rowCount(), 0);
        QCOMPARE(controller.publicPlaylists()->rowCount(), 1);
        QCOMPARE(controller.toplists()->rowCount(), 1);
    }

    void keepsHomeAndDiscoverTerminalStatesIndependent()
    {
        FakeMusicService service;
        ApplicationController controller(&service);

        controller.refreshHome();
        completeHome(&service, &controller, QVector<PlaylistSummary>());
        QCOMPARE(controller.homeState(), int(PageState::Empty));
        QCOMPARE(controller.discoverState(), int(PageState::Empty));

        controller.refreshDiscover();
        const quint64 discoverOperationId = controller.discoverOperationId();
        service.completeCategories(discoverOperationId,
                                   QVector<CategoryGroup>());
        QCOMPARE(service.toplistIds.last(), discoverOperationId);
        service.completeToplists(
            discoverOperationId,
            QVector<PlaylistSummary>() << playlist("26", "Chart"));
        QCOMPARE(controller.homeState(), int(PageState::Empty));
        QCOMPARE(controller.discoverState(), int(PageState::Content));

        controller.refreshHome();
        completeHome(
            &service, &controller,
            QVector<PlaylistSummary>() << playlist("1", "Recommended"));
        QCOMPARE(controller.homeState(), int(PageState::Content));
        QCOMPARE(controller.discoverState(), int(PageState::Content));

        controller.refreshDiscover();
        const quint64 failedDiscoverId = controller.discoverOperationId();
        QCOMPARE(controller.homeState(), int(PageState::Content));
        QCOMPARE(controller.discoverState(), int(PageState::Loading));
        service.fail(failedDiscoverId, ErrorKind::SchemaChanged);
        QCOMPARE(controller.homeState(), int(PageState::Content));
        QCOMPARE(controller.discoverState(), int(PageState::Error));
    }

    void switchesDiscoverCategoriesWithoutMislabelingAndRetries()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        Category firstCategory;
        firstCategory.id = "165";
        firstCategory.title = "Pop";
        Category secondCategory;
        secondCategory.id = "166";
        secondCategory.title = "Rock";
        CategoryGroup group;
        group.title = "Style";
        group.items << firstCategory << secondCategory;

        controller.refreshDiscover();
        const quint64 initialId = controller.discoverOperationId();
        service.completeCategories(initialId,
                                   QVector<CategoryGroup>() << group);
        QCOMPARE(service.requestedCategoryIds.last(), QStringLiteral("165"));
        service.completePlaylists(
            initialId,
            QVector<PlaylistSummary>() << playlist("2", "First category"), 1);
        service.completeToplists(
            initialId,
            QVector<PlaylistSummary>() << playlist("26", "Chart"));
        QCOMPARE(controller.discoverState(), int(PageState::Content));
        QCOMPARE(controller.publicPlaylists()->rowCount(), 1);
        QCOMPARE(controller.publicPlaylists()->data(
                     controller.publicPlaylists()->index(0, 0),
                     PlaylistListModel::TitleRole).toString(),
                 QStringLiteral("First category"));

        controller.selectCategory(QStringLiteral("166"));
        const quint64 failedId = controller.discoverOperationId();
        QVERIFY(failedId > initialId);
        QCOMPARE(controller.selectedCategoryId(), QStringLiteral("166"));
        QCOMPARE(service.requestedCategoryIds.last(), QStringLiteral("166"));
        QCOMPARE(controller.publicPlaylists()->rowCount(), 0);
        QCOMPARE(controller.toplists()->rowCount(), 1);
        QCOMPARE(controller.discoverState(), int(PageState::Loading));
        QCOMPARE(controller.homeState(), int(PageState::Empty));

        service.fail(failedId, ErrorKind::SchemaChanged);
        QCOMPARE(controller.discoverState(), int(PageState::Error));
        QCOMPARE(controller.publicPlaylists()->rowCount(), 0);
        QCOMPARE(controller.selectedCategoryId(), QStringLiteral("166"));

        controller.refreshDiscover();
        const quint64 retryId = controller.discoverOperationId();
        QVERIFY(retryId > failedId);
        QCOMPARE(controller.categoryGroups().size(), 1);
        QCOMPARE(controller.publicPlaylists()->rowCount(), 0);
        QCOMPARE(controller.toplists()->rowCount(), 1);
        QCOMPARE(controller.discoverState(), int(PageState::Loading));

        service.completePlaylists(
            failedId,
            QVector<PlaylistSummary>() << playlist("3", "Stale"), 1);
        QCOMPARE(controller.publicPlaylists()->rowCount(), 0);

        service.completeCategories(retryId,
                                   QVector<CategoryGroup>() << group);
        QCOMPARE(controller.selectedCategoryId(), QStringLiteral("166"));
        QCOMPARE(service.requestedCategoryIds.last(), QStringLiteral("166"));
        service.completePlaylists(
            retryId,
            QVector<PlaylistSummary>() << playlist("4", "Second category"), 1);
        service.completeToplists(retryId, QVector<PlaylistSummary>());
        QCOMPARE(controller.discoverState(), int(PageState::Content));
        QCOMPARE(controller.publicPlaylists()->rowCount(), 1);
        QCOMPARE(controller.toplists()->rowCount(), 0);
        QCOMPARE(controller.publicPlaylists()->data(
                     controller.publicPlaylists()->index(0, 0),
                     PlaylistListModel::TitleRole).toString(),
                 QStringLiteral("Second category"));
    }

    void cancelsSupersededInteractiveRequests()
    {
        FakeMusicService service;
        ApplicationController controller(&service);

        controller.refreshDiscover();
        const quint64 firstDiscoverId = controller.discoverOperationId();
        controller.refreshDiscover();
        QVERIFY(service.cancelledIds.contains(firstDiscoverId));

        controller.search(QStringLiteral("first"));
        const quint64 firstSearchId = controller.searchOperationId();
        controller.search(QStringLiteral("second"));
        QVERIFY(service.cancelledIds.contains(firstSearchId));

        controller.openToplist(QStringLiteral("26"));
        const quint64 firstToplistId = controller.playlistOperationId();
        controller.openToplist(QStringLiteral("27"));
        QVERIFY(service.cancelledIds.contains(firstToplistId));
    }

    void gatesSearchPlaylistAndLibrarySurfaces()
    {
        FakeMusicService service;
        ApplicationController controller(&service);

        controller.search(QStringLiteral("query"));
        const quint64 searchId = controller.searchOperationId();
        Track track;
        track.mid = "001SANITIZED01";
        track.title = "Track";
        service.completeTracks(searchId, QVector<Track>() << track, 1);
        QCOMPARE(controller.searchState(), int(PageState::Content));
        QCOMPARE(controller.searchTracks()->rowCount(), 1);

        controller.openPlaylist(QStringLiteral("9451963602"));
        PlaylistDetail detail;
        detail.info = playlist("9451963602", "Detail");
        detail.info.author = "Author";
        detail.info.artworkUrl = QUrl("https://y.gtimg.cn/cover.jpg");
        detail.info.trackCount = 1;
        detail.info.sourceUrl =
            QUrl("https://y.qq.com/n/ryqq/playlist/9451963602");
        detail.tracks << track;
        service.completePlaylist(controller.playlistOperationId(), detail);
        QCOMPARE(controller.playlistState(), int(PageState::Content));
        QCOMPARE(controller.playlistTracks()->rowCount(), 1);
        QCOMPARE(controller.playlistTitle(), QStringLiteral("Detail"));
        QCOMPARE(controller.playlistAuthor(), QStringLiteral("Author"));
        QCOMPARE(controller.playlistTrackCount(), 1);
        QCOMPARE(controller.playlistArtwork(), detail.info.artworkUrl);
        QCOMPARE(controller.playlistSourceUrl(), detail.info.sourceUrl);

        controller.openToplist(QStringLiteral("26"));
        const quint64 toplistOperationId = controller.playlistOperationId();
        QCOMPARE(service.toplistDetailIds.last(), toplistOperationId);
        QCOMPARE(service.requestedToplistIds.last(), QStringLiteral("26"));
        PlaylistDetail chartDetail;
        chartDetail.info = playlist("26", "Chart detail");
        chartDetail.period = QStringLiteral("2026-07-24");
        chartDetail.tracks << track;
        service.completePlaylist(toplistOperationId, chartDetail);
        QCOMPARE(controller.playlistState(), int(PageState::Content));
        QCOMPARE(controller.playlistTitle(), QStringLiteral("Chart detail"));
        QCOMPARE(controller.playlistPeriod(), QStringLiteral("2026-07-24"));

        controller.setUserUin(QStringLiteral("10001"));
        controller.refreshMyMusic();
        service.completeCreatedLibrary(
            controller.myMusicOperationId(),
            QVector<PlaylistSummary>() << playlist("4", "Liked"),
            QVector<PlaylistSummary>() << playlist("5", "Created"));
        service.completeCollectedLibrary(
            controller.collectedLibraryOperationId(),
            QVector<PlaylistSummary>() << playlist("6", "Collected"));
        QCOMPARE(controller.myMusicState(), int(PageState::Content));
        QCOMPARE(controller.likedPlaylists()->rowCount(), 1);
        QCOMPARE(controller.createdPlaylists()->rowCount(), 1);
        QCOMPARE(controller.collectedPlaylists()->rowCount(), 1);

        controller.setSearchMode(ApplicationController::PlaylistSearch);
        controller.search(QStringLiteral("playlist"));
        const quint64 playlistSearchId = controller.searchOperationId();
        QCOMPARE(service.playlistSearchIds.last(), playlistSearchId);
        service.completePlaylists(
            playlistSearchId,
            QVector<PlaylistSummary>() << playlist("7", "Result"), 2);
        QCOMPARE(controller.searchState(), int(PageState::Content));
        QCOMPARE(controller.searchPlaylists()->rowCount(), 1);
        QCOMPARE(service.playlistSearchPages.last(), 1);
        controller.loadMoreSearch();
        QCOMPARE(service.playlistSearchPages.last(), 2);
        service.completePlaylists(
            controller.searchOperationId(),
            QVector<PlaylistSummary>() << playlist("8", "Next"), 2);
        QCOMPARE(controller.searchPlaylists()->rowCount(), 2);
    }

    void bindsSearchRequestsToValidatedLoginSession()
    {
        FakeMusicService service;
        ApplicationController controller(&service);

        controller.search(QStringLiteral("anonymous"));
        QCOMPARE(service.searchAccountUins.last(), QString());

        UserProfile profile;
        profile.uin = QStringLiteral("1234567890");
        profile.nickname = QStringLiteral("Listener");
        QVERIFY(QMetaObject::invokeMethod(
            &controller, "sessionActivated", Qt::DirectConnection,
            Q_ARG(UserProfile, profile)));
        QCOMPARE(service.searchAccountUins.last(), profile.uin);

        controller.search(QStringLiteral("account track"));
        QCOMPARE(service.searchAccountUins.last(), profile.uin);
        Track result;
        result.mid = QStringLiteral("001SANITIZED01");
        service.completeTracks(controller.searchOperationId(),
                               QVector<Track>() << result, 2);
        controller.loadMoreSearch();
        QCOMPARE(service.searchAccountUins.last(), profile.uin);
        controller.setSearchMode(ApplicationController::PlaylistSearch);
        controller.search(QStringLiteral("account playlist"));
        QCOMPARE(service.playlistSearchAccountUins.last(), profile.uin);

        QVERIFY(QMetaObject::invokeMethod(
            &controller, "sessionCleared", Qt::DirectConnection));
        QCOMPARE(service.playlistSearchAccountUins.last(), QString());
    }

    void ignoresStaleSearchLoadMoreResponse()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        Track first;
        first.mid = "001SANITIZED01";
        first.title = "First";
        Track stale;
        stale.mid = "001SANITIZED02";
        stale.title = "Stale";
        Track current;
        current.mid = "001SANITIZED03";
        current.title = "Current";

        controller.search(QStringLiteral("first"));
        service.completeTracks(controller.searchOperationId(),
                               QVector<Track>() << first, 3);
        controller.loadMoreSearch();
        const quint64 staleId = controller.searchOperationId();
        controller.search(QStringLiteral("current"));
        const quint64 currentId = controller.searchOperationId();
        QVERIFY(currentId > staleId);
        service.completeTracks(currentId, QVector<Track>() << current, 1);

        service.completeTracks(staleId, QVector<Track>() << stale, 3);
        service.fail(staleId, ErrorKind::Offline);
        QCOMPARE(controller.searchState(), int(PageState::Content));
        QCOMPARE(controller.searchTracks()->rowCount(), 1);
        QCOMPARE(controller.searchTracks()->data(
                     controller.searchTracks()->index(0, 0),
                     TrackListModel::TitleRole).toString(),
                 QStringLiteral("Current"));
    }

    void retriesFailedSearchPageWithoutSkipping()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        Track first;
        first.mid = "001SANITIZED01";
        first.title = "First";

        controller.search(QStringLiteral("query"));
        QCOMPARE(service.searchPages.last(), 1);
        service.completeTracks(controller.searchOperationId(),
                               QVector<Track>() << first, 3);
        controller.loadMoreSearch();
        QCOMPARE(service.searchPages.last(), 2);
        service.fail(controller.searchOperationId(), ErrorKind::Offline);
        QCOMPARE(controller.searchState(), int(PageState::OfflineWithCache));

        controller.loadMoreSearch();
        QCOMPARE(service.searchPages.last(), 2);

        FakeMusicService emptyService;
        ApplicationController emptyController(&emptyService);
        emptyController.search(QStringLiteral("empty"));
        emptyService.fail(emptyController.searchOperationId(),
                          ErrorKind::Offline);
        emptyController.loadMoreSearch();
        QCOMPARE(emptyService.searchPages.size(), 1);
    }

    void clearsAccountModelsOnLogout()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        QSignalSpy logout(&controller, SIGNAL(logoutRequested()));

        controller.setUserUin(QStringLiteral("10001"));
        controller.refreshMyMusic();
        service.completeCreatedLibrary(
            controller.myMusicOperationId(),
            QVector<PlaylistSummary>() << playlist("4", "Liked"),
            QVector<PlaylistSummary>());
        service.completeCollectedLibrary(
            controller.collectedLibraryOperationId(),
            QVector<PlaylistSummary>());
        controller.logout();

        QCOMPARE(logout.count(), 1);
        QVERIFY(controller.userUin().isEmpty());
        QCOMPARE(controller.likedPlaylists()->rowCount(), 0);
        QCOMPARE(controller.myMusicState(), int(PageState::Empty));
    }

    void sessionStoreBindingAndLogoutAreAsynchronous()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        QqCookieJar active;
        SessionStore store(&active);
        FakeControllerSecretsBridge bridge;
        QVERIFY(store.bindBridge(&bridge));
        QSignalSpy logout(&controller, SIGNAL(logoutRequested()));
        QSignalSpy accountFailure(&controller, SIGNAL(accountActionFailed()));

        controller.setSessionStore(&store);
        QCOMPARE(logout.count(), 0);

        UserProfile profile;
        profile.uin = QStringLiteral("10001");
        profile.nickname = QStringLiteral("Account");
        store.sessionActivated(profile);
        QVERIFY(controller.loggedIn());

        controller.logout();
        QCOMPARE(bridge.deleteCalls, 1);
        QVERIFY(controller.loggedIn());
        QCOMPARE(logout.count(), 0);

        emit bridge.deleteFinished(false, 1);
        QCOMPARE(accountFailure.count(), 1);
        QVERIFY(controller.loggedIn());

        controller.logout();
        QCOMPARE(bridge.deleteCalls, 2);
        emit bridge.deleteFinished(true, 0);
        QVERIFY(!controller.loggedIn());
        QCOMPARE(logout.count(), 1);
    }

    void nativeLoginPresentationTracksBoundedAttemptStates()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        QNetworkAccessManager network;
        NativeLoginManager manager(&network, NativeLoginLimits());
        QqCookieJar active;
        SessionStore store(&active);
        controller.setSessionStore(&store);
        controller.setNativeLoginManager(&manager);
        QSignalSpy changed(&controller, SIGNAL(loginChanged()));

        manager.stateChanged(NativeLoginManager::Opening, QString());
        QCOMPARE(controller.loginStatus(), QStringLiteral("opening"));
        QVERIFY(controller.loginCancellable());

        const QByteArray png = QByteArray::fromHex("89504e470d0a1a0a");
        manager.qrReady(QString(), png, 120, 120, 7);
        QCOMPARE(controller.loginStatus(), QStringLiteral("qr"));
        QCOMPARE(controller.loginQrGeneration(), 7);
        QCOMPARE(controller.loginQrImage(),
                 QStringLiteral("data:image/png;base64,")
                     + QString::fromLatin1(png.toBase64()));

        manager.stateChanged(NativeLoginManager::Scanned, QString());
        QCOMPARE(controller.loginStatus(), QStringLiteral("scanned"));
        QVERIFY(controller.loginCancellable());

        manager.stateChanged(NativeLoginManager::CandidateReady, QString());
        QCOMPARE(controller.loginStatus(), QStringLiteral("validating"));
        QVERIFY(!controller.loginCancellable());
        QVERIFY(controller.loginQrImage().isEmpty());

        ServiceError loginFailure;
        loginFailure.kind = ErrorKind::Offline;
        loginFailure.diagnosticCode =
            QStringLiteral("qq.login.mqtt.connect.send");
        manager.attemptFailed(QString(), loginFailure);
        QCOMPARE(controller.loginStatus(), QStringLiteral("error"));
        QCOMPARE(controller.loginDiagnosticCode(),
                 QStringLiteral("qq.login.mqtt.connect.send"));

        manager.stateChanged(NativeLoginManager::CandidateReady, QString());

        UserProfile profile;
        profile.uin = QStringLiteral("10001");
        profile.nickname = QStringLiteral("Account");
        store.sessionActivated(profile);
        QCOMPARE(controller.loginStatus(), QStringLiteral("success"));
        QVERIFY(controller.loggedIn());

        store.sessionCleared();
        QVERIFY(controller.loginStatus().isEmpty());
        QVERIFY(!controller.loggedIn());

        manager.stateChanged(NativeLoginManager::CandidateReady, QString());
        store.candidateFailed();
        QCOMPARE(controller.loginStatus(), QStringLiteral("error"));
        QVERIFY(!controller.loginCancellable());
        QVERIFY(changed.count() >= 6);
    }

    void invalidatesMyMusicWhenUserIdentityIsCleared()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        controller.setUserUin(QStringLiteral("10001"));
        controller.refreshMyMusic();
        const quint64 staleCreatedId = controller.myMusicOperationId();
        const quint64 staleCollectedId =
            controller.collectedLibraryOperationId();

        controller.setUserUin(QString());
        controller.refreshMyMusic();
        QCOMPARE(controller.myMusicOperationId(), quint64(0));
        QCOMPARE(controller.myMusicState(), int(PageState::Error));
        service.completeCreatedLibrary(
            staleCreatedId,
            QVector<PlaylistSummary>() << playlist("4", "Stale"),
            QVector<PlaylistSummary>());
        service.completeCollectedLibrary(
            staleCollectedId,
            QVector<PlaylistSummary>());

        QCOMPARE(controller.myMusicState(), int(PageState::Error));
        QCOMPARE(controller.likedPlaylists()->rowCount(), 0);
    }

    void keepsSuccessfulLibraryHalfWhenTheOtherFails()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        controller.setUserUin(QStringLiteral("10001"));
        controller.refreshMyMusic();
        const quint64 createdId = controller.myMusicOperationId();
        const quint64 collectedId = controller.collectedLibraryOperationId();
        QVERIFY(createdId != collectedId);

        service.completeCreatedLibrary(
            createdId,
            QVector<PlaylistSummary>() << playlist("4", "Liked"),
            QVector<PlaylistSummary>() << playlist("5", "Created"));
        QCOMPARE(controller.myMusicState(), int(PageState::Content));
        service.fail(collectedId, ErrorKind::Offline);

        QCOMPARE(controller.likedPlaylists()->rowCount(), 1);
        QCOMPARE(controller.createdPlaylists()->rowCount(), 1);
        QCOMPARE(controller.collectedPlaylists()->rowCount(), 0);
        QCOMPARE(controller.createdPlaylistsState(), int(PageState::Content));
        QCOMPARE(controller.collectedPlaylistsState(), int(PageState::Error));
        QCOMPARE(controller.myMusicState(), int(PageState::Content));
    }

    void keepsCollectedLibraryWhenCreatedRequestFails()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        controller.setUserUin(QStringLiteral("10001"));
        controller.refreshMyMusic();
        const quint64 createdId = controller.myMusicOperationId();
        const quint64 collectedId = controller.collectedLibraryOperationId();

        service.completeCollectedLibrary(
            collectedId,
            QVector<PlaylistSummary>() << playlist("6", "Collected"));
        QCOMPARE(controller.createdPlaylistsState(), int(PageState::Loading));
        QCOMPARE(controller.collectedPlaylistsState(), int(PageState::Content));
        QCOMPARE(controller.myMusicState(), int(PageState::Content));

        service.fail(createdId, ErrorKind::SchemaChanged);
        QCOMPARE(controller.likedPlaylists()->rowCount(), 0);
        QCOMPARE(controller.createdPlaylists()->rowCount(), 0);
        QCOMPARE(controller.collectedPlaylists()->rowCount(), 1);
        QCOMPARE(controller.createdPlaylistsState(), int(PageState::Error));
        QCOMPARE(controller.collectedPlaylistsState(), int(PageState::Content));
        QCOMPARE(controller.myMusicState(), int(PageState::Content));
    }

    void combinesLibraryEmptyErrorAndOfflineCacheStates()
    {
        FakeMusicService emptyService;
        ApplicationController emptyController(&emptyService);
        emptyController.setUserUin(QStringLiteral("10001"));
        emptyController.refreshMyMusic();
        emptyService.completeCreatedLibrary(
            emptyController.myMusicOperationId(),
            QVector<PlaylistSummary>(),
            QVector<PlaylistSummary>());
        emptyService.fail(emptyController.collectedLibraryOperationId(),
                          ErrorKind::SchemaChanged);

        QCOMPARE(emptyController.createdPlaylistsState(),
                 int(PageState::Empty));
        QCOMPARE(emptyController.collectedPlaylistsState(),
                 int(PageState::Error));
        QCOMPARE(emptyController.myMusicState(), int(PageState::Error));

        FakeMusicService cachedService;
        ApplicationController cachedController(&cachedService);
        cachedController.setUserUin(QStringLiteral("10001"));
        cachedController.refreshMyMusic();
        cachedService.completeCreatedLibrary(
            cachedController.myMusicOperationId(),
            QVector<PlaylistSummary>() << playlist("4", "Liked"),
            QVector<PlaylistSummary>() << playlist("5", "Created"));
        cachedService.completeCollectedLibrary(
            cachedController.collectedLibraryOperationId(),
            QVector<PlaylistSummary>() << playlist("6", "Collected"));

        cachedController.refreshMyMusic();
        cachedService.fail(cachedController.myMusicOperationId(),
                           ErrorKind::Offline);
        QCOMPARE(cachedController.createdPlaylistsState(),
                 int(PageState::OfflineWithCache));
        QCOMPARE(cachedController.myMusicState(),
                 int(PageState::OfflineWithCache));

        cachedService.completeCollectedLibrary(
            cachedController.collectedLibraryOperationId(),
            QVector<PlaylistSummary>() << playlist("7", "Fresh collected"));
        QCOMPARE(cachedController.collectedPlaylistsState(),
                 int(PageState::Content));
        QCOMPARE(cachedController.createdPlaylists()->rowCount(), 1);
        QCOMPARE(cachedController.collectedPlaylists()->rowCount(), 1);
        QCOMPARE(cachedController.myMusicState(),
                 int(PageState::OfflineWithCache));
    }

    void ignoresBothLibraryResponsesFromPreviousRefresh()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        controller.setUserUin(QStringLiteral("10001"));
        controller.refreshMyMusic();
        const quint64 staleCreatedId = controller.myMusicOperationId();
        const quint64 staleCollectedId =
            controller.collectedLibraryOperationId();

        controller.refreshMyMusic();
        const quint64 currentCreatedId = controller.myMusicOperationId();
        const quint64 currentCollectedId =
            controller.collectedLibraryOperationId();
        QVERIFY(currentCreatedId > staleCollectedId);
        QVERIFY(currentCollectedId > currentCreatedId);

        service.completeCreatedLibrary(
            staleCreatedId,
            QVector<PlaylistSummary>() << playlist("1", "Stale liked"),
            QVector<PlaylistSummary>() << playlist("2", "Stale created"));
        service.completeCollectedLibrary(
            staleCollectedId,
            QVector<PlaylistSummary>() << playlist("3", "Stale collected"));
        QCOMPARE(controller.likedPlaylists()->rowCount(), 0);
        QCOMPARE(controller.createdPlaylists()->rowCount(), 0);
        QCOMPARE(controller.collectedPlaylists()->rowCount(), 0);
        QCOMPARE(controller.myMusicState(), int(PageState::Loading));

        service.completeCollectedLibrary(
            currentCollectedId,
            QVector<PlaylistSummary>() << playlist("6", "Current collected"));
        QCOMPARE(controller.myMusicState(), int(PageState::Content));
        service.completeCreatedLibrary(
            currentCreatedId,
            QVector<PlaylistSummary>() << playlist("4", "Current liked"),
            QVector<PlaylistSummary>() << playlist("5", "Current created"));
        QCOMPARE(controller.likedPlaylists()->rowCount(), 1);
        QCOMPARE(controller.createdPlaylists()->rowCount(), 1);
        QCOMPARE(controller.collectedPlaylists()->rowCount(), 1);
        QCOMPARE(controller.createdPlaylistsState(), int(PageState::Content));
        QCOMPARE(controller.collectedPlaylistsState(), int(PageState::Content));
        QCOMPARE(controller.myMusicState(), int(PageState::Content));
    }

    void routesModelRowsToPlaybackQueue()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        PlaybackController playback(0, 0);
        controller.setPlaybackController(&playback);

        Track first;
        first.mid = QStringLiteral("001SANITIZED01");
        first.title = QStringLiteral("First");
        Track second;
        second.mid = QStringLiteral("001SANITIZED02");
        second.title = QStringLiteral("Second");

        controller.search(QStringLiteral("query"));
        service.completeTracks(controller.searchOperationId(),
                               QVector<Track>() << first, 1);
        QVERIFY(controller.playSearchTrack(0));
        QCOMPARE(playback.queueTracks().size(), 1);
        QCOMPARE(playback.currentTrack().mid, first.mid);
        QVERIFY(controller.appendSearchTrack(0));
        QCOMPARE(playback.queueTracks().size(), 2);
        QVERIFY(!controller.playSearchTrack(4));

        controller.openPlaylist(QStringLiteral("9451963602"));
        PlaylistDetail detail;
        detail.info = playlist(QStringLiteral("9451963602"),
                               QStringLiteral("Playlist"));
        detail.tracks << first << second;
        service.completePlaylist(controller.playlistOperationId(), detail);
        QVERIFY(controller.playPlaylistAt(1));
        QCOMPARE(playback.queueTracks().size(), 2);
        QCOMPARE(playback.currentTrack().mid, second.mid);
        QVERIFY(controller.playNextPlaylistTrack(0));
        QVERIFY(controller.appendPlaylistTrack(1));
        QVERIFY(controller.playQueueIndex(0));
        QCOMPARE(playback.currentTrack().mid, first.mid);
        QVERIFY(!controller.playQueueIndex(-1));
    }

    void followsCurrentTrackLyricsAndIgnoresLateResponse()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        PlaybackController playback(0, 0);
        controller.setPlaybackController(&playback);

        Track first;
        first.mid = QStringLiteral("001SANITIZED01");
        first.title = QStringLiteral("First");
        Track second;
        second.mid = QStringLiteral("001SANITIZED02");
        second.title = QStringLiteral("Second");
        playback.replaceQueue(QVector<Track>() << first, 0, false);
        QCOMPARE(service.lyricMids.last(), first.mid);
        QCOMPARE(controller.lyricState(), int(PageState::Loading));
        const quint64 firstId = service.lyricIds.last();

        playback.replaceQueue(QVector<Track>() << second, 0, false);
        QCOMPARE(service.lyricMids.last(), second.mid);
        const quint64 secondId = service.lyricIds.last();
        QVERIFY(secondId > firstId);

        LyricDocument stale;
        LyricLine staleLine;
        staleLine.timeMs = 0;
        staleLine.text = QStringLiteral("Stale");
        stale.lines << staleLine;
        service.completeLyrics(firstId, stale);
        QCOMPARE(controller.lyrics()->count(), 0);
        QCOMPARE(controller.lyricState(), int(PageState::Loading));

        LyricDocument current;
        LyricLine firstLine;
        firstLine.timeMs = 0;
        firstLine.text = QStringLiteral("Current");
        LyricLine secondLine;
        secondLine.timeMs = 5000;
        secondLine.text = QStringLiteral("Next");
        current.lines << firstLine << secondLine;
        service.completeLyrics(secondId, current);
        QCOMPARE(controller.lyrics()->count(), 2);
        QCOMPARE(controller.lyricState(), int(PageState::Content));
        QCOMPARE(controller.lyrics()->currentIndex(), 0);
        playback.seek(6000);
        QCOMPARE(controller.lyrics()->currentIndex(), 1);

        Track third;
        third.mid = QStringLiteral("001SANITIZED03");
        playback.replaceQueue(QVector<Track>() << third, 0, false);
        const quint64 thirdId = service.lyricIds.last();
        QCOMPARE(controller.lyricState(), int(PageState::Loading));
        service.completeLyrics(thirdId, LyricDocument());
        QCOMPARE(controller.lyrics()->count(), 0);
        QCOMPARE(controller.lyricState(), int(PageState::Empty));

        Track fourth;
        fourth.mid = QStringLiteral("001SANITIZED04");
        playback.replaceQueue(QVector<Track>() << fourth, 0, false);
        const quint64 fourthId = service.lyricIds.last();
        service.fail(fourthId, ErrorKind::Offline);
        QCOMPARE(controller.lyrics()->count(), 0);
        QCOMPARE(controller.lyricState(), int(PageState::Error));
    }

    void exposesBoundedCacheControls()
    {
        QTemporaryDir settingsDirectory;
        QTemporaryDir imageDirectory;
        QVERIFY(settingsDirectory.isValid());
        QVERIFY(imageDirectory.isValid());
        FakeMusicService service;
        ApplicationController controller(&service);
        SettingsStore settings(settingsDirectory.path());
        ImageCache images(imageDirectory.path(),
                          SettingsStore::defaultImageCacheBytes());
        QSignalSpy artworkChanged(&controller,
                                  SIGNAL(artworkRevisionChanged()));
        controller.setStorage(&settings, &images);
        QCOMPARE(artworkChanged.count(), 1);
        QSignalSpy limitChanged(&controller, SIGNAL(cacheLimitBytesChanged()));

        const QUrl remote(QStringLiteral(
            "https://y.gtimg.cn/music/photo_new/controller.png"));
        QSignalSpy artworkAvailable(&images,
                                    SIGNAL(artworkAvailable(QUrl,QUrl)));
        QVERIFY(images.store(remote,
            QByteArray::fromHex("89504e470d0a1a0a")
                + QByteArray(16, 'p'), "image/png"));
        QTRY_COMPARE(artworkAvailable.count(), 1);
        const QUrl local = artworkAvailable.first().at(1).toUrl();
        QVERIFY(local.isLocalFile());
        QCOMPARE(controller.artworkSource(remote), local);
        QCOMPARE(controller.artworkRevision(), images.revision());
        QCOMPARE(artworkChanged.count(), 2);

        controller.setCacheLimitBytes(qint64(64) * 1024 * 1024);
        QCOMPARE(controller.cacheLimitBytes(), qint64(64) * 1024 * 1024);
        QCOMPARE(images.maximumBytes(), qint64(48) * 1024 * 1024);
        QCOMPARE(limitChanged.count(), 1);
        QVERIFY(controller.clearCache());
        QCOMPARE(controller.cacheUsageBytes(), qint64(0));
        QCOMPARE(artworkChanged.count(), 3);
    }

    void exposesAuthenticationExpiryUntilConfirmed()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        QqCookieJar active;
        SessionStore store(&active);
        FakeControllerSecretsBridge bridge;
        QVERIFY(store.bindBridge(&bridge));
        controller.setSessionStore(&store);
        QSignalSpy expired(&controller, SIGNAL(authenticationExpired()));
        QSignalSpy accountFailure(&controller, SIGNAL(accountActionFailed()));
        QSignalSpy pendingChanged(
            &controller, SIGNAL(authenticationRemovalPendingChanged()));

        service.fail(900, ErrorKind::AuthenticationExpired);
        QVERIFY(controller.authenticationRemovalPending());
        QCOMPARE(expired.count(), 1);
        QCOMPARE(pendingChanged.count(), 1);
        service.fail(901, ErrorKind::AuthenticationExpired);
        QCOMPARE(expired.count(), 1);

        controller.confirmExpiredSessionRemoval();
        QVERIFY(controller.authenticationRemovalPending());
        QCOMPARE(bridge.deleteCalls, 1);
        emit bridge.deleteFinished(false, 7);
        QCOMPARE(accountFailure.count(), 1);
        QVERIFY(controller.authenticationRemovalPending());
        controller.confirmExpiredSessionRemoval();
        QCOMPARE(bridge.deleteCalls, 2);
        emit bridge.deleteFinished(true, 0);
        QVERIFY(!controller.authenticationRemovalPending());
        QCOMPARE(pendingChanged.count(), 2);
    }

    void sanitizesTerminalPlaybackFailureSignal()
    {
        FakeMusicService service;
        ApplicationController controller(&service);
        PlaybackController playback(0, 0);
        controller.setPlaybackController(&playback);
        Track track;
        track.mid = QStringLiteral("001SANITIZED01");
        track.title = QStringLiteral("Visible title");
        playback.replaceQueue(QVector<Track>() << track, 0, false);
        QSignalSpy failure(
            &controller,
            SIGNAL(terminalPlaybackFailure(int,QString)));

        ServiceError error;
        error.kind = ErrorKind::Offline;
        error.diagnosticCode = QStringLiteral(
            "cookie=never-expose&url=https://example.invalid/private");
        emit playback.errorChanged(error);

        QCOMPARE(failure.count(), 1);
        QCOMPARE(failure.at(0).at(0).toInt(), int(ErrorKind::Offline));
        QCOMPARE(failure.at(0).at(1).toString(), track.title);
        QVERIFY(!failure.at(0).at(1).toString().contains("cookie"));
        QVERIFY(!failure.at(0).at(1).toString().contains("https"));
    }

    void persistsQueueWhilePositionUpdatesContinue()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        FakeMusicService service;
        SettingsStore settings(directory.path());
        PlaybackController playback(0, 0);
        Track track;
        track.mid = QStringLiteral("001PERSISTED01");
        track.title = QStringLiteral("Persisted track");
        playback.replaceQueue(QVector<Track>() << track, 0, false);

        ApplicationController controller(&service);
        controller.setPlaybackController(&playback);
        controller.setStorage(&settings, 0);

        int updateCount = 0;
        QTimer updates;
        updates.setInterval(100);
        connect(&updates, &QTimer::timeout, [&]() {
            ++updateCount;
            playback.seek(qint64(updateCount) * 1000);
        });
        updates.start();

        QueueSnapshot snapshot;
        QElapsedTimer elapsed;
        elapsed.start();
        while (elapsed.elapsed() < 4000) {
            QTest::qWait(50);
            bool ok = false;
            const QueueSnapshot candidate = settings.loadQueue(&ok);
            if (ok && !candidate.tracks.isEmpty()
                    && candidate.positionMs > 0) {
                snapshot = candidate;
                break;
            }
        }

        QVERIFY(updates.isActive());
        QVERIFY(updateCount >= 5);
        QCOMPARE(snapshot.tracks.size(), 1);
        QCOMPARE(snapshot.currentMid, track.mid);
        QVERIFY(snapshot.positionMs > 0);

        const qint64 firstPersistedPosition = snapshot.positionMs;
        snapshot = QueueSnapshot();
        elapsed.restart();
        while (elapsed.elapsed() < 4000) {
            QTest::qWait(50);
            bool ok = false;
            const QueueSnapshot candidate = settings.loadQueue(&ok);
            if (ok && candidate.positionMs > firstPersistedPosition) {
                snapshot = candidate;
                break;
            }
        }

        QVERIFY(updates.isActive());
        QCOMPARE(snapshot.currentMid, track.mid);
        QVERIFY(snapshot.positionMs > firstPersistedPosition);
        QVERIFY(snapshot.positionMs <= playback.positionMs());
        updates.stop();
    }

    void destructorFlushesPendingQueueSnapshot()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        FakeMusicService service;
        SettingsStore settings(directory.path());
        PlaybackController playback(0, 0);
        Track track;
        track.mid = QStringLiteral("001PERSISTED02");
        track.title = QStringLiteral("Pending track");
        playback.replaceQueue(QVector<Track>() << track, 0, false);

        {
            ApplicationController controller(&service);
            controller.setPlaybackController(&playback);
            controller.setStorage(&settings, 0);
            playback.seek(42000);

            bool ok = false;
            const QueueSnapshot beforeDestruction = settings.loadQueue(&ok);
            QVERIFY(ok);
            QVERIFY(beforeDestruction.tracks.isEmpty());
        }

        bool ok = false;
        const QueueSnapshot snapshot = settings.loadQueue(&ok);
        QVERIFY(ok);
        QCOMPARE(snapshot.tracks.size(), 1);
        QCOMPARE(snapshot.currentMid, track.mid);
        QCOMPARE(snapshot.positionMs, qint64(42000));
    }

    void respectsInjectedAndDefaultServiceOwnership()
    {
        QObject owner;
        FakeMusicService *injected = new FakeMusicService;
        injected->setParent(&owner);
        ApplicationController injectedController(injected);
        QCOMPARE(injected->parent(), &owner);

        delete injected;
        injectedController.refreshHome();
        QCOMPARE(injectedController.homeState(), int(PageState::Error));

        ApplicationController defaultController;
        QqMusicService *owned =
            defaultController.findChild<QqMusicService *>();
        QVERIFY(owned);
        QCOMPARE(owned->parent(), &defaultController);
    }
};

QTEST_GUILESS_MAIN(ApplicationControllerTest)
#include "tst_applicationcontroller.moc"
