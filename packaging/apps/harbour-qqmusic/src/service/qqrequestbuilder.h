#ifndef QQREQUESTBUILDER_H
#define QQREQUESTBUILDER_H

#include <QByteArray>
#include <QString>
#include <QUrl>

enum class SearchKind {
    Tracks,
    Playlists
};

class QqRequestBuilder
{
public:
    static QUrl recommendationsUrl();
    static QUrl categoriesUrl();
    static QUrl playlistsUrl(const QString &categoryId, int offset);
    static QUrl toplistsUrl();
    static QUrl toplistPeriodUrl();
    static QUrl toplistUrl(int topId, const QString &period, int limit);
    static QUrl playlistUrl(const QString &numericId);
    static QUrl personalizedUrl();
    static QByteArray dailyThirtyBody(const QString &accountUin,
                                      const QByteArray &musicKey,
                                      int page);
    static QByteArray guessYouLikeBody(const QString &accountUin,
                                       const QByteArray &musicKey);
    static QUrl userUrl(const QString &uin);
    static QUrl userCreatedUrl(const QString &uin);
    static QUrl userCollectedUrl(const QString &uin);
    static QUrl lyricUrl(const QString &trackMid);
    static QUrl vkeyUrl();
    static QByteArray vkeyBody(const QString &trackMid,
                               const QString &accountUin = QString());
    static QByteArray searchBody(const QString &query,
                                 SearchKind kind,
                                 int page,
                                 const QString &accountUin = QString());
};

#endif
