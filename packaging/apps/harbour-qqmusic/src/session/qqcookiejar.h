#ifndef QQCOOKIEJAR_H
#define QQCOOKIEJAR_H

#include "cookiepolicy.h"

#include <QNetworkCookieJar>

class QqCookieJar : public QNetworkCookieJar
{
    Q_OBJECT

public:
    explicit QqCookieJar(QObject *parent = 0);

    bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList,
                           const QUrl &url) override;
    void replaceRecords(const QVector<CookieRecord> &records);
    void clearSession();
    bool isEmpty() const;
};

#endif
