#include <QtTest>

#include "core/qqidentifier.h"
#include "core/qqjsonreader.h"

class CoreTest : public QObject
{
    Q_OBJECT

private slots:
    void initializesDomainDefaults();
    void registersQueuedSignalTypesAtStartup();
    void validatesIdsAndUrls();
    void rejectsWrongJsonTypes();
    void distinguishesMissingEmptyAndOptionalValues();
    void readsStrictIntegers();
    void handlesDocumentAndTraversalBoundaries();
    void reportsRequiredValueDiagnostics();
};

void CoreTest::initializesDomainDefaults()
{
    const ServiceError error;
    QCOMPARE(error.kind, ErrorKind::None);
    QCOMPARE(error.operationId, quint64(0));
    QVERIFY(error.diagnosticCode.isEmpty());
    QCOMPARE(error.retryAfterMs, 0);

    const Track track;
    QCOMPARE(track.durationMs, qint64(0));
    QVERIFY(track.playable);

    const PlaylistSummary playlist;
    QCOMPARE(playlist.trackCount, -1);

    const LyricLine lyric;
    QCOMPARE(lyric.timeMs, qint64(0));

    const PlaybackLocation location;
    QVERIFY(!location.accountBound);
}

void CoreTest::registersQueuedSignalTypesAtStartup()
{
    const char *const typeNames[] = {
        "ServiceError",
        "QVector<PlaylistSummary>",
        "QVector<CategoryGroup>",
        "QVector<Track>",
        "PlaylistDetail",
        "UserProfile",
        "LyricDocument",
        "PlaybackLocation"
    };

    for (size_t index = 0; index < sizeof(typeNames) / sizeof(typeNames[0]); ++index) {
        QVERIFY2(QMetaType::type(typeNames[index]) != QMetaType::UnknownType,
                 typeNames[index]);
    }
}

void CoreTest::validatesIdsAndUrls()
{
    QVERIFY(QqIdentifier::isTrackMid(QStringLiteral("003OUlho2HcRHC")));
    QVERIFY(!QqIdentifier::isTrackMid(QStringLiteral("../cookie")));
    QVERIFY(!QqIdentifier::isTrackMid(QString(13, QLatin1Char('A'))));
    QVERIFY(!QqIdentifier::isTrackMid(QString(15, QLatin1Char('A'))));
    QVERIFY(!QqIdentifier::isTrackMid(
        QStringLiteral("003OUlho2HcRH") + QChar(0x4e2d)));
    QVERIFY(QqIdentifier::isNumericPlaylistId(QStringLiteral("9451963602")));
    QVERIFY(!QqIdentifier::isNumericPlaylistId(QStringLiteral("0x10")));
    QVERIFY(!QqIdentifier::isNumericPlaylistId(QStringLiteral("09451963602")));
    QVERIFY(!QqIdentifier::isNumericPlaylistId(
        QStringLiteral("18446744073709551616")));
    QVERIFY(!QqIdentifier::isNumericPlaylistId(
        QString(QChar(0xff19)) + QStringLiteral("451963602")));
    QVERIFY(QqIdentifier::isSafeSourceUrl(
        QUrl(QStringLiteral("https://y.qq.com/n/ryqq/playlist/9451963602"))));
    QVERIFY(!QqIdentifier::isSafeSourceUrl(
        QUrl(QStringLiteral("http://evil.invalid/9451963602"))));
    QVERIFY(!QqIdentifier::isSafeSourceUrl(QUrl(QStringLiteral(
        "https://y.qq.com.evil.invalid/n/ryqq/playlist/9451963602"))));
    QVERIFY(!QqIdentifier::isSafeSourceUrl(QUrl(QStringLiteral(
        "https://music.y.qq.com/n/ryqq/playlist/9451963602"))));
    QVERIFY(!QqIdentifier::isSafeSourceUrl(QUrl(QStringLiteral(
        "https://user@y.qq.com/n/ryqq/playlist/9451963602"))));
    QVERIFY(!QqIdentifier::isSafeSourceUrl(QUrl(QStringLiteral(
        "https://y.qq.com:444/n/ryqq/playlist/9451963602"))));
    QVERIFY(!QqIdentifier::isSafeSourceUrl(QUrl(QStringLiteral(
        "https://y.qq.com/n/ryqq/playlist/9451963602#fragment"))));
    QVERIFY(!QqIdentifier::isSafeSourceUrl(QUrl(QStringLiteral(
        "https://y.qq.com/n/ryqq/%2e%2e/playlist/9451963602"))));
}

void CoreTest::rejectsWrongJsonTypes()
{
    QqJsonReader reader(
        QJsonDocument::fromJson("{\"data\":{\"list\":7}}"), 41);
    const ServiceResult<QJsonArray> result =
        reader.requiredArray(QStringList() << QStringLiteral("data")
                                           << QStringLiteral("list"));

    QVERIFY(!result.ok());
    QCOMPARE(result.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(result.error.operationId, quint64(41));
    QCOMPARE(result.error.diagnosticCode, QStringLiteral("json.data.list.array"));
}

void CoreTest::distinguishesMissingEmptyAndOptionalValues()
{
    QqJsonReader reader(QJsonDocument::fromJson(
                            "{\"data\":{\"list\":[],\"label\":\"QQ\","
                            "\"count\":3,\"badOptional\":false}}"),
                        42);

    const ServiceResult<QJsonArray> empty =
        reader.requiredArray(QStringList() << QStringLiteral("data")
                                           << QStringLiteral("list"));
    QVERIFY(empty.ok());
    QVERIFY(empty.value.isEmpty());

    const ServiceResult<QJsonArray> missing =
        reader.requiredArray(QStringList() << QStringLiteral("data")
                                           << QStringLiteral("missing"));
    QVERIFY(!missing.ok());
    QCOMPARE(missing.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(missing.error.operationId, quint64(42));
    QCOMPARE(missing.error.diagnosticCode,
             QStringLiteral("json.data.missing.array"));

    const ServiceResult<QString> optionalString =
        reader.optionalString(QStringList() << QStringLiteral("data")
                                            << QStringLiteral("label"),
                              QStringLiteral("untitled"));
    QVERIFY(optionalString.ok());
    QCOMPARE(optionalString.value, QStringLiteral("QQ"));

    const ServiceResult<qint64> optionalInteger =
        reader.optionalInteger(QStringList() << QStringLiteral("data")
                                             << QStringLiteral("count"),
                               qint64(-1));
    QVERIFY(optionalInteger.ok());
    QCOMPARE(optionalInteger.value, qint64(3));

    const ServiceResult<QString> defaultString =
        reader.optionalString(QStringList() << QStringLiteral("data")
                                            << QStringLiteral("missingLabel"),
                              QStringLiteral("untitled"));
    QVERIFY(defaultString.ok());
    QCOMPARE(defaultString.value, QStringLiteral("untitled"));

    const ServiceResult<qint64> defaultInteger =
        reader.optionalInteger(QStringList() << QStringLiteral("data")
                                             << QStringLiteral("missingCount"),
                               qint64(-1));
    QVERIFY(defaultInteger.ok());
    QCOMPARE(defaultInteger.value, qint64(-1));

    const ServiceResult<QString> wrongOptional =
        reader.optionalString(QStringList() << QStringLiteral("data")
                                            << QStringLiteral("badOptional"));
    QVERIFY(!wrongOptional.ok());
    QCOMPARE(wrongOptional.error.diagnosticCode,
             QStringLiteral("json.data.badOptional.string"));
}

void CoreTest::readsStrictIntegers()
{
    QqJsonReader reader(QJsonDocument::fromJson(
                            "{\"whole\":7,\"fraction\":7.5,"
                            "\"string\":\"7\",\"tooLarge\":9223372036854775808,"
                            "\"exactPositive\":9007199254740991,"
                            "\"exactNegative\":-9007199254740991,"
                            "\"unsafePositive\":9007199254740992,"
                            "\"unsafeNegative\":-9007199254740992,"
                            "\"roundedPositive\":9007199254740993,"
                            "\"roundedNegative\":-9007199254740993}"),
                        43);

    const ServiceResult<qint64> whole =
        reader.requiredInteger(QStringList() << QStringLiteral("whole"));
    QVERIFY(whole.ok());
    QCOMPARE(whole.value, qint64(7));

    const ServiceResult<qint64> fraction =
        reader.requiredInteger(QStringList() << QStringLiteral("fraction"));
    QVERIFY(!fraction.ok());
    QCOMPARE(fraction.error.diagnosticCode,
             QStringLiteral("json.fraction.integer"));

    const ServiceResult<qint64> string =
        reader.requiredInteger(QStringList() << QStringLiteral("string"));
    QVERIFY(!string.ok());
    QCOMPARE(string.error.diagnosticCode, QStringLiteral("json.string.integer"));

    const ServiceResult<qint64> tooLarge =
        reader.requiredInteger(QStringList() << QStringLiteral("tooLarge"));
    QVERIFY(!tooLarge.ok());
    QCOMPARE(tooLarge.error.diagnosticCode,
             QStringLiteral("json.tooLarge.integer"));

    const ServiceResult<qint64> exactPositive =
        reader.requiredInteger(QStringList() << QStringLiteral("exactPositive"));
    QVERIFY(exactPositive.ok());
    QCOMPARE(exactPositive.value, qint64(9007199254740991LL));

    const ServiceResult<qint64> exactNegative =
        reader.requiredInteger(QStringList() << QStringLiteral("exactNegative"));
    QVERIFY(exactNegative.ok());
    QCOMPARE(exactNegative.value, qint64(-9007199254740991LL));

    const ServiceResult<qint64> unsafePositive =
        reader.requiredInteger(QStringList() << QStringLiteral("unsafePositive"));
    QVERIFY(!unsafePositive.ok());

    const ServiceResult<qint64> unsafeNegative =
        reader.requiredInteger(QStringList() << QStringLiteral("unsafeNegative"));
    QVERIFY(!unsafeNegative.ok());

    const ServiceResult<qint64> roundedPositive =
        reader.requiredInteger(QStringList() << QStringLiteral("roundedPositive"));
    QVERIFY(!roundedPositive.ok());

    const ServiceResult<qint64> roundedNegative =
        reader.requiredInteger(QStringList() << QStringLiteral("roundedNegative"));
    QVERIFY(!roundedNegative.ok());
}

void CoreTest::handlesDocumentAndTraversalBoundaries()
{
    QqJsonReader malformedIntermediate(
        QJsonDocument::fromJson("{\"data\":7}"), 45);
    const ServiceResult<QString> malformedOptional =
        malformedIntermediate.optionalString(
            QStringList() << QStringLiteral("data") << QStringLiteral("name"),
            QStringLiteral("fallback"));
    QVERIFY(!malformedOptional.ok());
    QCOMPARE(malformedOptional.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(malformedOptional.error.operationId, quint64(45));
    QCOMPARE(malformedOptional.error.diagnosticCode,
             QStringLiteral("json.data.name.string"));

    QqJsonReader nullDocument(QJsonDocument(), 46);
    const ServiceResult<QJsonObject> nullObject =
        nullDocument.requiredObject(QStringList() << QStringLiteral("data"));
    QVERIFY(!nullObject.ok());
    QCOMPARE(nullObject.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(nullObject.error.operationId, quint64(46));

    QqJsonReader rootArray(QJsonDocument::fromJson("[]"), 47);
    const ServiceResult<QJsonArray> array = rootArray.requiredArray(QStringList());
    QVERIFY(array.ok());
    QVERIFY(array.value.isEmpty());

    QqJsonReader values(
        QJsonDocument::fromJson("{\"nullValue\":null,\"empty\":\"\"}"), 48);
    const ServiceResult<QString> nullOptional =
        values.optionalString(QStringList() << QStringLiteral("nullValue"),
                              QStringLiteral("fallback"));
    QVERIFY(!nullOptional.ok());
    QCOMPARE(nullOptional.error.diagnosticCode,
             QStringLiteral("json.nullValue.string"));

    const ServiceResult<QString> emptyRequired =
        values.requiredString(QStringList() << QStringLiteral("empty"));
    QVERIFY(emptyRequired.ok());
    QVERIFY(emptyRequired.value.isEmpty());
}

void CoreTest::reportsRequiredValueDiagnostics()
{
    QqJsonReader reader(
        QJsonDocument::fromJson("{\"data\":{\"name\":7,\"count\":2}}"), 44);

    const ServiceResult<QJsonObject> object =
        reader.requiredObject(QStringList() << QStringLiteral("data"));
    QVERIFY(object.ok());

    const ServiceResult<QString> string =
        reader.requiredString(QStringList() << QStringLiteral("data")
                                            << QStringLiteral("name"));
    QVERIFY(!string.ok());
    QCOMPARE(string.error.kind, ErrorKind::SchemaChanged);
    QCOMPARE(string.error.operationId, quint64(44));
    QCOMPARE(string.error.diagnosticCode, QStringLiteral("json.data.name.string"));

    const ServiceResult<qint64> integer =
        reader.requiredInteger(QStringList() << QStringLiteral("data")
                                             << QStringLiteral("count"));
    QVERIFY(integer.ok());
    QCOMPARE(integer.value, qint64(2));
}

QTEST_APPLESS_MAIN(CoreTest)

#include "tst_core.moc"
