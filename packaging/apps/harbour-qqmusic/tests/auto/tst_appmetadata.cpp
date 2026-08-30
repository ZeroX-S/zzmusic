#include <QtTest>

#include "appmetadata.h"

class AppMetadataTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesStableRuntimeIdentityAndZZMusicDisplayName()
    {
        QCOMPARE(AppMetadata::applicationId(), QString("harbour-qqmusic"));
        QCOMPARE(AppMetadata::displayName(), QString("ZZ Music"));
        QCOMPARE(AppMetadata::organizationName(), QString("org.bonito.qqmusic"));
        QCOMPARE(AppMetadata::schemaVersion(), 1);
    }
};

QTEST_APPLESS_MAIN(AppMetadataTest)

#include "tst_appmetadata.moc"
