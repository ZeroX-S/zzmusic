#include "qqplaybackparser.h"

#include "core/qqidentifier.h"
#include "core/qqjsonreader.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QUrlQuery>

#include <limits>

namespace {

template<typename T>
ServiceResult<T> playbackError(ErrorKind kind,
                               quint64 operationId,
                               const QString &diagnostic)
{
    ServiceResult<T> result;
    result.error.kind = kind;
    result.error.operationId = operationId;
    result.error.diagnosticCode = diagnostic;
    return result;
}

template<typename T, typename U>
ServiceResult<T> endpointError(const ServiceResult<U> &source,
                               const QString &prefix)
{
    ServiceResult<T> result;
    result.error = source.error;
    if (!source.error.diagnosticCode.startsWith(QStringLiteral("qq."))) {
        result.error.diagnosticCode = prefix;
        if (!source.error.diagnosticCode.isEmpty()) {
            result.error.diagnosticCode += QLatin1Char('.')
                + source.error.diagnosticCode;
        }
    }
    return result;
}

bool hasUserInfoSyntax(const QUrl &url)
{
    return url.authority(QUrl::FullyEncoded).contains(QLatin1Char('@'));
}

const QStringList &allowedMediaHosts()
{
    static const QStringList hosts = QStringList()
        << QStringLiteral("ws.stream.qqmusic.qq.com")
        << QStringLiteral("isure.stream.qqmusic.qq.com")
        << QStringLiteral("dl.stream.qqmusic.qq.com")
        << QStringLiteral("aqqmusic.tc.qq.com");
    return hosts;
}

bool hasAllowedRawMediaAuthority(const QString &urlText,
                                 const QString &scheme)
{
    const int separator = urlText.indexOf(QStringLiteral("://"));
    if (separator < 0
            || urlText.left(separator).compare(scheme,
                                               Qt::CaseInsensitive) != 0) {
        return false;
    }

    const int authorityStart = separator + 3;
    int authorityEnd = urlText.size();
    const QString delimiters = QStringLiteral("/?#");
    for (int index = 0; index < delimiters.size(); ++index) {
        const int position = urlText.indexOf(delimiters.at(index),
                                             authorityStart);
        if (position >= 0 && position < authorityEnd) {
            authorityEnd = position;
        }
    }

    const QString authority = urlText.mid(
        authorityStart, authorityEnd - authorityStart);
    const QStringList &hosts = allowedMediaHosts();
    for (int index = 0; index < hosts.size(); ++index) {
        const QString &host = hosts.at(index);
        const QString explicitDefaultPort = scheme == QLatin1String("http")
            ? QStringLiteral(":80") : QStringLiteral(":443");
        if (authority.compare(host, Qt::CaseInsensitive) == 0
                || authority.compare(host + explicitDefaultPort,
                                     Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool hasAllowedMediaOrigin(const QUrl &url)
{
    const QStringList &hosts = allowedMediaHosts();

    const int port = url.port(-1);
    return url.isValid()
        && !url.isEmpty()
        && url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
        && hosts.contains(url.host(), Qt::CaseInsensitive)
        && (port == -1 || port == 443)
        && !hasUserInfoSyntax(url)
        && url.userName().isEmpty()
        && url.password().isEmpty()
        && !url.hasFragment();
}

bool normalizedMediaOrigin(const QString &urlText, QUrl *normalized)
{
    QUrl url(urlText, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if ((scheme != QLatin1String("http")
         && scheme != QLatin1String("https"))
            || !hasAllowedRawMediaAuthority(urlText, scheme)
            || url.isRelative()
            || hasUserInfoSyntax(url)
            || !url.userName().isEmpty()
            || !url.password().isEmpty()
            || url.hasFragment()) {
        return false;
    }

    const int port = url.port(-1);
    if (scheme == QLatin1String("http")) {
        if (port != -1 && port != 80) {
            return false;
        }
        url.setScheme(QStringLiteral("https"));
        url.setPort(-1);
    } else if (port != -1 && port != 443) {
        return false;
    }
    if (!hasAllowedMediaOrigin(url)) {
        return false;
    }
    *normalized = url;
    return true;
}

struct PlaybackQuality
{
    const char *prefix;
    const char *suffix;
    const char *bitrate;
};

const PlaybackQuality PlaybackQualities[] = {
    { "F000", ".flac", "FLAC lossless" },
    { "M800", ".mp3", "320 kbps" },
    { "M500", ".mp3", "128 kbps" }
};

QString requestedFilename(const PlaybackQuality &quality,
                          const QString &trackMid)
{
    return QString::fromLatin1(quality.prefix) + trackMid + trackMid
        + QString::fromLatin1(quality.suffix);
}

bool isCompactQualityFilename(const QString &filename,
                              const PlaybackQuality &quality)
{
    const QString prefix = QString::fromLatin1(quality.prefix);
    const QString suffix = QString::fromLatin1(quality.suffix);
    if (filename.size() != prefix.size() + 14 + suffix.size()
            || !filename.startsWith(prefix)
            || !filename.endsWith(suffix)) {
        return false;
    }

    return QqIdentifier::isTrackMid(filename.mid(prefix.size(), 14));
}

bool isEmptyOrAsciiDecimal(const QString &value)
{
    for (int index = 0; index < value.size(); ++index) {
        const ushort character = value.at(index).unicode();
        if (character < '0' || character > '9') {
            return false;
        }
    }
    return true;
}

bool isSafeRelativePurl(const QUrl &url, QString *decodedPath)
{
    if (!url.isValid()
            || url.isEmpty()
            || !url.isRelative()
            || !url.scheme().isEmpty()
            || !url.authority().isEmpty()
            || hasUserInfoSyntax(url)
            || !url.userName().isEmpty()
            || !url.password().isEmpty()
            || url.hasFragment()) {
        return false;
    }

    const QString path = url.path(QUrl::FullyDecoded);
    if (path.isEmpty()
            || path.contains(QLatin1Char('/'))
            || path.contains(QLatin1Char('\\'))
            || path == QLatin1String(".")
            || path == QLatin1String("..")) {
        return false;
    }

    const QStringList allowedQueryKeys = QStringList()
        << QStringLiteral("fromtag")
        << QStringLiteral("guid")
        << QStringLiteral("src")
        << QStringLiteral("uin")
        << QStringLiteral("vkey");
    const QList<QPair<QString, QString> > queryItems =
        QUrlQuery(url).queryItems(QUrl::FullyDecoded);
    if (url.hasQuery() && queryItems.isEmpty()) {
        return false;
    }
    QStringList seenQueryKeys;
    for (int index = 0; index < queryItems.size(); ++index) {
        const QString &key = queryItems.at(index).first;
        const QString &value = queryItems.at(index).second;
        if (!allowedQueryKeys.contains(key)
                || seenQueryKeys.contains(key)
                || (key == QLatin1String("uin")
                    && !isEmptyOrAsciiDecimal(value))) {
            return false;
        }
        seenQueryKeys.append(key);
    }

    *decodedPath = path;
    return true;
}

}

ServiceResult<PlaybackLocation> QqPlaybackParser::parse(
    const QByteArray &payload,
    const QString &expectedTrackMid,
    const QDateTime &now,
    quint64 operationId)
{
    const QString prefix = QStringLiteral("qq.playback");
    if (!QqIdentifier::isTrackMid(expectedTrackMid)) {
        return playbackError<PlaybackLocation>(
            ErrorKind::Internal, operationId,
            prefix + QStringLiteral(".expectedId.invalid"));
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return playbackError<PlaybackLocation>(
            ErrorKind::SchemaChanged, operationId,
            prefix + QStringLiteral(".root.object"));
    }

    QqJsonReader reader(document, operationId);
    const ServiceResult<QJsonArray> midurlinfo = reader.requiredArray(
        QStringList() << QStringLiteral("req_1")
                      << QStringLiteral("data")
                      << QStringLiteral("midurlinfo"));
    if (!midurlinfo.ok()) {
        return endpointError<PlaybackLocation>(midurlinfo, prefix);
    }
    if (midurlinfo.value.isEmpty()) {
        return playbackError<PlaybackLocation>(
            ErrorKind::SchemaChanged, operationId,
            prefix + QStringLiteral(".midurlinfo.item.object"));
    }

    QString candidateFilename;
    QString candidatePurl;
    const PlaybackQuality *selectedQuality = 0;
    bool matchedExpectedFilename = false;
    for (uint qualityIndex = 0;
         qualityIndex < sizeof(PlaybackQualities) / sizeof(PlaybackQualities[0]);
         ++qualityIndex) {
        const PlaybackQuality &quality = PlaybackQualities[qualityIndex];
        const QString expectedFilename = requestedFilename(
            quality, expectedTrackMid);
        for (int locationIndex = 0;
             locationIndex < midurlinfo.value.size(); ++locationIndex) {
            if (!midurlinfo.value.at(locationIndex).isObject()) {
                return playbackError<PlaybackLocation>(
                    ErrorKind::SchemaChanged, operationId,
                    prefix + QStringLiteral(".midurlinfo.item.object"));
            }
            QqJsonReader locationReader(
                QJsonDocument(midurlinfo.value.at(locationIndex).toObject()),
                operationId);
            const ServiceResult<QString> filename =
                locationReader.requiredString(
                    QStringList() << QStringLiteral("filename"));
            if (!filename.ok()) {
                return endpointError<PlaybackLocation>(filename, prefix);
            }
            if (filename.value != expectedFilename) {
                continue;
            }
            matchedExpectedFilename = true;
            const ServiceResult<QString> purl = locationReader.requiredString(
                QStringList() << QStringLiteral("purl"));
            if (!purl.ok()) {
                return endpointError<PlaybackLocation>(purl, prefix);
            }
            if (!purl.value.isEmpty()) {
                candidateFilename = filename.value;
                candidatePurl = purl.value;
                selectedQuality = &quality;
            }
            break;
        }
        if (selectedQuality) {
            break;
        }
    }
    if (!matchedExpectedFilename) {
        return playbackError<PlaybackLocation>(
            ErrorKind::SchemaChanged, operationId,
            prefix + QStringLiteral(".filename.invalid"));
    }
    if (!selectedQuality) {
        return playbackError<PlaybackLocation>(
            ErrorKind::NotPlayable, operationId,
            prefix + QStringLiteral(".purl.empty"));
    }

    const QUrl relativePurl(candidatePurl, QUrl::StrictMode);
    QString purlPath;
    if (!isSafeRelativePurl(relativePurl, &purlPath)) {
        return playbackError<PlaybackLocation>(
            ErrorKind::SchemaChanged, operationId,
            prefix + QStringLiteral(".purl.invalid"));
    }
    if (purlPath != candidateFilename
            && !isCompactQualityFilename(purlPath, *selectedQuality)) {
        return playbackError<PlaybackLocation>(
            ErrorKind::SchemaChanged, operationId,
            prefix + QStringLiteral(".purl.filename"));
    }

    const ServiceResult<QJsonArray> sip = reader.requiredArray(
        QStringList() << QStringLiteral("req_1")
                      << QStringLiteral("data")
                      << QStringLiteral("sip"));
    if (!sip.ok()) {
        return endpointError<PlaybackLocation>(sip, prefix);
    }
    if (sip.value.isEmpty() || !sip.value.first().isString()) {
        return playbackError<PlaybackLocation>(
            ErrorKind::SchemaChanged, operationId,
            prefix + QStringLiteral(".sip.item.string"));
    }

    const QString sipText = sip.value.first().toString();
    QUrl sipUrl;
    if (!normalizedMediaOrigin(sipText, &sipUrl)) {
        return playbackError<PlaybackLocation>(
            ErrorKind::SchemaChanged, operationId,
            prefix + QStringLiteral(".url.invalid"));
    }

    const QUrl location = sipUrl.resolved(relativePurl);
    const QString expectedPath = QLatin1Char('/') + purlPath;
    if (!hasAllowedMediaOrigin(location)
            || location.path(QUrl::FullyDecoded) != expectedPath) {
        return playbackError<PlaybackLocation>(
            ErrorKind::SchemaChanged, operationId,
            prefix + QStringLiteral(".url.invalid"));
    }

    if (!now.isValid()) {
        return playbackError<PlaybackLocation>(
            ErrorKind::Internal, operationId,
            prefix + QStringLiteral(".expiry.invalid"));
    }
    const qint64 lifetimeMs = 10 * 60 * 1000;
    const qint64 nowMs = now.toMSecsSinceEpoch();
    if (nowMs > std::numeric_limits<qint64>::max() - lifetimeMs) {
        return playbackError<PlaybackLocation>(
            ErrorKind::Internal, operationId,
            prefix + QStringLiteral(".expiry.invalid"));
    }

    const qint64 expiryMs = nowMs + lifetimeMs;
    const QDateTime expiresAt =
        QDateTime::fromMSecsSinceEpoch(expiryMs, Qt::UTC);
    if (!expiresAt.isValid()
            || expiryMs <= nowMs
            || expiresAt.toMSecsSinceEpoch() != expiryMs) {
        return playbackError<PlaybackLocation>(
            ErrorKind::Internal, operationId,
            prefix + QStringLiteral(".expiry.invalid"));
    }

    ServiceResult<PlaybackLocation> result;
    result.value.url = location;
    result.value.bitrate = QString::fromLatin1(selectedQuality->bitrate);
    result.value.expiresAt = expiresAt;
    return result;
}
