#include "qqrequestpolicy.h"

namespace {

const int MaxFollowedRedirects = 4;

bool hasForbiddenAuthoritySyntax(const QUrl &url)
{
    const QByteArray encoded = url.toEncoded(QUrl::FullyEncoded);
    const int schemeEnd = encoded.indexOf("//");
    if (schemeEnd < 0) {
        return true;
    }
    const int authorityStart = schemeEnd + 2;
    int authorityEnd = encoded.size();
    const int slash = encoded.indexOf('/', authorityStart);
    const int query = encoded.indexOf('?', authorityStart);
    const int fragment = encoded.indexOf('#', authorityStart);
    if (slash >= 0 && slash < authorityEnd) {
        authorityEnd = slash;
    }
    if (query >= 0 && query < authorityEnd) {
        authorityEnd = query;
    }
    if (fragment >= 0 && fragment < authorityEnd) {
        authorityEnd = fragment;
    }

    const QByteArray authority = encoded.mid(authorityStart,
                                              authorityEnd - authorityStart);
    if (authority.contains('@')) {
        return true;
    }
    const int colon = authority.lastIndexOf(':');
    if (colon >= 0) {
        const QByteArray port = authority.mid(colon + 1);
        if (port.isEmpty() || port != "443") {
            return true;
        }
    }
    return false;
}

bool isSafeHttpsUrl(const QUrl &url)
{
    return url.isValid()
        && !url.isEmpty()
        && url.scheme() == QStringLiteral("https")
        && !url.host().isEmpty()
        && !url.hasFragment()
        && !hasForbiddenAuthoritySyntax(url);
}

bool hostIsOneOf(const QString &host, const char *const *hosts, int count)
{
    for (int index = 0; index < count; ++index) {
        if (host == QLatin1String(hosts[index])) {
            return true;
        }
    }
    return false;
}

bool hasReviewedRawAuthority(const QString &urlText,
                             const char *const *hosts,
                             int count)
{
    const int separator = urlText.indexOf(QStringLiteral("://"));
    if (separator < 0
            || urlText.left(separator).compare(QStringLiteral("https"),
                                               Qt::CaseInsensitive) != 0) {
        return false;
    }
    const int authorityStart = separator + 3;
    int authorityEnd = urlText.size();
    const QString delimiters = QStringLiteral("/?#");
    for (int index = 0; index < delimiters.size(); ++index) {
        const int position = urlText.indexOf(delimiters.at(index),
                                             authorityStart);
        if (position >= 0 && position < authorityEnd) {
            authorityEnd = position;
        }
    }
    const QString authority = urlText.mid(authorityStart,
                                          authorityEnd - authorityStart);
    for (int index = 0; index < count; ++index) {
        const QString host = QLatin1String(hosts[index]);
        if (authority.compare(host, Qt::CaseInsensitive) == 0
                || authority.compare(host + QStringLiteral(":443"),
                                     Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool isApiKind(QqRequestKind kind)
{
    return kind == QqRequestKind::PublicMetadata
        || kind == QqRequestKind::Search
        || kind == QqRequestKind::AuthenticatedAccount
        || kind == QqRequestKind::Vkey
        || kind == QqRequestKind::Lyric;
}

}

bool QqRequestPolicy::allowsApi(const QUrl &url)
{
    static const char *const hosts[] = {
        "u.y.qq.com",
        "c.y.qq.com",
        "i.y.qq.com"
    };
    return isSafeHttpsUrl(url)
        && hostIsOneOf(url.host(), hosts, sizeof(hosts) / sizeof(hosts[0]));
}

bool QqRequestPolicy::allowsApi(const QString &urlText)
{
    static const char *const hosts[] = {
        "u.y.qq.com",
        "c.y.qq.com",
        "i.y.qq.com"
    };
    return hasReviewedRawAuthority(urlText,
                                   hosts,
                                   sizeof(hosts) / sizeof(hosts[0]))
        && allowsApi(QUrl(urlText, QUrl::StrictMode));
}

bool QqRequestPolicy::allowsArtwork(const QUrl &url)
{
    static const char *const hosts[] = {
        "y.gtimg.cn",
        "qpic.y.qq.com",
        "p.qpic.cn",
        "music-file.y.qq.com",
        "y.qq.com",
        "i.gtimg.cn",
        "thirdqq.qlogo.cn"
    };
    return isSafeHttpsUrl(url)
        && hostIsOneOf(url.host(), hosts, sizeof(hosts) / sizeof(hosts[0]));
}

bool QqRequestPolicy::allowsArtwork(const QString &urlText)
{
    static const char *const hosts[] = {
        "y.gtimg.cn",
        "qpic.y.qq.com",
        "p.qpic.cn",
        "music-file.y.qq.com",
        "y.qq.com",
        "i.gtimg.cn",
        "thirdqq.qlogo.cn"
    };
    return hasReviewedRawAuthority(urlText,
                                   hosts,
                                   sizeof(hosts) / sizeof(hosts[0]))
        && allowsArtwork(QUrl(urlText, QUrl::StrictMode));
}

bool QqRequestPolicy::allowsMedia(const QUrl &url)
{
    static const char *const hosts[] = {
        "ws.stream.qqmusic.qq.com",
        "isure.stream.qqmusic.qq.com",
        "dl.stream.qqmusic.qq.com",
        "aqqmusic.tc.qq.com"
    };
    return isSafeHttpsUrl(url)
        && hostIsOneOf(url.host(), hosts, sizeof(hosts) / sizeof(hosts[0]));
}

bool QqRequestPolicy::allowsMedia(const QString &urlText)
{
    static const char *const hosts[] = {
        "ws.stream.qqmusic.qq.com",
        "isure.stream.qqmusic.qq.com",
        "dl.stream.qqmusic.qq.com",
        "aqqmusic.tc.qq.com"
    };
    return hasReviewedRawAuthority(urlText,
                                   hosts,
                                   sizeof(hosts) / sizeof(hosts[0]))
        && allowsMedia(QUrl(urlText, QUrl::StrictMode));
}

bool QqRequestPolicy::allows(QqRequestKind kind, const QUrl &url)
{
    if (isApiKind(kind)) {
        return allowsApi(url);
    }
    if (kind == QqRequestKind::Artwork) {
        return allowsArtwork(url);
    }
    return kind == QqRequestKind::Media && allowsMedia(url);
}

bool QqRequestPolicy::allows(QqRequestKind kind, const QString &urlText)
{
    if (isApiKind(kind)) {
        return allowsApi(urlText);
    }
    if (kind == QqRequestKind::Artwork) {
        return allowsArtwork(urlText);
    }
    return kind == QqRequestKind::Media && allowsMedia(urlText);
}

bool QqRequestPolicy::allowsRedirect(QqRequestKind kind,
                                     const QUrl &url,
                                     int redirectCount)
{
    return redirectCount >= 1
        && redirectCount <= MaxFollowedRedirects
        && allows(kind, url);
}

bool QqRequestPolicy::allowsRedirect(const QUrl &url, int redirectCount)
{
    return redirectCount >= 1
        && redirectCount <= MaxFollowedRedirects
        && (allowsApi(url) || allowsArtwork(url) || allowsMedia(url));
}

bool QqRequestPolicy::allowsRedirect(QqRequestKind kind,
                                     const QString &urlText,
                                     int redirectCount)
{
    return redirectCount >= 1
        && redirectCount <= MaxFollowedRedirects
        && allows(kind, urlText);
}

QNetworkRequest QqRequestPolicy::prepareRequest(QqRequestKind kind,
                                                const QUrl &url,
                                                bool isGet,
                                                bool containsCookies)
{
    QNetworkRequest request(url);
    if (isApiKind(kind)) {
        request.setRawHeader("Referer", "https://y.qq.com/");
        request.setRawHeader("Origin", "https://y.qq.com");
        request.setRawHeader("Accept", "application/json, text/plain, */*");
    }
    if (!isGet && isApiKind(kind)) {
        request.setRawHeader("Content-Type", "application/json");
    }

    const bool cacheable = kind == QqRequestKind::PublicMetadata
        && isGet
        && !containsCookies;
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         cacheable ? QNetworkRequest::PreferCache
                                   : QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute,
                         cacheable);
    const QNetworkRequest::LoadControl cookieControl = containsCookies
        ? QNetworkRequest::Automatic
        : QNetworkRequest::Manual;
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,
                         cookieControl);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,
                         cookieControl);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
#endif
    return request;
}

bool QqRequestPolicy::allowsResponseCache(
    const QList<QPair<QByteArray, QByteArray> > &rawHeaders)
{
    for (int index = 0; index < rawHeaders.size(); ++index) {
        if (rawHeaders.at(index).first.toLower() == "set-cookie") {
            return false;
        }
    }
    return true;
}
