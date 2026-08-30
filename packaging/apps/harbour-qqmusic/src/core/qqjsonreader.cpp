#include "qqjsonreader.h"

#include <cmath>

namespace {

template<typename T>
ServiceResult<T> schemaChanged(const QStringList &path,
                               const QString &expectedType,
                               quint64 operationId)
{
    ServiceResult<T> result;
    result.error.kind = ErrorKind::SchemaChanged;
    result.error.operationId = operationId;
    result.error.diagnosticCode = QStringLiteral("json.%1.%2")
        .arg(path.join(QStringLiteral(".")), expectedType);
    return result;
}

bool isStrictInteger(const QJsonValue &value, qint64 *integer)
{
    if (!value.isDouble()) {
        return false;
    }

    const double number = value.toDouble();
    const double maximumExactInteger = 9007199254740991.0;
    if (!std::isfinite(number)
            || std::floor(number) != number
            || number < -maximumExactInteger
            || number > maximumExactInteger) {
        return false;
    }

    *integer = static_cast<qint64>(number);
    return true;
}

}

QqJsonReader::QqJsonReader(const QJsonDocument &document, quint64 operationId)
    : m_document(document)
    , m_operationId(operationId)
{
}

QqJsonReader::LookupResult QqJsonReader::lookup(const QStringList &path) const
{
    LookupResult result;
    if (m_document.isObject()) {
        result.value = QJsonValue(m_document.object());
    } else if (m_document.isArray()) {
        result.value = QJsonValue(m_document.array());
    } else {
        result.invalidContainer = true;
        return result;
    }

    for (int index = 0; index < path.size(); ++index) {
        if (!result.value.isObject()) {
            result.invalidContainer = true;
            return result;
        }

        const QJsonObject object = result.value.toObject();
        const QString &member = path.at(index);
        if (!object.contains(member)) {
            return result;
        }
        result.value = object.value(member);
    }

    result.found = true;
    return result;
}

ServiceResult<QJsonObject> QqJsonReader::requiredObject(const QStringList &path) const
{
    const LookupResult lookupResult = lookup(path);
    if (!lookupResult.found || !lookupResult.value.isObject()) {
        return schemaChanged<QJsonObject>(path, QStringLiteral("object"), m_operationId);
    }

    ServiceResult<QJsonObject> result;
    result.value = lookupResult.value.toObject();
    return result;
}

ServiceResult<QJsonArray> QqJsonReader::requiredArray(const QStringList &path) const
{
    const LookupResult lookupResult = lookup(path);
    if (!lookupResult.found || !lookupResult.value.isArray()) {
        return schemaChanged<QJsonArray>(path, QStringLiteral("array"), m_operationId);
    }

    ServiceResult<QJsonArray> result;
    result.value = lookupResult.value.toArray();
    return result;
}

ServiceResult<QString> QqJsonReader::requiredString(const QStringList &path) const
{
    const LookupResult lookupResult = lookup(path);
    if (!lookupResult.found || !lookupResult.value.isString()) {
        return schemaChanged<QString>(path, QStringLiteral("string"), m_operationId);
    }

    ServiceResult<QString> result;
    result.value = lookupResult.value.toString();
    return result;
}

ServiceResult<qint64> QqJsonReader::requiredInteger(const QStringList &path) const
{
    const LookupResult lookupResult = lookup(path);
    qint64 integer = 0;
    if (!lookupResult.found || !isStrictInteger(lookupResult.value, &integer)) {
        return schemaChanged<qint64>(path, QStringLiteral("integer"), m_operationId);
    }

    ServiceResult<qint64> result;
    result.value = integer;
    return result;
}

ServiceResult<QString> QqJsonReader::optionalString(
    const QStringList &path,
    const QString &defaultValue) const
{
    const LookupResult lookupResult = lookup(path);
    if (!lookupResult.found && !lookupResult.invalidContainer) {
        ServiceResult<QString> result;
        result.value = defaultValue;
        return result;
    }
    if (!lookupResult.found || !lookupResult.value.isString()) {
        return schemaChanged<QString>(path, QStringLiteral("string"), m_operationId);
    }

    ServiceResult<QString> result;
    result.value = lookupResult.value.toString();
    return result;
}

ServiceResult<qint64> QqJsonReader::optionalInteger(
    const QStringList &path,
    qint64 defaultValue) const
{
    const LookupResult lookupResult = lookup(path);
    if (!lookupResult.found && !lookupResult.invalidContainer) {
        ServiceResult<qint64> result;
        result.value = defaultValue;
        return result;
    }

    qint64 integer = 0;
    if (!lookupResult.found || !isStrictInteger(lookupResult.value, &integer)) {
        return schemaChanged<qint64>(path, QStringLiteral("integer"), m_operationId);
    }

    ServiceResult<qint64> result;
    result.value = integer;
    return result;
}
