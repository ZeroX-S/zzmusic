#include <QtTest>

#include "session/cookiepolicy.h"
#include "session/qqcookiejar.h"

#include <QNetworkCookie>

namespace {

CookieRecord record(const QByteArray &name = "uin",
                    const QByteArray &value = "o10001",
                    const QString &domain = QStringLiteral(".qq.com"),
                    const QString &path = QStringLiteral("/"))
{
    CookieRecord cookie;
    cookie.name = name;
    cookie.value = value;
    cookie.domain = domain;
    cookie.path = path;
    cookie.secure = true;
    cookie.expiresAt = QDateTime::currentDateTimeUtc().addDays(30);
    return cookie;
}

}

class CookiePolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void allowsOnlyReviewedNamesAndDomains()
    {
        QVERIFY(CookiePolicy::isAllowedName("uin"));
        QVERIFY(CookiePolicy::isAllowedName("wxuin"));
        QVERIFY(CookiePolicy::isAllowedName("qm_keyst"));
        QVERIFY(CookiePolicy::isAllowedName("qqmusic_key"));
        QVERIFY(CookiePolicy::isAllowedName("p_skey"));
        QVERIFY(CookiePolicy::isAllowedName("skey"));
        QVERIFY(CookiePolicy::isAllowedName("pt2gguin"));
        QVERIFY(!CookiePolicy::isAllowedName("password"));

        const QDateTime now = QDateTime::currentDateTimeUtc();
        const QStringList domains = QStringList()
            << ".qq.com" << "qq.com" << ".y.qq.com" << "y.qq.com"
            << ".ptlogin2.qq.com" << "ptlogin2.qq.com";
        for (int index = 0; index < domains.size(); ++index) {
            QVERIFY(CookiePolicy::validate(
                record("uin", "o10001", domains.at(index)), now).ok());
        }
        QVERIFY(!CookiePolicy::validate(
            record("uin", "o10001", ".evil.invalid"), now).ok());
        QVERIFY(!CookiePolicy::validate(
            record("uin", "o10001", ".sub.qq.com"), now).ok());
    }

    void rejectsInvalidCookieFields()
    {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        CookieRecord cookie = record();
        cookie.value.clear();
        QVERIFY(!CookiePolicy::validate(cookie, now).ok());
        cookie = record("uin", QByteArray(4097, 'x'));
        QVERIFY(!CookiePolicy::validate(cookie, now).ok());
        cookie = record("uin", QByteArray("x\nvalue"));
        QVERIFY(!CookiePolicy::validate(cookie, now).ok());
        cookie = record();
        cookie.secure = false;
        QVERIFY(!CookiePolicy::validate(cookie, now).ok());
        cookie = record();
        cookie.path = QStringLiteral("relative");
        QVERIFY(!CookiePolicy::validate(cookie, now).ok());
        cookie = record();
        cookie.expiresAt = now.addSecs(-1);
        const CookieValidation expired = CookiePolicy::validate(cookie, now);
        QVERIFY(!expired.ok());
        QCOMPARE(expired.error.kind, ErrorKind::AuthenticationExpired);
        cookie = record();
        cookie.expiresAt = now.addDays(401);
        QVERIFY(!CookiePolicy::validate(cookie, now).ok());
    }

    void rejectsDuplicatesAndOversizedPayloads()
    {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        QVector<CookieRecord> duplicate;
        duplicate << record() << record();
        QVERIFY(!CookiePolicy::validateAll(duplicate, now).ok());

        QVector<CookieRecord> oversized;
        for (int index = 0; index < 8; ++index) {
            oversized.append(record(
                "uin", QByteArray(4096, char('a' + index)),
                QStringLiteral(".qq.com"),
                QStringLiteral("/%1").arg(index)));
        }
        QVERIFY(CookiePolicy::serialize(oversized).size()
                > CookiePolicy::maximumPayloadBytes());
        QVERIFY(!CookiePolicy::validateAll(oversized, now).ok());
    }

    void roundTripsStrictVersionedPayload()
    {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        QVector<CookieRecord> records;
        records << record() << record("qm_keyst", "sanitized-key");
        const QByteArray payload = CookiePolicy::serialize(records);
        const ServiceResult<QVector<CookieRecord> > decoded =
            CookiePolicy::deserialize(payload, now);
        QVERIFY(decoded.ok());
        QCOMPARE(decoded.value.size(), 2);
        QCOMPARE(decoded.value.at(1).name, QByteArray("qm_keyst"));
        QCOMPARE(decoded.value.at(1).value, QByteArray("sanitized-key"));

        QVERIFY(!CookiePolicy::deserialize(QByteArray("{}"), now).ok());
        QVERIFY(!CookiePolicy::deserialize(
            QByteArray(CookiePolicy::maximumPayloadBytes() + 1, 'x'), now).ok());
        QVERIFY(!CookiePolicy::deserialize(
            QByteArray("{\"version\":2,\"cookies\":[]}"), now).ok());

        QByteArray nonCanonical = payload;
        QVERIFY(nonCanonical.contains("\"name\":\"dWlu\""));
        nonCanonical.replace("\"name\":\"dWlu\"",
                             "\"name\":\"dWlu=\"");
        QVERIFY(!CookiePolicy::deserialize(nonCanonical, now).ok());
        QByteArray malformed = payload;
        QVERIFY(malformed.contains("\"name\":\"dWlu\""));
        malformed.replace("\"name\":\"dWlu\"",
                          "\"name\":\"dWlu*\"");
        QVERIFY(!CookiePolicy::deserialize(malformed, now).ok());
    }

    void cookieJarRetainsOnlyAcceptedRecords()
    {
        QqCookieJar jar;
        QNetworkCookie accepted("uin", "o10001");
        accepted.setDomain(".qq.com");
        accepted.setPath("/");
        accepted.setSecure(true);
        accepted.setExpirationDate(QDateTime::currentDateTimeUtc().addDays(1));
        QNetworkCookie rejected("password", "secret");
        rejected.setDomain(".qq.com");
        rejected.setPath("/");
        rejected.setSecure(true);
        rejected.setExpirationDate(QDateTime::currentDateTimeUtc().addDays(1));

        QVERIFY(jar.setCookiesFromUrl(
            QList<QNetworkCookie>() << accepted << rejected,
            QUrl("https://y.qq.com/")));
        const QList<QNetworkCookie> cookies =
            jar.cookiesForUrl(QUrl("https://y.qq.com/"));
        QCOMPARE(cookies.size(), 1);
        QCOMPARE(cookies.first().name(), QByteArray("uin"));
        jar.clearSession();
        QVERIFY(jar.isEmpty());
    }
};

QTEST_GUILESS_MAIN(CookiePolicyTest)
#include "tst_cookiepolicy.moc"
