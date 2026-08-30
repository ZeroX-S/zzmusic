#include <QtTest>

#include "service/qqrequestpolicy.h"

#include <QNetworkRequest>

class QqRequestPolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void acceptsExactReviewedHosts_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<QString>("host");

        QTest::newRow("api-u")
            << int(QqRequestKind::PublicMetadata) << "u.y.qq.com";
        QTest::newRow("api-c")
            << int(QqRequestKind::Search) << "c.y.qq.com";
        QTest::newRow("api-i")
            << int(QqRequestKind::Lyric) << "i.y.qq.com";
        QTest::newRow("artwork-gtimg")
            << int(QqRequestKind::Artwork) << "y.gtimg.cn";
        QTest::newRow("artwork-qpic")
            << int(QqRequestKind::Artwork) << "qpic.y.qq.com";
        QTest::newRow("artwork-p-qpic")
            << int(QqRequestKind::Artwork) << "p.qpic.cn";
        QTest::newRow("artwork-music-file")
            << int(QqRequestKind::Artwork) << "music-file.y.qq.com";
        QTest::newRow("artwork-y-qq")
            << int(QqRequestKind::Artwork) << "y.qq.com";
        QTest::newRow("artwork-i-gtimg")
            << int(QqRequestKind::Artwork) << "i.gtimg.cn";
        QTest::newRow("artwork-qlogo")
            << int(QqRequestKind::Artwork) << "thirdqq.qlogo.cn";
        QTest::newRow("media-ws")
            << int(QqRequestKind::Media) << "ws.stream.qqmusic.qq.com";
        QTest::newRow("media-isure")
            << int(QqRequestKind::Media) << "isure.stream.qqmusic.qq.com";
        QTest::newRow("media-dl")
            << int(QqRequestKind::Media) << "dl.stream.qqmusic.qq.com";
        QTest::newRow("media-tc")
            << int(QqRequestKind::Media) << "aqqmusic.tc.qq.com";
    }

    void acceptsExactReviewedHosts()
    {
        QFETCH(int, kind);
        QFETCH(QString, host);
        const QUrl url(QStringLiteral("https://%1/resource").arg(host));
        QVERIFY(QqRequestPolicy::allows(static_cast<QqRequestKind>(kind), url));
        QVERIFY(QqRequestPolicy::allows(static_cast<QqRequestKind>(kind),
                                        url.toString()));
    }

    void rejectsArtworkHostLookalikes_data()
    {
        QTest::addColumn<QString>("lookalike");

        QTest::newRow("music-file")
            << "https://music-file.y.qq.com.evil.invalid/cover.jpg";
        QTest::newRow("y-qq")
            << "https://y.qq.com.evil.invalid/cover.jpg";
        QTest::newRow("i-gtimg")
            << "https://i.gtimg.cn.evil.invalid/cover.jpg";
    }

    void rejectsArtworkHostLookalikes()
    {
        QFETCH(QString, lookalike);
        QVERIFY(!QqRequestPolicy::allowsArtwork(QUrl(lookalike)));
        QVERIFY(!QqRequestPolicy::allowsArtwork(lookalike));
    }

    void rejectsMalformedAndUnreviewedUrls_data()
    {
        QTest::addColumn<QString>("url");

        QTest::newRow("lookalike") << "https://u.y.qq.com.evil.invalid/x";
        QTest::newRow("ip-v4") << "https://127.0.0.1/x";
        QTest::newRow("ip-v6") << "https://[::1]/x";
        QTest::newRow("userinfo") << "https://user@u.y.qq.com/x";
        QTest::newRow("empty-userinfo") << "https://@u.y.qq.com/x";
        QTest::newRow("password") << "https://user:pass@u.y.qq.com/x";
        QTest::newRow("http") << "http://u.y.qq.com/x";
        QTest::newRow("ftp") << "ftp://u.y.qq.com/x";
        QTest::newRow("other-port") << "https://u.y.qq.com:444/x";
        QTest::newRow("empty-port") << "https://u.y.qq.com:/x";
        QTest::newRow("fragment") << "https://u.y.qq.com/x#fragment";
        QTest::newRow("empty-fragment") << "https://u.y.qq.com/x#";
    }

    void rejectsMalformedAndUnreviewedUrls()
    {
        QFETCH(QString, url);
        QVERIFY(!QqRequestPolicy::allowsApi(url));
    }

    void allowsExplicitHttpsPort()
    {
        QVERIFY(QqRequestPolicy::allowsApi(
            QUrl(QStringLiteral("https://u.y.qq.com:443/cgi-bin/musicu.fcg"))));
    }

    void keepsRedirectsWithinRequestFamily()
    {
        QVERIFY(QqRequestPolicy::allowsRedirect(
            QqRequestKind::PublicMetadata,
            QUrl(QStringLiteral("https://c.y.qq.com/next")), 4));
        QVERIFY(!QqRequestPolicy::allowsRedirect(
            QqRequestKind::PublicMetadata,
            QUrl(QStringLiteral("https://c.y.qq.com/next")), 5));
        QVERIFY(!QqRequestPolicy::allowsRedirect(
            QqRequestKind::PublicMetadata,
            QUrl(QStringLiteral("https://y.gtimg.cn/next")), 1));
        QVERIFY(!QqRequestPolicy::allowsRedirect(
            QqRequestKind::Media,
            QUrl(QStringLiteral("http://ws.stream.qqmusic.qq.com/x")), 1));
        QVERIFY(!QqRequestPolicy::allowsRedirect(
            QqRequestKind::Media,
            QUrl(QStringLiteral("https://ws.stream.qqmusic.qq.com/x")), 6));
    }

    void appliesReviewedHeadersAndCachePolicy()
    {
        QNetworkRequest publicRequest = QqRequestPolicy::prepareRequest(
            QqRequestKind::PublicMetadata,
            QUrl(QStringLiteral("https://u.y.qq.com/public")), true, false);
        QCOMPARE(publicRequest.rawHeader("Referer"), QByteArray("https://y.qq.com/"));
        QCOMPARE(publicRequest.rawHeader("Origin"), QByteArray("https://y.qq.com"));
        QCOMPARE(publicRequest.rawHeader("Accept"),
                 QByteArray("application/json, text/plain, */*"));
        QCOMPARE(publicRequest.attribute(QNetworkRequest::CacheLoadControlAttribute).toInt(),
                 int(QNetworkRequest::PreferCache));
        QCOMPARE(publicRequest.attribute(QNetworkRequest::CacheSaveControlAttribute).toBool(),
                 true);
        QCOMPARE(publicRequest.attribute(QNetworkRequest::CookieLoadControlAttribute).toInt(),
                 int(QNetworkRequest::Manual));
        QCOMPARE(publicRequest.attribute(QNetworkRequest::CookieSaveControlAttribute).toInt(),
                 int(QNetworkRequest::Manual));
        QVERIFY(!publicRequest.hasRawHeader("Content-Type"));

        const QNetworkRequest searchRequest = QqRequestPolicy::prepareRequest(
            QqRequestKind::Search,
            QUrl(QStringLiteral("https://u.y.qq.com/search")), false, false);
        QCOMPARE(searchRequest.attribute(QNetworkRequest::CacheLoadControlAttribute).toInt(),
                 int(QNetworkRequest::AlwaysNetwork));
        QCOMPARE(searchRequest.attribute(QNetworkRequest::CacheSaveControlAttribute).toBool(),
                 false);
        QCOMPARE(searchRequest.rawHeader("Content-Type"),
                 QByteArray("application/json"));
        QCOMPARE(searchRequest.attribute(QNetworkRequest::CookieLoadControlAttribute).toInt(),
                 int(QNetworkRequest::Manual));
        QCOMPARE(searchRequest.attribute(QNetworkRequest::CookieSaveControlAttribute).toInt(),
                 int(QNetworkRequest::Manual));

        const QNetworkRequest lyricRequest = QqRequestPolicy::prepareRequest(
            QqRequestKind::Lyric,
            QUrl(QStringLiteral("https://i.y.qq.com/lyric")), true, false);
        QCOMPARE(lyricRequest.rawHeader("Referer"), QByteArray("https://y.qq.com/"));
        QCOMPARE(lyricRequest.attribute(QNetworkRequest::CacheLoadControlAttribute).toInt(),
                 int(QNetworkRequest::AlwaysNetwork));

        const QNetworkRequest cookieRequest = QqRequestPolicy::prepareRequest(
            QqRequestKind::PublicMetadata,
            QUrl(QStringLiteral("https://u.y.qq.com/public")), true, true);
        QCOMPARE(cookieRequest.attribute(QNetworkRequest::CacheLoadControlAttribute).toInt(),
                 int(QNetworkRequest::AlwaysNetwork));
        QCOMPARE(cookieRequest.attribute(QNetworkRequest::CacheSaveControlAttribute).toBool(),
                 false);
        QCOMPARE(cookieRequest.attribute(QNetworkRequest::CookieLoadControlAttribute).toInt(),
                 int(QNetworkRequest::Automatic));
        QCOMPARE(cookieRequest.attribute(QNetworkRequest::CookieSaveControlAttribute).toInt(),
                 int(QNetworkRequest::Automatic));

        const QNetworkRequest vkeyPost = QqRequestPolicy::prepareRequest(
            QqRequestKind::Vkey,
            QUrl(QStringLiteral("https://u.y.qq.com/vkey")), false, false);
        QCOMPARE(vkeyPost.rawHeader("Content-Type"),
                 QByteArray("application/json"));

        const QNetworkRequest vkeyGet = QqRequestPolicy::prepareRequest(
            QqRequestKind::Vkey,
            QUrl(QStringLiteral("https://u.y.qq.com/vkey")), true, false);
        QVERIFY(!vkeyGet.hasRawHeader("Content-Type"));
    }
};

QTEST_APPLESS_MAIN(QqRequestPolicyTest)
#include "tst_qqrequestpolicy.moc"
