#ifndef QQREQUESTPOLICY_H
#define QQREQUESTPOLICY_H

#include <QList>
#include <QNetworkRequest>
#include <QPair>
#include <QUrl>

enum class QqRequestKind {
    PublicMetadata,
    Search,
    AuthenticatedAccount,
    Vkey,
    Lyric,
    Artwork,
    Media
};

class QqRequestPolicy
{
public:
    static bool allowsApi(const QUrl &url);
    static bool allowsApi(const QString &urlText);
    static bool allowsArtwork(const QUrl &url);
    static bool allowsArtwork(const QString &urlText);
    static bool allowsMedia(const QUrl &url);
    static bool allowsMedia(const QString &urlText);
    static bool allows(QqRequestKind kind, const QUrl &url);
    static bool allows(QqRequestKind kind, const QString &urlText);
    static bool allowsRedirect(QqRequestKind kind,
                               const QUrl &url,
                               int redirectCount);
    static bool allowsRedirect(const QUrl &url, int redirectCount);
    static bool allowsRedirect(QqRequestKind kind,
                               const QString &urlText,
                               int redirectCount);

    static QNetworkRequest prepareRequest(QqRequestKind kind,
                                          const QUrl &url,
                                          bool isGet,
                                          bool containsCookies);
    static bool allowsResponseCache(
        const QList<QPair<QByteArray, QByteArray> > &rawHeaders);
};

#endif
