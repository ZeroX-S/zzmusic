#include "qqidentifier.h"

namespace {

bool isAsciiLetterOrDigit(QChar character)
{
    const ushort value = character.unicode();
    return (value >= '0' && value <= '9')
        || (value >= 'A' && value <= 'Z')
        || (value >= 'a' && value <= 'z');
}

bool isAsciiDigit(QChar character)
{
    const ushort value = character.unicode();
    return value >= '0' && value <= '9';
}

}

bool QqIdentifier::isTrackMid(const QString &value)
{
    if (value.size() != 14) {
        return false;
    }

    for (int index = 0; index < value.size(); ++index) {
        if (!isAsciiLetterOrDigit(value.at(index))) {
            return false;
        }
    }
    return true;
}

bool QqIdentifier::isNumericPlaylistId(const QString &value)
{
    if (value.isEmpty() || value.size() > 20 || value.at(0) == QLatin1Char('0')) {
        return false;
    }

    for (int index = 0; index < value.size(); ++index) {
        if (!isAsciiDigit(value.at(index))) {
            return false;
        }
    }

    bool converted = false;
    const qulonglong numericValue = value.toULongLong(&converted, 10);
    return converted && numericValue != 0;
}

bool QqIdentifier::isSafeSourceUrl(const QUrl &url)
{
    if (!url.isValid()
            || url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0
            || url.host().compare(QStringLiteral("y.qq.com"), Qt::CaseInsensitive) != 0
            || !url.userName().isEmpty()
            || !url.password().isEmpty()
            || !url.fragment().isEmpty()) {
        return false;
    }

    const int port = url.port(-1);
    if (port != -1 && port != 443) {
        return false;
    }

    const QString path = url.path(QUrl::FullyDecoded);
    if (!path.startsWith(QStringLiteral("/n/ryqq/"))) {
        return false;
    }

    const QStringList segments = path.split(QLatin1Char('/'), QString::SkipEmptyParts);
    for (int index = 0; index < segments.size(); ++index) {
        if (segments.at(index) == QLatin1String(".")
                || segments.at(index) == QLatin1String("..")) {
            return false;
        }
    }
    return true;
}
