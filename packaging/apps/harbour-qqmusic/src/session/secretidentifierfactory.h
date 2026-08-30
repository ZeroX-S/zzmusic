#ifndef SECRETIDENTIFIERFACTORY_H
#define SECRETIDENTIFIERFACTORY_H

#include <QObject>
#include <QVariant>

class SecretIdentifierFactory : public QObject
{
    Q_OBJECT

public:
    explicit SecretIdentifierFactory(QObject *parent = 0);

    Q_INVOKABLE QVariant sessionIdentifier(
        const QString &collectionName,
        const QString &storagePluginName) const;
    Q_INVOKABLE QVariant namedSessionIdentifier(
        const QString &secretName,
        const QString &collectionName,
        const QString &storagePluginName) const;
    Q_INVOKABLE QString newSessionSecretName() const;
};

#endif
