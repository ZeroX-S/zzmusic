#include <QtTest>

#include "session/secretidentifierfactory.h"

#include <Secrets/createcollectionrequest.h>
#include <Secrets/secret.h>
#include <Secrets/storesecretrequest.h>

class SecretIdentifierFactoryTest : public QObject
{
    Q_OBJECT

private slots:
    void createsOnlyTheFixedCollectionSessionIdentifier()
    {
        SecretIdentifierFactory factory;
        QVERIFY(!factory.sessionIdentifier(
                    QString(),
                    QStringLiteral("plugin.storage.default")).isValid());
        QVERIFY(!factory.sessionIdentifier(
                    QStringLiteral("org.bonito.qqmusic.sessions.v1"),
                    QString()).isValid());

        const QVariant value = factory.sessionIdentifier(
            QStringLiteral("org.bonito.qqmusic.sessions.v2"),
            QStringLiteral("plugin.storage.default"));
        QVERIFY(value.isValid());
        const Sailfish::Secrets::Secret::Identifier identifier =
            value.value<Sailfish::Secrets::Secret::Identifier>();
        QCOMPARE(identifier.name(), QStringLiteral("qq-session-v2"));
        QCOMPARE(identifier.collectionName(),
                 QStringLiteral("org.bonito.qqmusic.sessions.v2"));
        QCOMPARE(identifier.storagePluginName(),
                 QStringLiteral("plugin.storage.default"));
        QVERIFY(!identifier.identifiesStandaloneSecret());
    }

    void createsNamedImmutableSessionIdentifiers()
    {
        SecretIdentifierFactory factory;
        QVERIFY(!factory.namedSessionIdentifier(
                    QString(), QStringLiteral("collection"),
                    QStringLiteral("storage")).isValid());

        const QString firstName = factory.newSessionSecretName();
        const QString secondName = factory.newSessionSecretName();
        QVERIFY(firstName.startsWith(QStringLiteral("qq-session-v3-")));
        QVERIFY(secondName.startsWith(QStringLiteral("qq-session-v3-")));
        QVERIFY(firstName != secondName);

        const QVariant value = factory.namedSessionIdentifier(
            firstName, QStringLiteral("org.bonito.zzmusic.sessions.v3"),
            QStringLiteral("plugin.storage.default"));
        QVERIFY(value.isValid());
        const Sailfish::Secrets::Secret::Identifier identifier =
            value.value<Sailfish::Secrets::Secret::Identifier>();
        QCOMPARE(identifier.name(), firstName);
        QCOMPARE(identifier.collectionName(),
                 QStringLiteral("org.bonito.zzmusic.sessions.v3"));
        QCOMPARE(identifier.storagePluginName(),
                 QStringLiteral("plugin.storage.default"));
    }

    void requestDefaultsUseCollectionStorage()
    {
        Sailfish::Secrets::CreateCollectionRequest createRequest;
        QCOMPARE(createRequest.collectionLockType(),
                 Sailfish::Secrets::CreateCollectionRequest::DeviceLock);

        Sailfish::Secrets::StoreSecretRequest storeRequest;
        QCOMPARE(storeRequest.secretStorageType(),
                 Sailfish::Secrets::StoreSecretRequest::CollectionSecret);
    }
};

QTEST_GUILESS_MAIN(SecretIdentifierFactoryTest)
#include "tst_secretidentifierfactory.moc"
