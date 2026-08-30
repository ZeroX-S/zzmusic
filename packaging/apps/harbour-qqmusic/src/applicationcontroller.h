#ifndef APPLICATIONCONTROLLER_H
#define APPLICATIONCONTROLLER_H

#include "core/types.h"

#include <QObject>
#include <QPointer>
#include <QVariantList>

class LyricListModel;
class ImageCache;
class NativeLoginManager;
class PlaylistListModel;
class PlaybackController;
class QqMusicService;
class SessionStore;
class SettingsStore;
class TrackListModel;
class QTimer;

class ApplicationController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PlaylistListModel *recommendations READ recommendations CONSTANT)
    Q_PROPERTY(PlaylistListModel *publicPlaylists READ publicPlaylists CONSTANT)
    Q_PROPERTY(PlaylistListModel *toplists READ toplists CONSTANT)
    Q_PROPERTY(TrackListModel *searchTracks READ searchTracks CONSTANT)
    Q_PROPERTY(PlaylistListModel *searchPlaylists READ searchPlaylists CONSTANT)
    Q_PROPERTY(TrackListModel *playlistTracks READ playlistTracks CONSTANT)
    Q_PROPERTY(PlaylistListModel *likedPlaylists READ likedPlaylists CONSTANT)
    Q_PROPERTY(PlaylistListModel *createdPlaylists READ createdPlaylists CONSTANT)
    Q_PROPERTY(PlaylistListModel *collectedPlaylists READ collectedPlaylists CONSTANT)
    Q_PROPERTY(LyricListModel *lyrics READ lyrics CONSTANT)
    Q_PROPERTY(QVariantList categoryGroups READ categoryGroups NOTIFY categoryGroupsChanged)
    Q_PROPERTY(QString selectedCategoryId READ selectedCategoryId
               NOTIFY selectedCategoryChanged)
    Q_PROPERTY(int homeState READ homeState NOTIFY homeStateChanged)
    Q_PROPERTY(int discoverState READ discoverState NOTIFY discoverStateChanged)
    Q_PROPERTY(int searchState READ searchState NOTIFY searchStateChanged)
    Q_PROPERTY(int playlistState READ playlistState NOTIFY playlistStateChanged)
    Q_PROPERTY(int myMusicState READ myMusicState NOTIFY myMusicStateChanged)
    Q_PROPERTY(int createdPlaylistsState READ createdPlaylistsState
               NOTIFY createdPlaylistsStateChanged)
    Q_PROPERTY(int collectedPlaylistsState READ collectedPlaylistsState
               NOTIFY collectedPlaylistsStateChanged)
    Q_PROPERTY(int lyricState READ lyricState NOTIFY lyricStateChanged)
    Q_PROPERTY(quint64 homeOperationId READ homeOperationId NOTIFY homeOperationIdChanged)
    Q_PROPERTY(quint64 discoverOperationId READ discoverOperationId
               NOTIFY discoverOperationIdChanged)
    Q_PROPERTY(quint64 searchOperationId READ searchOperationId NOTIFY searchOperationIdChanged)
    Q_PROPERTY(quint64 playlistOperationId READ playlistOperationId NOTIFY playlistOperationIdChanged)
    Q_PROPERTY(quint64 myMusicOperationId READ myMusicOperationId NOTIFY myMusicOperationIdChanged)
    Q_PROPERTY(int searchMode READ searchMode WRITE setSearchMode NOTIFY searchModeChanged)
    Q_PROPERTY(QString playlistTitle READ playlistTitle NOTIFY playlistInfoChanged)
    Q_PROPERTY(QString playlistPeriod READ playlistPeriod NOTIFY playlistInfoChanged)
    Q_PROPERTY(QString playlistAuthor READ playlistAuthor NOTIFY playlistInfoChanged)
    Q_PROPERTY(QUrl playlistArtwork READ playlistArtwork NOTIFY playlistInfoChanged)
    Q_PROPERTY(int playlistTrackCount READ playlistTrackCount NOTIFY playlistInfoChanged)
    Q_PROPERTY(QUrl playlistSourceUrl READ playlistSourceUrl NOTIFY playlistInfoChanged)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY sessionChanged)
    Q_PROPERTY(QString nickname READ nickname NOTIFY sessionChanged)
    Q_PROPERTY(QUrl avatar READ avatar NOTIFY sessionChanged)
    Q_PROPERTY(PlaybackController *playback READ playback CONSTANT)
    Q_PROPERTY(SettingsStore *settings READ settings CONSTANT)
    Q_PROPERTY(ImageCache *imageCache READ imageCache CONSTANT)
    Q_PROPERTY(int artworkRevision READ artworkRevision
               NOTIFY artworkRevisionChanged)
    Q_PROPERTY(qint64 cacheUsageBytes READ cacheUsageBytes NOTIFY cacheUsageChanged)
    Q_PROPERTY(qint64 cacheLimitBytes READ cacheLimitBytes NOTIFY cacheLimitBytesChanged)
    Q_PROPERTY(bool authenticationRemovalPending READ authenticationRemovalPending
               NOTIFY authenticationRemovalPendingChanged)
    Q_PROPERTY(QString loginStatus READ loginStatus NOTIFY loginChanged)
    Q_PROPERTY(QString loginDiagnosticCode READ loginDiagnosticCode
               NOTIFY loginChanged)
    Q_PROPERTY(QString loginQrImage READ loginQrImage NOTIFY loginChanged)
    Q_PROPERTY(int loginQrGeneration READ loginQrGeneration NOTIFY loginChanged)
    Q_PROPERTY(bool loginCancellable READ loginCancellable NOTIFY loginChanged)

public:
    enum State {
        Loading = int(PageState::Loading),
        Content = int(PageState::Content),
        Empty = int(PageState::Empty),
        OfflineWithCache = int(PageState::OfflineWithCache),
        Error = int(PageState::Error)
    };
    Q_ENUMS(State)

    enum SearchMode {
        TrackSearch = 0,
        PlaylistSearch = 1
    };
    Q_ENUMS(SearchMode)

    explicit ApplicationController(QObject *parent = 0);
    explicit ApplicationController(QqMusicService *service,
                                   QObject *parent = 0);
    ~ApplicationController() override;

    PlaylistListModel *recommendations() const;
    PlaylistListModel *publicPlaylists() const;
    PlaylistListModel *toplists() const;
    TrackListModel *searchTracks() const;
    PlaylistListModel *searchPlaylists() const;
    TrackListModel *playlistTracks() const;
    PlaylistListModel *likedPlaylists() const;
    PlaylistListModel *createdPlaylists() const;
    PlaylistListModel *collectedPlaylists() const;
    LyricListModel *lyrics() const;
    QVariantList categoryGroups() const;
    QString selectedCategoryId() const;

    int homeState() const;
    int discoverState() const;
    int searchState() const;
    int playlistState() const;
    int myMusicState() const;
    int createdPlaylistsState() const;
    int collectedPlaylistsState() const;
    int lyricState() const;
    quint64 homeOperationId() const;
    quint64 discoverOperationId() const;
    quint64 searchOperationId() const;
    quint64 playlistOperationId() const;
    quint64 myMusicOperationId() const;
    quint64 collectedLibraryOperationId() const;
    int searchMode() const;
    void setSearchMode(int mode);
    QString playlistTitle() const;
    QString playlistPeriod() const;
    QString playlistAuthor() const;
    QUrl playlistArtwork() const;
    int playlistTrackCount() const;
    QUrl playlistSourceUrl() const;
    QString userUin() const;
    void setUserUin(const QString &uin);
    bool loggedIn() const;
    QString nickname() const;
    QUrl avatar() const;
    void setSessionStore(SessionStore *sessionStore);
    void setNativeLoginManager(NativeLoginManager *loginManager);
    PlaybackController *playback() const;
    void setPlaybackController(PlaybackController *playbackController);
    SettingsStore *settings() const;
    ImageCache *imageCache() const;
    int artworkRevision() const;
    void setStorage(SettingsStore *settingsStore, ImageCache *imageCache);
    qint64 cacheUsageBytes() const;
    qint64 cacheLimitBytes() const;
    bool authenticationRemovalPending() const;
    QString loginStatus() const;
    QString loginDiagnosticCode() const;
    QString loginQrImage() const;
    int loginQrGeneration() const;
    bool loginCancellable() const;

    Q_INVOKABLE void refreshHome();
    Q_INVOKABLE void refreshDiscover();
    Q_INVOKABLE void selectCategory(const QString &categoryId);
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void loadMoreSearch();
    Q_INVOKABLE void openPlaylist(const QString &playlistId);
    Q_INVOKABLE void openToplist(const QString &toplistId);
    Q_INVOKABLE bool playPlaylistAt(int index);
    Q_INVOKABLE bool playSearchTrack(int index);
    Q_INVOKABLE bool playQueueIndex(int index);
    Q_INVOKABLE bool playNextPlaylistTrack(int index);
    Q_INVOKABLE bool appendPlaylistTrack(int index);
    Q_INVOKABLE bool playNextSearchTrack(int index);
    Q_INVOKABLE bool appendSearchTrack(int index);
    Q_INVOKABLE bool clearCache();
    Q_INVOKABLE QUrl artworkSource(const QUrl &url);
    Q_INVOKABLE bool requestArtworkForeground(const QUrl &url);
    Q_INVOKABLE void setCacheLimitBytes(qint64 bytes);
    Q_INVOKABLE void refreshMyMusic();
    Q_INVOKABLE void startLogin();
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void confirmExpiredSessionRemoval();

signals:
    void homeStateChanged();
    void discoverStateChanged();
    void searchStateChanged();
    void playlistStateChanged();
    void myMusicStateChanged();
    void createdPlaylistsStateChanged();
    void collectedPlaylistsStateChanged();
    void lyricStateChanged();
    void homeOperationIdChanged();
    void discoverOperationIdChanged();
    void searchOperationIdChanged();
    void playlistOperationIdChanged();
    void myMusicOperationIdChanged();
    void categoryGroupsChanged();
    void selectedCategoryChanged();
    void searchModeChanged();
    void playlistInfoChanged();
    void userUinChanged();
    void accountActionFailed();
    void logoutRequested();
    void sessionChanged();
    void cacheUsageChanged();
    void artworkRevisionChanged();
    void artworkAvailable(const QUrl &remoteUrl, const QUrl &localUrl);
    void artworkForegroundAvailable(const QUrl &remoteUrl, bool lightText);
    void artworkCacheReset();
    void cacheLimitBytesChanged();
    void authenticationRemovalPendingChanged();
    void authenticationExpired();
    void loginChanged();
    void terminalPlaybackFailure(int errorKind,
                                 const QString &trackTitle);

private slots:
    void recommendationsReady(quint64 operationId,
                              const QVector<PlaylistSummary> &items);
    void dailyThirtyReady(quint64 operationId,
                          const QVector<Track> &items);
    void guessYouLikeReady(quint64 operationId,
                           const QVector<Track> &items);
    void categoriesReady(quint64 operationId,
                         const QVector<CategoryGroup> &groups);
    void playlistsReady(quint64 operationId,
                        const QVector<PlaylistSummary> &items,
                        qint64 total);
    void toplistsReady(quint64 operationId,
                       const QVector<PlaylistSummary> &items);
    void tracksReady(quint64 operationId,
                     const QVector<Track> &items,
                     qint64 total);
    void playlistReady(quint64 operationId,
                       const PlaylistDetail &playlist);
    void createdLibraryReady(quint64 operationId,
                             const QVector<PlaylistSummary> &liked,
                             const QVector<PlaylistSummary> &created);
    void collectedLibraryReady(
        quint64 operationId,
        const QVector<PlaylistSummary> &collected);
    void serviceFailed(const ServiceError &error);
    void sessionActivated(const UserProfile &profile);
    void sessionCleared();
    void sessionRemovalPendingChanged();
    void lyricsReady(quint64 operationId, const LyricDocument &lyrics);
    void requestCurrentLyrics();
    void updateLyricPosition();
    void playbackFailed(const ServiceError &error);
    void schedulePersistQueue();
    void persistQueue();

private:
    enum class HomeStage {
        None,
        DailyThirty,
        GuessYouLike,
        Recommendations
    };

    enum class DiscoverStage {
        None,
        Categories,
        Playlists,
        FilteredPlaylists,
        Toplists
    };

    quint64 nextOperationId();
    int activeSearchCount() const;
    bool hasHomeContent() const;
    bool hasDiscoverContent() const;
    void requestHomeDailyThirty();
    void requestHomeGuessYouLike();
    void requestHomePublicRecommendations();
    void updateHomeRecommendations();
    bool openCachedPersonalized(const QString &playlistId);
    void requestDiscoverToplists();
    void finishHome();
    void finishDiscover();
    void setHomeState(PageState state);
    void setDiscoverState(PageState state);
    void setSearchState(PageState state);
    void setPlaylistState(PageState state);
    void setMyMusicState(PageState state);
    void setCreatedPlaylistsState(PageState state);
    void setCollectedPlaylistsState(PageState state);
    void updateMyMusicState();
    void setLyricState(PageState state);
    void clearSessionState(bool notifyLogout);
    void setLoginPresentation(const QString &status,
                              const QString &qrImage,
                              int generation,
                              bool cancellable,
                              const QString &diagnosticCode = QString());

    QPointer<QqMusicService> m_service;
    QPointer<SessionStore> m_sessionStore;
    QPointer<NativeLoginManager> m_loginManager;
    QPointer<PlaybackController> m_playbackController;
    QPointer<SettingsStore> m_settingsStore;
    QPointer<ImageCache> m_imageCache;
    QTimer *m_persistTimer;
    bool m_queueRestored;
    bool m_authenticationRemovalPending;
    QString m_loginStatus;
    QString m_loginDiagnosticCode;
    QString m_loginQrImage;
    QString m_loginAttemptId;
    int m_loginQrGeneration;
    bool m_loginCancellable;
    PlaylistListModel *m_recommendations;
    QVector<PlaylistSummary> m_publicHomeRecommendations;
    PlaylistDetail m_dailyThirty;
    PlaylistDetail m_guessYouLike;
    PlaylistListModel *m_publicPlaylists;
    PlaylistListModel *m_toplists;
    TrackListModel *m_searchTracks;
    PlaylistListModel *m_searchPlaylists;
    TrackListModel *m_playlistTracks;
    PlaylistListModel *m_likedPlaylists;
    PlaylistListModel *m_createdPlaylists;
    PlaylistListModel *m_collectedPlaylists;
    LyricListModel *m_lyrics;
    QVariantList m_categoryGroups;
    QString m_selectedCategoryId;
    PlaylistSummary m_playlistInfo;
    QString m_playlistPeriod;
    PageState m_homeState;
    PageState m_discoverState;
    PageState m_searchState;
    PageState m_playlistState;
    PageState m_myMusicState;
    PageState m_createdPlaylistsState;
    PageState m_collectedPlaylistsState;
    PageState m_lyricState;
    quint64 m_nextOperationId;
    quint64 m_homeOperationId;
    quint64 m_homeRefreshSequence;
    quint64 m_discoverOperationId;
    quint64 m_searchOperationId;
    quint64 m_playlistOperationId;
    quint64 m_myMusicOperationId;
    quint64 m_collectedLibraryOperationId;
    quint64 m_lyricOperationId;
    QString m_searchQuery;
    QString m_searchAccountUin;
    QString m_playlistId;
    QString m_userUin;
    QString m_nickname;
    QUrl m_avatar;
    bool m_loggedIn;
    int m_searchPage;
    qint64 m_searchTotal;
    bool m_searchAppend;
    int m_searchMode;
    HomeStage m_homeStage;
    DiscoverStage m_discoverStage;
};

void registerApplicationControllerTypes();

#endif
