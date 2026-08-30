#include "qqcookiejar.h"

#include <QDateTime>
#include <QNetworkCookie>

namespace {

CookieRecord fromNetworkCookie(const QNetworkCookie &cookie)
{
    CookieRecord record;
    record.name = cookie.name();
    record.value = cookie.value();
    record.domain = cookie.domain();
    record.path = cookie.path();
    record.secure = cookie.isSecure();
    record.expiresAt = cookie.expirationDate();
    return record;
}

QNetworkCookie toNetworkCookie(const CookieRecord &record)
{
    QNetworkCookie cookie(record.name, record.value);
    cookie.setDomain(record.domain);
    cookie.setPath(record.path);
    cookie.setSecure(record.secure);
    cookie.setExpirationDate(record.expiresAt.toUTC());
    return cookie;
}

}

QqCookieJar::QqCookieJar(QObject *parent)
    : QNetworkCookieJar(parent)
{
}

bool QqCookieJar::setCookiesFromUrl(
    const QList<QNetworkCookie> &cookieList, const QUrl &url)
{
    QList<QNetworkCookie> accepted;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (int index = 0; index < cookieList.size(); ++index) {
        if (CookiePolicy::validate(fromNetworkCookie(cookieList.at(index)), now).ok()) {
            accepted.append(cookieList.at(index));
        }
    }
    return !accepted.isEmpty()
        && QNetworkCookieJar::setCookiesFromUrl(accepted, url);
}

void QqCookieJar::replaceRecords(const QVector<CookieRecord> &records)
{
    QList<QNetworkCookie> cookies;
    for (int index = 0; index < records.size(); ++index) {
        cookies.append(toNetworkCookie(records.at(index)));
    }
    setAllCookies(cookies);
}

void QqCookieJar::clearSession()
{
    setAllCookies(QList<QNetworkCookie>());
}

bool QqCookieJar::isEmpty() const
{
    return allCookies().isEmpty();
}
