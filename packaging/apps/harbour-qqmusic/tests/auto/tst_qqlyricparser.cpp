#include <QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

#include <algorithm>

#include "service/qqlyricparser.h"
#include "service/qqrequestbuilder.h"

namespace {

QByteArray fixture(const QString &name)
{
    const QByteArray relativePath =
        (QStringLiteral("../fixtures/") + name).toUtf8();
    QFile file(QFINDTESTDATA(relativePath.constData()));
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

typedef QPair<QString, QString> QueryItem;

bool queryItemLessThan(const QueryItem &left, const QueryItem &right)
{
    return left.first < right.first
        || (left.first == right.first && left.second < right.second);
}

QList<QueryItem> decodedQuery(const QUrl &url)
{
    QList<QueryItem> result =
        QUrlQuery(url).queryItems(QUrl::FullyDecoded);
    std::sort(result.begin(), result.end(), queryItemLessThan);
    return result;
}

void sortQueryItems(QList<QueryItem> *items)
{
    std::sort(items->begin(), items->end(), queryItemLessThan);
}

}

class QqLyricParserTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesTimestampPrecisionAndFindsCurrentLine();
    void preservesTimestampLikeLyricText();
    void ignoresMetadataSortsStablyAndMergesDuplicateTimes();
    void alignsExactTranslationsFromSanitizedFixture();
    void preservesValidEmptyLyrics();
    void rejectsChangedLyricSchemas();
    void buildsPinnedLyricRequest();
    void rejectsUnsafeLyricRequestIdentifiers();
};

void QqLyricParserTest::parsesTimestampPrecisionAndFindsCurrentLine()
{
    QCOMPARE(QqLyricParser::parseTimestamp(QStringLiteral("03:12.345")),
             qint64(192345));
    QCOMPARE(QqLyricParser::parseTimestamp(QStringLiteral("00:01.20")),
             qint64(1200));
    QCOMPARE(QqLyricParser::parseTimestamp(QStringLiteral("00:01.2")),
             qint64(1200));
    QCOMPARE(QqLyricParser::parseTimestamp(QStringLiteral("ar:Artist")),
             qint64(-1));

    QCOMPARE(QqLyricParser::currentIndex(QVector<LyricLine>(), 1000), -1);

    QVector<LyricLine> lines;
    LyricLine first;
    first.timeMs = 1000;
    lines.append(first);
    LyricLine second;
    second.timeMs = 2000;
    lines.append(second);

    QCOMPARE(QqLyricParser::currentIndex(lines, 999), -1);
    QCOMPARE(QqLyricParser::currentIndex(lines, 1000), 0);
    QCOMPARE(QqLyricParser::currentIndex(lines, 1500), 0);
    QCOMPARE(QqLyricParser::currentIndex(lines, 2000), 1);
    QCOMPARE(QqLyricParser::currentIndex(lines, 2500), 1);
}

void QqLyricParserTest::preservesTimestampLikeLyricText()
{
    const QVector<LyricLine> lines = QqLyricParser::parseLrc(
        QStringLiteral("[00:01]Meet at [12:30]"));

    QCOMPARE(lines.size(), 1);
    QCOMPARE(lines.at(0).timeMs, qint64(1000));
    QCOMPARE(lines.at(0).text, QStringLiteral("Meet at [12:30]"));
}

void QqLyricParserTest::ignoresMetadataSortsStablyAndMergesDuplicateTimes()
{
    const QVector<LyricLine> lines = QqLyricParser::parseLrc(
        QString::fromUtf8(fixture(QStringLiteral("lyrics.lrc"))));

    QCOMPARE(lines.size(), 4);
    QCOMPARE(lines.at(0).timeMs, qint64(1200));
    QCOMPARE(lines.at(0).text,
             QStringLiteral("Source // marker\nMerged source"));
    QCOMPARE(lines.at(1).timeMs, qint64(2000));
    QCOMPARE(lines.at(1).text, QStringLiteral("Source // marker"));
    QCOMPARE(lines.at(2).timeMs, qint64(3500));
    QCOMPARE(lines.at(2).text, QStringLiteral("Last source"));
    QCOMPARE(lines.at(3).timeMs, qint64(4000));
    QCOMPARE(lines.at(3).text, QStringLiteral("No translation"));
}

void QqLyricParserTest::alignsExactTranslationsFromSanitizedFixture()
{
    const ServiceResult<LyricDocument> result = QqLyricParser::parse(
        fixture(QStringLiteral("lyrics.json")), 21);

    QVERIFY(result.ok());
    QCOMPARE(result.value.lines.size(), 4);
    QCOMPARE(result.value.lines.at(0).text,
             QStringLiteral("Source // marker\nMerged source"));
    QCOMPARE(result.value.lines.at(0).translation,
             QStringLiteral("Translatedtext"));
    QVERIFY(result.value.lines.at(1).translation.isEmpty());
    QVERIFY(result.value.lines.at(2).translation.isEmpty());
    QVERIFY(result.value.lines.at(3).translation.isEmpty());

    const QVector<LyricLine> untranslated = QqLyricParser::parseLrc(
        QString::fromUtf8(fixture(QStringLiteral("lyrics-translated.lrc"))));
    QCOMPARE(untranslated.at(0).text, QStringLiteral("Translated//text"));
}

void QqLyricParserTest::preservesValidEmptyLyrics()
{
    const ServiceResult<LyricDocument> result = QqLyricParser::parse(
        QByteArrayLiteral("{\"lyric\":\"\",\"trans\":\"\"}"), 22);

    QVERIFY(result.ok());
    QVERIFY(result.value.lines.isEmpty());
}

void QqLyricParserTest::rejectsChangedLyricSchemas()
{
    const ServiceResult<LyricDocument> wrongType = QqLyricParser::parse(
        QByteArrayLiteral("{\"lyric\":7,\"trans\":\"\"}"), 23);
    QVERIFY(!wrongType.ok());
    QCOMPARE(wrongType.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(wrongType.error.operationId, quint64(23));
    QCOMPARE(wrongType.error.diagnosticCode,
             QStringLiteral("qq.lyric.json.lyric.string"));

    const ServiceResult<LyricDocument> missingTranslation =
        QqLyricParser::parse(QByteArrayLiteral("{\"lyric\":\"\"}"), 24);
    QVERIFY(!missingTranslation.ok());
    QCOMPARE(missingTranslation.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(missingTranslation.error.operationId, quint64(24));
    QCOMPARE(missingTranslation.error.diagnosticCode,
             QStringLiteral("qq.lyric.json.trans.string"));

    const ServiceResult<LyricDocument> wrongTranslation =
        QqLyricParser::parse(
            QByteArrayLiteral("{\"lyric\":\"\",\"trans\":7}"), 25);
    QVERIFY(!wrongTranslation.ok());
    QCOMPARE(wrongTranslation.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(wrongTranslation.error.operationId, quint64(25));
    QCOMPARE(wrongTranslation.error.diagnosticCode,
             QStringLiteral("qq.lyric.json.trans.string"));
}

void QqLyricParserTest::buildsPinnedLyricRequest()
{
    const QUrl url =
        QqRequestBuilder::lyricUrl(QStringLiteral("003OUlho2HcRHC"));
    QCOMPARE(url.scheme(), QStringLiteral("https"));
    QCOMPARE(url.host(), QStringLiteral("i.y.qq.com"));
    QCOMPARE(url.path(),
             QStringLiteral("/lyric/fcgi-bin/fcg_query_lyric_new.fcg"));

    QList<QueryItem> expected;
    expected
        << QueryItem(QStringLiteral("songmid"),
                     QStringLiteral("003OUlho2HcRHC"))
        << QueryItem(QStringLiteral("g_tk"), QStringLiteral("5381"))
        << QueryItem(QStringLiteral("format"), QStringLiteral("json"))
        << QueryItem(QStringLiteral("inCharset"), QStringLiteral("utf8"))
        << QueryItem(QStringLiteral("outCharset"), QStringLiteral("utf-8"))
        << QueryItem(QStringLiteral("nobase64"), QStringLiteral("1"));
    sortQueryItems(&expected);
    QCOMPARE(decodedQuery(url), expected);
    QCOMPARE(QUrlQuery(url).queryItems().size(), 6);
}

void QqLyricParserTest::rejectsUnsafeLyricRequestIdentifiers()
{
    QVERIFY(QqRequestBuilder::lyricUrl(QStringLiteral("../cookie")).isEmpty());
    QVERIFY(QqRequestBuilder::lyricUrl(QString(13, QLatin1Char('A'))).isEmpty());
}

QTEST_APPLESS_MAIN(QqLyricParserTest)

#include "tst_qqlyricparser.moc"
