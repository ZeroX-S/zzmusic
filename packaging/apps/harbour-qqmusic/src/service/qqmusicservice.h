#ifndef QQMUSICSERVICE_H
#define QQMUSICSERVICE_H

#include "core/types.h"
#include "qqrequestpolicy.h"

#include <QByteArray>
#include <QHash>
#include <QObject>

class QNetworkAccessManager;
class QNetworkDiskCache;
class QqNetworkTransport;

class QqMusicService : public QObject
{
    Q_OBJECT

public:
    explicit QqMusicService(QObject *parent = 0);
    explicit QqMusicService(QNetworkAccessManager *manager,
                            QObject *parent = 0);
    ~QqMusicService();

    QNetworkDiskCache *configurePublicCache(const QString &directory,
                                            qint64 maximumBytes);
    qint64 publicCacheSize() const;
    void setPublicCacheMaximumBytes(qint64 maximumBytes);
    bool clearPublicCache();

    Q_INVOKABLE virtual bool requestRecommendations(quint64 operationId);
    Q_INVOKABLE virtual bool requestDailyThirty(quint64 operationId);
    Q_INVOKABLE virtual bool requestGuessYouLike(quint64 operationId);
    Q_INVOKABLE virtual bool requestCategories(quint64 operationId);
    Q_INVOKABLE virtual bool requestPlaylists(quint64 operationId,
                                              const QString &categoryId,
                                              int offset);
    Q_INVOKABLE virtual bool requestToplists(quint64 operationId);
    Q_INVOKABLE virtual bool requestToplist(quint64 operationId,
                                            const QString &toplistId);
    Q_INVOKABLE virtual bool searchTracks(quint64 operationId,
                                          const QString &query,
                                          int page,
                                          const QString &accountUin = QString());
    Q_INVOKABLE virtual bool searchPlaylists(quint64 operationId,
                                             const QString &query,
                                             int page,
                                             const QString &accountUin = QString());
    Q_INVOKABLE virtual bool requestPlaylist(quint64 operationId,
                                             const QString &playlistId,
                                             bool containsCookies = false);
    Q_INVOKABLE virtual bool requestUser(quint64 operationId,
                                         const QString &uin);
    Q_INVOKABLE virtual bool requestCreatedLibrary(quint64 operationId,
                                                   const QString &uin);
    Q_INVOKABLE virtual bool requestCollectedLibrary(quint64 operationId,
                                                     const QString &uin);
    Q_INVOKABLE virtual bool requestLyrics(quint64 operationId,
                                           const QString &trackMid);
    Q_INVOKABLE virtual bool resolvePlayback(quint64 operationId,
                                             const QString &trackMid,
                                             const QString &accountUin = QString());
    Q_INVOKABLE virtual bool cancel(quint64 operationId);

signals:
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
    void playlistReady(quint64 operationId, const PlaylistDetail &playlist);
    void userReady(quint64 operationId, const UserProfile &user);
    void createdLibraryReady(quint64 operationId,
                             const QVector<PlaylistSummary> &liked,
                             const QVector<PlaylistSummary> &created);
    void collectedLibraryReady(
        quint64 operationId,
        const QVector<PlaylistSummary> &collected);
    void lyricsReady(quint64 operationId, const LyricDocument &lyrics);
    void playbackReady(quint64 operationId,
                       const PlaybackLocation &location);
    void failed(const ServiceError &error);

private:
    struct Pending;

    bool beginGet(Pending *pending,
                  const QUrl &url,
                  QqRequestKind requestKind,
                  bool containsCookies,
                  bool forceNetwork = false);
    bool beginPost(Pending *pending,
                   const QUrl &url,
                   QqRequestKind requestKind,
                   const QByteArray &body,
                   bool containsCookies);
    void complete(quint64 operationId, const QByteArray &body);
    void transportFailed(const ServiceError &error);
    void emitInternalFailure(quint64 operationId,
                             const QString &diagnosticCode);
    Pending *takePending(quint64 operationId);

    QqNetworkTransport *m_transport;
    QHash<quint64, Pending *> m_pending;
};

#endif
