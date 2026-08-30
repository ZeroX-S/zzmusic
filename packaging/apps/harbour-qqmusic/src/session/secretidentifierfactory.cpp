#include "secretidentifierfactory.h"

#include <Secrets/secret.h>
#include <QDateTime>
#include <QUuid>

SecretIdentifierFactory::SecretIdentifierFactory(QObject *parent)
    : QObject(parent)
{
}

QVariant SecretIdentifierFactory::sessionIdentifier(
    const QString &collectionName,
    const QString &storagePluginName) const
{
    return namedSessionIdentifier(QStringLiteral("qq-session-v2"),
                                  collectionName,
                                  storagePluginName);
}

QVariant SecretIdentifierFactory::namedSessionIdentifier(
    const QString &secretName,
    const QString &collectionName,
    const QString &storagePluginName) const
{
    if (secretName.isEmpty() || collectionName.isEmpty()
            || storagePluginName.isEmpty()) {
        return QVariant();
    }
    const Sailfish::Secrets::Secret::Identifier identifier(
        secretName, collectionName, storagePluginName);
    return QVariant::fromValue(identifier);
}

QString SecretIdentifierFactory::newSessionSecretName() const
{
    QString uuid = QUuid::createUuid().toString();
    uuid.remove(QLatin1Char('{'));
    uuid.remove(QLatin1Char('}'));
    uuid.remove(QLatin1Char('-'));
    return QStringLiteral("qq-session-v3-%1-%2")
        .arg(QDateTime::currentMSecsSinceEpoch(), 13, 10, QLatin1Char('0'))
        .arg(uuid);
}
