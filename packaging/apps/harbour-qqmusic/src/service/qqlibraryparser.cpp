#include "qqlibraryparser.h"

#include "core/qqidentifier.h"
#include "core/qqjsonreader.h"
#include "qqrequestpolicy.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextDocumentFragment>

namespace {

template<typename T>
ServiceResult<T> schemaChanged(quint64 operationId,
                               const QString &diagnostic)
{
    ServiceResult<T> result;
    result.error.kind = ErrorKind::SchemaChanged;
    result.error.operationId = operationId;
    result.error.diagnosticCode = diagnostic;
    return result;
}

template<typename T>
ServiceResult<T> authenticationExpired(quint64 operationId,
                                       const QString &diagnostic)
{
    ServiceResult<T> result;
    result.error.kind = ErrorKind::AuthenticationExpired;
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

QString htmlDecode(const QString &value)
{
    return QTextDocumentFragment::fromHtml(value).toPlainText();
}

bool isNonEmpty(const QString &value)
{
    return !value.trimmed().isEmpty();
}

bool isHttpsUrl(const QUrl &url)
{
    return url.isValid()
        && url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
        && !url.host().isEmpty()
        && url.userName().isEmpty()
        && url.password().isEmpty();
}

bool normalizeArtworkUrl(const QString &value, QUrl *normalized)
{
    const QUrl original(value, QUrl::StrictMode);
    QString normalizedText = value;
    if (original.scheme().compare(QStringLiteral("http"),
                                  Qt::CaseInsensitive) == 0) {
        const int separator = value.indexOf(QStringLiteral("://"));
        if (separator < 0) {
            return false;
        }
        normalizedText = QStringLiteral("https") + value.mid(separator);
    }
    if (!QqRequestPolicy::allowsArtwork(normalizedText)) {
        return false;
    }
    const QUrl url(normalizedText, QUrl::StrictMode);
    if (!isHttpsUrl(url)) {
        return false;
    }
    *normalized = url;
    return true;
}

QJsonDocument objectDocument(const QJsonObject &object)
{
    return QJsonDocument(object);
}

template<typename T>
ServiceResult<T> requireObjectPayload(const QByteArray &payload,
                                      quint64 operationId,
                                      const QString &prefix,
                                      QJsonDocument *document)
{
    QJsonParseError parseError;
    *document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document->isObject()) {
        return schemaChanged<T>(
            operationId, prefix + QStringLiteral(".root.object"));
    }
    return ServiceResult<T>();
}

ServiceResult<Track> parseLegacySong(const QJsonObject &song,
                                     quint64 operationId)
{
    const QString prefix = QStringLiteral("qq.library.playlist.track");
    QqJsonReader reader(objectDocument(song), operationId);

    const ServiceResult<QString> mid = reader.requiredString(
        QStringList() << QStringLiteral("songmid"));
    if (!mid.ok()) {
        return endpointError<Track>(mid, prefix);
    }
    if (!QqIdentifier::isTrackMid(mid.value)) {
        return schemaChanged<Track>(
            operationId, prefix + QStringLiteral(".mid.invalid"));
    }

    const ServiceResult<QString> title = reader.requiredString(
        QStringList() << QStringLiteral("songname"));
    if (!title.ok()) {
        return endpointError<Track>(title, prefix);
    }
    const QString decodedTitle = htmlDecode(title.value);
    if (!isNonEmpty(decodedTitle)) {
        return schemaChanged<Track>(
            operationId, prefix + QStringLiteral(".title.empty"));
    }

    const ServiceResult<QJsonArray> singers = reader.requiredArray(
        QStringList() << QStringLiteral("singer"));
    if (!singers.ok()) {
        return endpointError<Track>(singers, prefix);
    }
    if (singers.value.isEmpty() || !singers.value.first().isObject()) {
        return schemaChanged<Track>(
            operationId, prefix + QStringLiteral(".singer.object"));
    }
    QqJsonReader singerReader(
        objectDocument(singers.value.first().toObject()), operationId);
    const ServiceResult<QString> singerName = singerReader.requiredString(
        QStringList() << QStringLiteral("name"));
    if (!singerName.ok()) {
        return endpointError<Track>(singerName, prefix + QStringLiteral(".singer"));
    }
    const QString decodedSinger = htmlDecode(singerName.value);
    if (!isNonEmpty(decodedSinger)) {
        return schemaChanged<Track>(
            operationId, prefix + QStringLiteral(".singer.name.empty"));
    }
    const ServiceResult<QString> singerMid = singerReader.optionalString(
        QStringList() << QStringLiteral("mid"));
    if (!singerMid.ok()) {
        return endpointError<Track>(singerMid,
                                    prefix + QStringLiteral(".singer"));
    }
    if (isNonEmpty(singerMid.value)
            && !QqIdentifier::isTrackMid(singerMid.value)) {
        return schemaChanged<Track>(
            operationId, prefix + QStringLiteral(".singer.mid.invalid"));
    }

    const ServiceResult<QString> albumMid = reader.requiredString(
        QStringList() << QStringLiteral("albummid"));
    if (!albumMid.ok()) {
        return endpointError<Track>(albumMid, prefix);
    }
    const ServiceResult<QString> albumName = reader.requiredString(
        QStringList() << QStringLiteral("albumname"));
    if (!albumName.ok()) {
        return endpointError<Track>(albumName, prefix);
    }
    const QString decodedAlbum = htmlDecode(albumName.value);
    const bool albumless = albumMid.value.isEmpty()
        && albumName.value.isEmpty();
    if (!albumless) {
        if (!QqIdentifier::isTrackMid(albumMid.value)) {
            return schemaChanged<Track>(
                operationId, prefix + QStringLiteral(".albumMid.invalid"));
        }
        if (!isNonEmpty(decodedAlbum)) {
            return schemaChanged<Track>(
                operationId, prefix + QStringLiteral(".album.empty"));
        }
    }

    const ServiceResult<qint64> switchValue = reader.optionalInteger(
        QStringList() << QStringLiteral("switch"), 2);
    if (!switchValue.ok()) {
        return endpointError<Track>(switchValue, prefix);
    }
    if (switchValue.value < 0) {
        return schemaChanged<Track>(
            operationId, prefix + QStringLiteral(".switch.invalid"));
    }

    Track track;
    track.mid = mid.value;
    track.title = decodedTitle;
    track.artist = decodedSinger;
    track.artistMid = isNonEmpty(singerMid.value) ? singerMid.value : QString();
    track.album = decodedAlbum;
    track.albumMid = albumMid.value;
    if (!albumless) {
        track.artworkUrl = QUrl(QStringLiteral(
            "https://y.gtimg.cn/music/photo_new/T002R300x300M000%1.jpg")
            .arg(albumMid.value));
    }
    track.sourceUrl = QUrl(QStringLiteral(
        "https://y.qq.com/n/ryqq/songDetail/%1").arg(mid.value));
    track.playable = (switchValue.value & qint64(2)) != 0;
    return ServiceResult<Track>{track, ServiceError()};
}

ServiceResult<PlaylistSummary> accountPlaylist(
    qint64 numericId,
    const QString &rawTitle,
    const QString &rawArtwork,
    quint64 operationId,
    const QString &prefix)
{
    const QString id = QString::number(numericId);
    if (!QqIdentifier::isNumericPlaylistId(id)) {
        return schemaChanged<PlaylistSummary>(
            operationId, prefix + QStringLiteral(".id.invalid"));
    }
    const QString title = htmlDecode(rawTitle);
    if (!isNonEmpty(title)) {
        return schemaChanged<PlaylistSummary>(
            operationId, prefix + QStringLiteral(".title.empty"));
    }
    QUrl artwork;
    if (!rawArtwork.trimmed().isEmpty()
            && !normalizeArtworkUrl(rawArtwork, &artwork)) {
        return schemaChanged<PlaylistSummary>(
            operationId, prefix + QStringLiteral(".artwork.invalid"));
    }

    PlaylistSummary playlist;
    playlist.id = id;
    playlist.title = title;
    playlist.artworkUrl = artwork;
    playlist.sourceUrl = QUrl(QStringLiteral(
        "https://y.qq.com/n/ryqq/playlist/%1").arg(id));
    return ServiceResult<PlaylistSummary>{playlist, ServiceError()};
}

ServiceResult<UserLibrary> parseCreated(const QByteArray &payload,
                                        quint64 operationId)
{
    const QString prefix = QStringLiteral("qq.library.created");
    QJsonDocument document;
    const ServiceResult<UserLibrary> root =
        requireObjectPayload<UserLibrary>(
            payload, operationId, prefix, &document);
    if (!root.ok()) {
        return root;
    }
    QqJsonReader reader(document, operationId);
    const ServiceResult<qint64> code = reader.optionalInteger(
        QStringList() << QStringLiteral("code"), 0);
    if (!code.ok()) {
        return endpointError<UserLibrary>(code, prefix);
    }
    if (code.value == 5000) {
        return authenticationExpired<UserLibrary>(
            operationId, prefix + QStringLiteral(".code.5000"));
    }
    const ServiceResult<QJsonArray> items = reader.requiredArray(
        QStringList() << QStringLiteral("data")
                      << QStringLiteral("disslist"));
    if (!items.ok()) {
        return endpointError<UserLibrary>(items, prefix);
    }

    ServiceResult<UserLibrary> result;
    for (int index = 0; index < items.value.size(); ++index) {
        if (!items.value.at(index).isObject()) {
            return schemaChanged<UserLibrary>(
                operationId, prefix + QStringLiteral(".item.object"));
        }
        QqJsonReader itemReader(
            objectDocument(items.value.at(index).toObject()), operationId);
        const ServiceResult<qint64> visibility = itemReader.requiredInteger(
            QStringList() << QStringLiteral("dir_show"));
        if (!visibility.ok()) {
            return endpointError<UserLibrary>(visibility, prefix);
        }
        const ServiceResult<qint64> id = itemReader.requiredInteger(
            QStringList() << QStringLiteral("tid"));
        if (!id.ok()) {
            return endpointError<UserLibrary>(id, prefix);
        }
        const ServiceResult<QString> title = itemReader.requiredString(
            QStringList() << QStringLiteral("diss_name"));
        if (!title.ok()) {
            return endpointError<UserLibrary>(title, prefix);
        }

        if (visibility.value <= 0) {
            const QString decodedTitle = htmlDecode(title.value);
            if (visibility.value != 0
                    || id.value <= 0
                    || decodedTitle != QString::fromUtf8("我喜欢")) {
                continue;
            }
            const ServiceResult<PlaylistSummary> liked = accountPlaylist(
                id.value, decodedTitle,
                QStringLiteral(
                    "https://y.gtimg.cn/mediastyle/y/img/cover_love_300.jpg"),
                operationId, prefix + QStringLiteral(".liked"));
            if (!liked.ok()) {
                return endpointError<UserLibrary>(liked, prefix);
            }
            result.value.liked.append(liked.value);
            continue;
        }

        const ServiceResult<QString> artwork = itemReader.requiredString(
            QStringList() << QStringLiteral("diss_cover"));
        if (!artwork.ok()) {
            return endpointError<UserLibrary>(artwork, prefix);
        }
        const ServiceResult<PlaylistSummary> playlist = accountPlaylist(
            id.value, title.value, artwork.value, operationId,
            prefix + QStringLiteral(".item"));
        if (!playlist.ok()) {
            return endpointError<UserLibrary>(playlist, prefix);
        }
        result.value.created.append(playlist.value);
    }
    return result;
}

ServiceResult<UserLibrary> parseCollected(const QByteArray &payload,
                                          quint64 operationId)
{
    const QString prefix = QStringLiteral("qq.library.collected");
    QJsonDocument document;
    const ServiceResult<UserLibrary> root =
        requireObjectPayload<UserLibrary>(
            payload, operationId, prefix, &document);
    if (!root.ok()) {
        return root;
    }
    QqJsonReader reader(document, operationId);
    const ServiceResult<qint64> code = reader.optionalInteger(
        QStringList() << QStringLiteral("code"), 0);
    if (!code.ok()) {
        return endpointError<UserLibrary>(code, prefix);
    }
    if (code.value == 4000) {
        return authenticationExpired<UserLibrary>(
            operationId, prefix + QStringLiteral(".code.4000"));
    }
    const ServiceResult<QJsonArray> items = reader.requiredArray(
        QStringList() << QStringLiteral("data")
                      << QStringLiteral("cdlist"));
    if (!items.ok()) {
        return endpointError<UserLibrary>(items, prefix);
    }

    ServiceResult<UserLibrary> result;
    for (int index = 0; index < items.value.size(); ++index) {
        if (!items.value.at(index).isObject()) {
            return schemaChanged<UserLibrary>(
                operationId, prefix + QStringLiteral(".item.object"));
        }
        QqJsonReader itemReader(
            objectDocument(items.value.at(index).toObject()), operationId);
        const ServiceResult<qint64> visibility = itemReader.requiredInteger(
            QStringList() << QStringLiteral("dir_show"));
        if (!visibility.ok()) {
            return endpointError<UserLibrary>(visibility, prefix);
        }
        if (visibility.value <= 0) {
            continue;
        }

        const ServiceResult<qint64> id = itemReader.requiredInteger(
            QStringList() << QStringLiteral("dissid"));
        if (!id.ok()) {
            return endpointError<UserLibrary>(id, prefix);
        }
        const ServiceResult<QString> title = itemReader.requiredString(
            QStringList() << QStringLiteral("dissname"));
        if (!title.ok()) {
            return endpointError<UserLibrary>(title, prefix);
        }
        const ServiceResult<QString> artwork = itemReader.requiredString(
            QStringList() << QStringLiteral("logo"));
        if (!artwork.ok()) {
            return endpointError<UserLibrary>(artwork, prefix);
        }
        const ServiceResult<PlaylistSummary> playlist = accountPlaylist(
            id.value, title.value, artwork.value, operationId,
            prefix + QStringLiteral(".item"));
        if (!playlist.ok()) {
            return endpointError<UserLibrary>(playlist, prefix);
        }
        result.value.collected.append(playlist.value);
    }
    return result;
}

}

ServiceResult<PlaylistDetail> QqLibraryParser::playlist(
    const QByteArray &payload,
    const QString &expectedNumericId,
    quint64 operationId)
{
    const QString prefix = QStringLiteral("qq.library.playlist");
    if (!QqIdentifier::isNumericPlaylistId(expectedNumericId)) {
        return schemaChanged<PlaylistDetail>(
            operationId, prefix + QStringLiteral(".expectedId.invalid"));
    }

    QJsonDocument document;
    const ServiceResult<PlaylistDetail> root =
        requireObjectPayload<PlaylistDetail>(
            payload, operationId, prefix, &document);
    if (!root.ok()) {
        return root;
    }
    QqJsonReader reader(document, operationId);
    const ServiceResult<QJsonArray> items = reader.requiredArray(
        QStringList() << QStringLiteral("cdlist"));
    if (!items.ok()) {
        return endpointError<PlaylistDetail>(items, prefix);
    }
    if (items.value.size() != 1 || !items.value.first().isObject()) {
        return schemaChanged<PlaylistDetail>(
            operationId, prefix + QStringLiteral(".item.object"));
    }

    QqJsonReader itemReader(
        objectDocument(items.value.first().toObject()), operationId);
    const ServiceResult<QString> id = itemReader.requiredString(
        QStringList() << QStringLiteral("disstid"));
    if (!id.ok()) {
        return schemaChanged<PlaylistDetail>(
            operationId, prefix + QStringLiteral(".id.string"));
    }
    if (!QqIdentifier::isNumericPlaylistId(id.value)
            || id.value != expectedNumericId) {
        return schemaChanged<PlaylistDetail>(
            operationId, prefix + QStringLiteral(".id.invalid"));
    }
    const ServiceResult<QString> title = itemReader.requiredString(
        QStringList() << QStringLiteral("dissname"));
    if (!title.ok()) {
        return endpointError<PlaylistDetail>(title, prefix);
    }
    const QString decodedTitle = htmlDecode(title.value);
    if (!isNonEmpty(decodedTitle)) {
        return schemaChanged<PlaylistDetail>(
            operationId, prefix + QStringLiteral(".title.empty"));
    }
    const ServiceResult<QString> artwork = itemReader.requiredString(
        QStringList() << QStringLiteral("logo"));
    if (!artwork.ok()) {
        return endpointError<PlaylistDetail>(artwork, prefix);
    }
    QUrl artworkUrl;
    if (!normalizeArtworkUrl(artwork.value, &artworkUrl)) {
        return schemaChanged<PlaylistDetail>(
            operationId, prefix + QStringLiteral(".artwork.invalid"));
    }
    const ServiceResult<QJsonArray> songs = itemReader.requiredArray(
        QStringList() << QStringLiteral("songlist"));
    if (!songs.ok()) {
        return endpointError<PlaylistDetail>(songs, prefix);
    }

    ServiceResult<PlaylistDetail> result;
    result.value.info.id = id.value;
    result.value.info.title = decodedTitle;
    result.value.info.artworkUrl = artworkUrl;
    result.value.info.sourceUrl = QUrl(QStringLiteral(
        "https://y.qq.com/n/ryqq/playlist/%1").arg(id.value));
    result.value.info.trackCount = songs.value.size();
    for (int index = 0; index < songs.value.size(); ++index) {
        if (!songs.value.at(index).isObject()) {
            return schemaChanged<PlaylistDetail>(
                operationId, prefix + QStringLiteral(".track.object"));
        }
        const ServiceResult<Track> track = parseLegacySong(
            songs.value.at(index).toObject(), operationId);
        if (!track.ok()) {
            return endpointError<PlaylistDetail>(track, prefix);
        }
        result.value.tracks.append(track.value);
    }
    return result;
}

ServiceResult<UserProfile> QqLibraryParser::user(const QByteArray &payload,
                                                 const QString &expectedUin,
                                                 quint64 operationId)
{
    const QString prefix = QStringLiteral("qq.library.user");
    if (!QqIdentifier::isNumericPlaylistId(expectedUin)) {
        return schemaChanged<UserProfile>(
            operationId, prefix + QStringLiteral(".uin.invalid"));
    }

    QJsonDocument document;
    const ServiceResult<UserProfile> root = requireObjectPayload<UserProfile>(
        payload, operationId, prefix, &document);
    if (!root.ok()) {
        return root;
    }
    QqJsonReader reader(document, operationId);
    const ServiceResult<QJsonObject> users = reader.requiredObject(
        QStringList() << QStringLiteral("base") << QStringLiteral("data")
                      << QStringLiteral("map_userinfo"));
    if (!users.ok()) {
        return endpointError<UserProfile>(users, prefix);
    }
    if (!users.value.contains(expectedUin)
            || !users.value.value(expectedUin).isObject()) {
        return schemaChanged<UserProfile>(
            operationId, prefix + QStringLiteral(".uin.object"));
    }

    QqJsonReader profileReader(
        objectDocument(users.value.value(expectedUin).toObject()), operationId);
    const ServiceResult<QString> nickname = profileReader.requiredString(
        QStringList() << QStringLiteral("nick"));
    if (!nickname.ok()) {
        return endpointError<UserProfile>(nickname, prefix);
    }
    const QString decodedNickname = htmlDecode(nickname.value);
    if (!isNonEmpty(decodedNickname)) {
        return schemaChanged<UserProfile>(
            operationId, prefix + QStringLiteral(".nickname.empty"));
    }
    const ServiceResult<QString> avatar = profileReader.requiredString(
        QStringList() << QStringLiteral("headurl"));
    if (!avatar.ok()) {
        return endpointError<UserProfile>(avatar, prefix);
    }
    QUrl avatarUrl;
    if (!avatar.value.trimmed().isEmpty()
            && !normalizeArtworkUrl(avatar.value, &avatarUrl)) {
        return schemaChanged<UserProfile>(
            operationId, prefix + QStringLiteral(".avatar.invalid"));
    }

    UserProfile profile;
    profile.uin = expectedUin;
    profile.nickname = decodedNickname;
    profile.avatarUrl = avatarUrl;
    return ServiceResult<UserProfile>{profile, ServiceError()};
}

ServiceResult<UserLibrary> QqLibraryParser::userLibrary(
    const QByteArray &createdPayload,
    const QByteArray &collectedPayload,
    quint64 operationId)
{
    const ServiceResult<UserLibrary> created =
        parseCreated(createdPayload, operationId);
    if (!created.ok()) {
        return created;
    }
    const ServiceResult<UserLibrary> collected =
        parseCollected(collectedPayload, operationId);
    if (!collected.ok()) {
        return collected;
    }

    ServiceResult<UserLibrary> result;
    result.value.liked = created.value.liked;
    result.value.created = created.value.created;
    result.value.collected = collected.value.collected;
    return result;
}

ServiceResult<UserLibrary> QqLibraryParser::userCreated(
    const QByteArray &payload,
    quint64 operationId)
{
    return parseCreated(payload, operationId);
}

ServiceResult<UserLibrary> QqLibraryParser::userCollected(
    const QByteArray &payload,
    quint64 operationId)
{
    return parseCollected(payload, operationId);
}
