#include "cookiepolicy.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace {

ServiceError invalid(const QString &code)
{
    ServiceError error;
    error.kind = ErrorKind::Internal;
    error.diagnosticCode = code;
    return error;
}

ServiceError expired(const QString &code)
{
    ServiceError error;
    error.kind = ErrorKind::AuthenticationExpired;
    error.diagnosticCode = code;
    return error;
}

bool containsControlBytes(const QByteArray &value)
{
    for (int index = 0; index < value.size(); ++index) {
        const uchar byte = static_cast<uchar>(value.at(index));
        if (byte < 0x20 || byte == 0x7f) {
            return true;
        }
    }
    return false;
}

bool containsControlCharacters(const QString &value)
{
    for (int index = 0; index < value.size(); ++index) {
        const ushort character = value.at(index).unicode();
        if (character < 0x20 || character == 0x7f) {
            return true;
        }
    }
    return false;
}

bool isAllowedDomain(const QString &domain)
{
    static const char *const allowed[] = {
        ".qq.com",
        "qq.com",
        ".y.qq.com",
        "y.qq.com",
        ".ptlogin2.qq.com",
        "ptlogin2.qq.com"
    };
    for (uint index = 0; index < sizeof(allowed) / sizeof(allowed[0]); ++index) {
        if (domain.compare(QString::fromLatin1(allowed[index]),
                           Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

QString duplicateKey(const CookieRecord &record)
{
    return QString::fromLatin1(record.name.toLower().toHex())
        + QLatin1Char('|') + record.domain.toLower()
        + QLatin1Char('|') + record.path;
}

bool hasExactKeys(const QJsonObject &object)
{
    static const char *const keys[] = {
        "name", "value", "domain", "path", "secure", "expires"
    };
    if (object.size() != int(sizeof(keys) / sizeof(keys[0]))) {
        return false;
    }
    for (uint index = 0; index < sizeof(keys) / sizeof(keys[0]); ++index) {
        if (!object.contains(QString::fromLatin1(keys[index]))) {
            return false;
        }
    }
    return true;
}

}

int CookiePolicy::maximumCookieBytes()
{
    return 4096;
}

int CookiePolicy::maximumPayloadBytes()
{
    return 32 * 1024;
}

bool CookiePolicy::isAllowedName(const QByteArray &name)
{
    static const char *const allowed[] = {
        "uin", "wxuin", "qm_keyst", "qqmusic_key", "p_skey", "skey",
        "pt2gguin"
    };
    for (uint index = 0; index < sizeof(allowed) / sizeof(allowed[0]); ++index) {
        if (name == allowed[index]) {
            return true;
        }
    }
    return false;
}

CookieValidation CookiePolicy::validate(const CookieRecord &record,
                                        const QDateTime &nowUtc)
{
    CookieValidation result;
    const QDateTime now = nowUtc.toUTC();
    if (!now.isValid()) {
        result.error = invalid(QStringLiteral("qq.session.now.invalid"));
    } else if (!isAllowedName(record.name)) {
        result.error = invalid(QStringLiteral("qq.session.cookie.name"));
    } else if (record.value.isEmpty()
               || record.value.size() > maximumCookieBytes()
               || containsControlBytes(record.name)
               || containsControlBytes(record.value)) {
        result.error = invalid(QStringLiteral("qq.session.cookie.value"));
    } else if (!record.secure) {
        result.error = invalid(QStringLiteral("qq.session.cookie.secure"));
    } else if (!isAllowedDomain(record.domain)
               || containsControlCharacters(record.domain)) {
        result.error = invalid(QStringLiteral("qq.session.cookie.domain"));
    } else if (!record.path.startsWith(QLatin1Char('/'))
               || containsControlCharacters(record.path)) {
        result.error = invalid(QStringLiteral("qq.session.cookie.path"));
    } else if (record.expiresAt.isValid()
               && record.expiresAt.toUTC() <= now) {
        result.error = expired(QStringLiteral("qq.session.cookie.expiry"));
    } else if (!record.expiresAt.isValid()
               || record.expiresAt.toUTC() > now.addDays(400)) {
        result.error = invalid(QStringLiteral("qq.session.cookie.expiry"));
    } else {
        result.value = record;
        result.value.expiresAt = record.expiresAt.toUTC();
    }
    return result;
}

ServiceResult<QVector<CookieRecord> > CookiePolicy::validateAll(
    const QVector<CookieRecord> &records,
    const QDateTime &nowUtc)
{
    ServiceResult<QVector<CookieRecord> > result;
    QSet<QString> identities;
    for (int index = 0; index < records.size(); ++index) {
        const CookieValidation validation = validate(records.at(index), nowUtc);
        if (!validation.ok()) {
            result.error = validation.error;
            return result;
        }
        const QString identity = duplicateKey(validation.value);
        if (identities.contains(identity)) {
            result.error = invalid(QStringLiteral("qq.session.cookie.duplicate"));
            return result;
        }
        identities.insert(identity);
        result.value.append(validation.value);
    }
    const QByteArray encoded = serialize(result.value);
    if (encoded.size() > maximumPayloadBytes()) {
        result.value.clear();
        result.error = invalid(QStringLiteral("qq.session.payload.oversized"));
    }
    return result;
}

QByteArray CookiePolicy::serialize(const QVector<CookieRecord> &records)
{
    QJsonArray cookies;
    for (int index = 0; index < records.size(); ++index) {
        const CookieRecord &record = records.at(index);
        QJsonObject object;
        object.insert(QStringLiteral("name"),
                      QString::fromLatin1(record.name.toBase64()));
        object.insert(QStringLiteral("value"),
                      QString::fromLatin1(record.value.toBase64()));
        object.insert(QStringLiteral("domain"), record.domain);
        object.insert(QStringLiteral("path"), record.path);
        object.insert(QStringLiteral("secure"), record.secure);
        object.insert(QStringLiteral("expires"),
                      double(record.expiresAt.toUTC().toMSecsSinceEpoch() / 1000));
        cookies.append(object);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("cookies"), cookies);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

ServiceResult<QVector<CookieRecord> > CookiePolicy::deserialize(
    const QByteArray &payload,
    const QDateTime &nowUtc)
{
    ServiceResult<QVector<CookieRecord> > result;
    if (payload.isEmpty() || payload.size() > maximumPayloadBytes()) {
        result.error = invalid(QStringLiteral("qq.session.payload.size"));
        return result;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.error = invalid(QStringLiteral("qq.session.payload.json"));
        return result;
    }
    const QJsonObject root = document.object();
    if (root.size() != 2
            || !root.value(QStringLiteral("version")).isDouble()
            || root.value(QStringLiteral("version")).toInt(-1) != 1
            || !root.value(QStringLiteral("cookies")).isArray()) {
        result.error = invalid(QStringLiteral("qq.session.payload.schema"));
        return result;
    }

    QVector<CookieRecord> records;
    const QJsonArray cookies = root.value(QStringLiteral("cookies")).toArray();
    for (int index = 0; index < cookies.size(); ++index) {
        if (!cookies.at(index).isObject()) {
            result.error = invalid(QStringLiteral("qq.session.payload.record"));
            return result;
        }
        const QJsonObject object = cookies.at(index).toObject();
        if (!hasExactKeys(object)
                || !object.value(QStringLiteral("name")).isString()
                || !object.value(QStringLiteral("value")).isString()
                || !object.value(QStringLiteral("domain")).isString()
                || !object.value(QStringLiteral("path")).isString()
                || !object.value(QStringLiteral("secure")).isBool()
                || !object.value(QStringLiteral("expires")).isDouble()) {
            result.error = invalid(QStringLiteral("qq.session.payload.fields"));
            return result;
        }
        CookieRecord record;
        const QByteArray encodedName =
            object.value(QStringLiteral("name")).toString().toLatin1();
        const QByteArray encodedValue =
            object.value(QStringLiteral("value")).toString().toLatin1();
        record.name = QByteArray::fromBase64(encodedName);
        record.value = QByteArray::fromBase64(encodedValue);
        if (record.name.toBase64() != encodedName
                || record.value.toBase64() != encodedValue) {
            result.error = invalid(QStringLiteral("qq.session.payload.base64"));
            return result;
        }
        record.domain = object.value(QStringLiteral("domain")).toString();
        record.path = object.value(QStringLiteral("path")).toString();
        record.secure = object.value(QStringLiteral("secure")).toBool();
        const qint64 expiry = qint64(
            object.value(QStringLiteral("expires")).toDouble(-1));
        record.expiresAt = QDateTime::fromMSecsSinceEpoch(expiry * 1000, Qt::UTC);
        records.append(record);
    }
    return validateAll(records, nowUtc);
}
