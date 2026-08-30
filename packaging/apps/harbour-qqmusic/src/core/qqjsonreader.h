#ifndef QQJSONREADER_H
#define QQJSONREADER_H

#include "types.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

class QqJsonReader
{
public:
    explicit QqJsonReader(const QJsonDocument &document, quint64 operationId);

    ServiceResult<QJsonObject> requiredObject(const QStringList &path) const;
    ServiceResult<QJsonArray> requiredArray(const QStringList &path) const;
    ServiceResult<QString> requiredString(const QStringList &path) const;
    ServiceResult<qint64> requiredInteger(const QStringList &path) const;

    ServiceResult<QString> optionalString(
        const QStringList &path,
        const QString &defaultValue = QString()) const;
    ServiceResult<qint64> optionalInteger(
        const QStringList &path,
        qint64 defaultValue = 0) const;

private:
    struct LookupResult {
        QJsonValue value;
        bool found = false;
        bool invalidContainer = false;
    };

    LookupResult lookup(const QStringList &path) const;

    QJsonDocument m_document;
    quint64 m_operationId;
};

#endif
