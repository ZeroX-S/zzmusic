#include "qqpublicparser.h"

#include "core/qqidentifier.h"
#include "core/qqjsonreader.h"
#include "qqrequestpolicy.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextDocumentFragment>

#include <limits>

namespace {

template<typename T>
ServiceResult<T> schemaChanged(quint64 operationId, const QString &diagnostic)
{
    ServiceResult<T> result;
    result.error.kind = ErrorKind::SchemaChanged;
    result.error.operationId = operationId;
    result.error.diagnosticCode = diagnostic;
    return result;
}

template<typename T, typename U>
ServiceResult<T> forwardError(const ServiceResult<U> &source)
{
    ServiceResult<T> result;
    result.error = source.error;
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

ServiceResult<Track> parseSong(const QJsonObject &song,
                               quint64 operationId,
                               bool allowNullSwitch = false)
{
    const bool modern = !song.contains(QStringLiteral("songmid"));
    const QString midKey = modern ? QStringLiteral("mid")
                                  : QStringLiteral("songmid");
    const QString titleKey = modern ? QStringLiteral("name")
                                    : QStringLiteral("songname");
    const QString albumMidKey = modern ? QStringLiteral("album.mid")
                                       : QStringLiteral("albummid");
    const QString albumNameKey = modern ? QStringLiteral("album.name")
                                        : QStringLiteral("albumname");
    QqJsonReader reader(objectDocument(song), operationId);

    const ServiceResult<QString> mid =
        reader.requiredString(QStringList() << midKey);
    if (!mid.ok()) {
        return forwardError<Track>(mid);
    }
    if (!QqIdentifier::isTrackMid(mid.value)) {
        return schemaChanged<Track>(
            operationId, QStringLiteral("qq.public.track.mid.invalid"));
    }

    const ServiceResult<QString> title =
        reader.requiredString(QStringList() << titleKey);
    if (!title.ok()) {
        return forwardError<Track>(title);
    }
    const QString decodedTitle = htmlDecode(title.value);
    if (!isNonEmpty(decodedTitle)) {
        return schemaChanged<Track>(
            operationId, QStringLiteral("qq.public.track.title.empty"));
    }

    const ServiceResult<QJsonArray> singers =
        reader.requiredArray(QStringList() << QStringLiteral("singer"));
    if (!singers.ok()) {
        return forwardError<Track>(singers);
    }
    if (singers.value.isEmpty() || !singers.value.first().isObject()) {
        return schemaChanged<Track>(
            operationId, QStringLiteral("qq.public.track.singer.object"));
    }
    QqJsonReader singerReader(
        objectDocument(singers.value.first().toObject()), operationId);
    const ServiceResult<QString> singerName =
        singerReader.requiredString(QStringList() << QStringLiteral("name"));
    if (!singerName.ok()) {
        return forwardError<Track>(singerName);
    }
    const QString decodedSingerName = htmlDecode(singerName.value);
    if (!isNonEmpty(decodedSingerName)) {
        return schemaChanged<Track>(
            operationId, QStringLiteral("qq.public.track.singer.empty"));
    }
    const ServiceResult<QString> singerMid =
        singerReader.optionalString(QStringList() << QStringLiteral("mid"));
    if (!singerMid.ok()) {
        return forwardError<Track>(singerMid);
    }

    ServiceResult<QString> albumMid;
    ServiceResult<QString> albumName;
    if (modern) {
        const ServiceResult<QJsonObject> album =
            reader.requiredObject(QStringList() << QStringLiteral("album"));
        if (!album.ok()) {
            return forwardError<Track>(album);
        }
        QqJsonReader albumReader(objectDocument(album.value), operationId);
        albumMid = albumReader.requiredString(
            QStringList() << QStringLiteral("mid"));
        albumName = albumReader.requiredString(
            QStringList() << QStringLiteral("name"));
    } else {
        albumMid = reader.requiredString(QStringList() << albumMidKey);
        albumName = reader.requiredString(QStringList() << albumNameKey);
    }
    if (!albumMid.ok()) {
        return forwardError<Track>(albumMid);
    }
    if (!albumName.ok()) {
        return forwardError<Track>(albumName);
    }
    const QString decodedAlbumName = htmlDecode(albumName.value);
    const bool albumless = albumMid.value.isEmpty()
        && albumName.value.isEmpty();
    if (!albumless) {
        if (!QqIdentifier::isTrackMid(albumMid.value)) {
            return schemaChanged<Track>(
                operationId, QStringLiteral("qq.public.track.albumMid.invalid"));
        }
        if (!isNonEmpty(decodedAlbumName)) {
            return schemaChanged<Track>(
                operationId, QStringLiteral("qq.public.track.album.empty"));
        }
    }

    bool playable = true;
    const QJsonValue rawSwitch = song.value(QStringLiteral("switch"));
    QStringList switchPath;
    if (!rawSwitch.isUndefined()
            && !(modern && allowNullSwitch && rawSwitch.isNull())) {
        switchPath << QStringLiteral("switch");
    } else if (modern) {
        const QJsonValue rawAction = song.value(QStringLiteral("action"));
        if (!rawAction.isUndefined() && !rawAction.isNull()) {
            switchPath << QStringLiteral("action")
                       << QStringLiteral("switch");
        }
    }
    if (!switchPath.isEmpty()) {
        const ServiceResult<qint64> switchValue =
            reader.requiredInteger(switchPath);
        if (!switchValue.ok()) {
            return forwardError<Track>(switchValue);
        }
        if (switchValue.value < 0) {
            return schemaChanged<Track>(
                operationId, QStringLiteral("qq.public.track.switch.invalid"));
        }
        playable = (switchValue.value & qint64(2)) != 0;
    }

    Track track;
    track.mid = mid.value;
    track.title = decodedTitle;
    track.artist = decodedSingerName;
    track.artistMid = isNonEmpty(singerMid.value) ? singerMid.value : QString();
    track.album = decodedAlbumName;
    track.albumMid = albumMid.value;
    if (!albumless) {
        track.artworkUrl = QUrl(QStringLiteral(
            "https://y.gtimg.cn/music/photo_new/T002R300x300M000%1.jpg")
            .arg(albumMid.value));
    }
    track.sourceUrl = QUrl(QStringLiteral(
        "https://y.qq.com/n/ryqq/songDetail/%1").arg(mid.value));
    track.playable = playable;
    return ServiceResult<Track>{track, ServiceError()};
}

template<typename T>
ServiceResult<T> requireJsonObjectPayload(const QByteArray &payload,
                                          quint64 operationId,
                                          const QString &diagnostic,
                                          QJsonDocument *document)
{
    QJsonParseError parseError;
    *document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document->isObject()) {
        return schemaChanged<T>(operationId, diagnostic);
    }
    return ServiceResult<T>();
}

ServiceResult<PlaylistSummary> parsePlaylistSummary(
    const QJsonObject &item,
    const QString &idKey,
    const QString &titleKey,
    const QString &artworkKey,
    quint64 operationId,
    const QString &diagnosticPrefix,
    bool allowUnavailableArtwork = false)
{
    QqJsonReader reader(objectDocument(item), operationId);
    ServiceResult<QString> id;
    if (item.value(idKey).isDouble()) {
        const ServiceResult<qint64> numericId =
            reader.requiredInteger(QStringList() << idKey);
        if (!numericId.ok()) {
            return forwardError<PlaylistSummary>(numericId);
        }
        id.value = QString::number(numericId.value);
    } else {
        id = reader.requiredString(QStringList() << idKey);
    }
    if (!id.ok()) {
        return forwardError<PlaylistSummary>(id);
    }
    if (!QqIdentifier::isNumericPlaylistId(id.value)) {
        return schemaChanged<PlaylistSummary>(
            operationId, diagnosticPrefix + QStringLiteral(".id.invalid"));
    }

    const ServiceResult<QString> title =
        reader.requiredString(QStringList() << titleKey);
    if (!title.ok()) {
        return schemaChanged<PlaylistSummary>(
            operationId, diagnosticPrefix + QStringLiteral(".title.string"));
    }
    const QString decodedTitle = htmlDecode(title.value);
    if (!isNonEmpty(decodedTitle)) {
        return schemaChanged<PlaylistSummary>(
            operationId, diagnosticPrefix + QStringLiteral(".title.empty"));
    }

    const ServiceResult<QString> artwork =
        reader.requiredString(QStringList() << artworkKey);
    if (!artwork.ok()) {
        return forwardError<PlaylistSummary>(artwork);
    }
    QUrl artworkUrl;
    if (!normalizeArtworkUrl(artwork.value, &artworkUrl)
            && !allowUnavailableArtwork) {
        return schemaChanged<PlaylistSummary>(
            operationId, diagnosticPrefix + QStringLiteral(".artwork.invalid"));
    }

    PlaylistSummary playlist;
    playlist.id = id.value;
    playlist.title = decodedTitle;
    playlist.artworkUrl = artworkUrl;
    playlist.sourceUrl = QUrl(QStringLiteral(
        "https://y.qq.com/n/ryqq/playlist/%1").arg(id.value));
    return ServiceResult<PlaylistSummary>{playlist, ServiceError()};
}

QString singleAttribute(const QString &tag, const QString &name)
{
    const QRegularExpression expression(
        QStringLiteral("(?:^|\\s)%1\\s*=\\s*\"([^\"]*)\"")
            .arg(QRegularExpression::escape(name)));
    QRegularExpressionMatchIterator matches = expression.globalMatch(tag);
    if (!matches.hasNext()) {
        return QString();
    }
    const QRegularExpressionMatch match = matches.next();
    if (matches.hasNext()) {
        return QString();
    }
    return match.captured(1);
}

}

ServiceResult<QVector<PlaylistSummary> > QqPublicParser::recommendations(
    const QByteArray &payload,
    quint64 operationId)
{
    QJsonDocument document;
    const ServiceResult<QVector<PlaylistSummary> > root =
        requireJsonObjectPayload<QVector<PlaylistSummary> >(
            payload, operationId,
            QStringLiteral("qq.public.recommendations.root.object"), &document);
    if (!root.ok()) {
        return root;
    }
    QqJsonReader reader(document, operationId);
    const ServiceResult<QJsonArray> items = reader.requiredArray(
        QStringList() << QStringLiteral("recomPlaylist")
                      << QStringLiteral("data")
                      << QStringLiteral("v_hot"));
    if (!items.ok()) {
        return forwardError<QVector<PlaylistSummary> >(items);
    }

    ServiceResult<QVector<PlaylistSummary> > result;
    for (int index = 0; index < items.value.size(); ++index) {
        if (!items.value.at(index).isObject()) {
            return schemaChanged<QVector<PlaylistSummary> >(
                operationId,
                QStringLiteral("qq.public.recommendation.item.object"));
        }
        const ServiceResult<PlaylistSummary> playlist = parsePlaylistSummary(
            items.value.at(index).toObject(), QStringLiteral("content_id"),
            QStringLiteral("title"), QStringLiteral("cover"), operationId,
            QStringLiteral("qq.public.recommendation"));
        if (!playlist.ok()) {
            return forwardError<QVector<PlaylistSummary> >(playlist);
        }
        result.value.append(playlist.value);
    }
    return result;
}

ServiceResult<QVector<CategoryGroup> > QqPublicParser::categories(
    const QByteArray &payload,
    quint64 operationId)
{
    QJsonDocument document;
    const ServiceResult<QVector<CategoryGroup> > root =
        requireJsonObjectPayload<QVector<CategoryGroup> >(
            payload, operationId,
            QStringLiteral("qq.public.categories.root.object"), &document);
    if (!root.ok()) {
        return root;
    }
    QqJsonReader reader(document, operationId);
    const ServiceResult<QJsonArray> groups = reader.requiredArray(
        QStringList() << QStringLiteral("data")
                      << QStringLiteral("categories"));
    if (!groups.ok()) {
        return forwardError<QVector<CategoryGroup> >(groups);
    }

    ServiceResult<QVector<CategoryGroup> > result;
    for (int groupIndex = 0; groupIndex < groups.value.size(); ++groupIndex) {
        if (!groups.value.at(groupIndex).isObject()) {
            return schemaChanged<QVector<CategoryGroup> >(
                operationId, QStringLiteral("qq.public.categoryGroup.object"));
        }
        QqJsonReader groupReader(
            objectDocument(groups.value.at(groupIndex).toObject()), operationId);
        const ServiceResult<qint64> usable = groupReader.requiredInteger(
            QStringList() << QStringLiteral("usable"));
        if (!usable.ok()) {
            return forwardError<QVector<CategoryGroup> >(usable);
        }
        if (usable.value != 1) {
            continue;
        }

        const ServiceResult<QString> groupName = groupReader.requiredString(
            QStringList() << QStringLiteral("categoryGroupName"));
        if (!groupName.ok()) {
            return forwardError<QVector<CategoryGroup> >(groupName);
        }
        const QString decodedGroupName = htmlDecode(groupName.value);
        if (!isNonEmpty(decodedGroupName)) {
            return schemaChanged<QVector<CategoryGroup> >(
                operationId, QStringLiteral("qq.public.categoryGroup.title.empty"));
        }
        const ServiceResult<QJsonArray> categories = groupReader.requiredArray(
            QStringList() << QStringLiteral("items"));
        if (!categories.ok()) {
            return forwardError<QVector<CategoryGroup> >(categories);
        }

        CategoryGroup group;
        group.title = decodedGroupName;
        for (int itemIndex = 0; itemIndex < categories.value.size(); ++itemIndex) {
            if (!categories.value.at(itemIndex).isObject()) {
                return schemaChanged<QVector<CategoryGroup> >(
                    operationId, QStringLiteral("qq.public.category.object"));
            }
            QqJsonReader categoryReader(
                objectDocument(categories.value.at(itemIndex).toObject()),
                operationId);
            const ServiceResult<qint64> id = categoryReader.requiredInteger(
                QStringList() << QStringLiteral("categoryId"));
            if (!id.ok()) {
                return forwardError<QVector<CategoryGroup> >(id);
            }
            if (id.value <= 0) {
                return schemaChanged<QVector<CategoryGroup> >(
                    operationId, QStringLiteral("qq.public.category.id.invalid"));
            }
            const ServiceResult<QString> title = categoryReader.requiredString(
                QStringList() << QStringLiteral("categoryName"));
            if (!title.ok()) {
                return forwardError<QVector<CategoryGroup> >(title);
            }
            const QString decodedTitle = htmlDecode(title.value);
            if (!isNonEmpty(decodedTitle)) {
                return schemaChanged<QVector<CategoryGroup> >(
                    operationId, QStringLiteral("qq.public.category.title.empty"));
            }
            Category category;
            category.id = QString::number(id.value);
            category.title = decodedTitle;
            group.items.append(category);
        }
        result.value.append(group);
    }
    return result;
}

ServiceResult<PlaylistPage> QqPublicParser::playlists(const QByteArray &payload,
                                                       quint64 operationId)
{
    QJsonDocument document;
    const ServiceResult<PlaylistPage> root =
        requireJsonObjectPayload<PlaylistPage>(
            payload, operationId,
            QStringLiteral("qq.public.playlists.root.object"), &document);
    if (!root.ok()) {
        return root;
    }
    QqJsonReader reader(document, operationId);
    const ServiceResult<QJsonArray> items = reader.requiredArray(
        QStringList() << QStringLiteral("data") << QStringLiteral("list"));
    if (!items.ok()) {
        return forwardError<PlaylistPage>(items);
    }
    const ServiceResult<qint64> total = reader.requiredInteger(
        QStringList() << QStringLiteral("data") << QStringLiteral("sum"));
    if (!total.ok()) {
        return forwardError<PlaylistPage>(total);
    }
    if (total.value < 0) {
        return schemaChanged<PlaylistPage>(
            operationId, QStringLiteral("qq.public.playlists.total.invalid"));
    }

    ServiceResult<PlaylistPage> result;
    result.value.total = total.value;
    for (int index = 0; index < items.value.size(); ++index) {
        if (!items.value.at(index).isObject()) {
            return schemaChanged<PlaylistPage>(
                operationId, QStringLiteral("qq.public.playlist.item.object"));
        }
        const ServiceResult<PlaylistSummary> playlist = parsePlaylistSummary(
            items.value.at(index).toObject(), QStringLiteral("dissid"),
            QStringLiteral("dissname"), QStringLiteral("imgurl"), operationId,
            QStringLiteral("qq.public.playlist"));
        if (!playlist.ok()) {
            return forwardError<PlaylistPage>(playlist);
        }
        result.value.items.append(playlist.value);
    }
    return result;
}

ServiceResult<TrackPage> QqPublicParser::searchTracks(const QByteArray &payload,
                                                      quint64 operationId)
{
    QJsonDocument document;
    const ServiceResult<TrackPage> root = requireJsonObjectPayload<TrackPage>(
        payload, operationId,
        QStringLiteral("qq.public.searchTracks.root.object"), &document);
    if (!root.ok()) {
        return root;
    }
    QqJsonReader reader(document, operationId);
    const ServiceResult<QJsonArray> items = reader.requiredArray(
        QStringList() << QStringLiteral("req") << QStringLiteral("data")
                      << QStringLiteral("body") << QStringLiteral("song")
                      << QStringLiteral("list"));
    if (!items.ok()) {
        return forwardError<TrackPage>(items);
    }
    const ServiceResult<qint64> total = reader.requiredInteger(
        QStringList() << QStringLiteral("req") << QStringLiteral("data")
                      << QStringLiteral("meta") << QStringLiteral("sum"));
    if (!total.ok()) {
        return forwardError<TrackPage>(total);
    }
    if (total.value < 0) {
        return schemaChanged<TrackPage>(
            operationId, QStringLiteral("qq.public.searchTracks.total.invalid"));
    }

    ServiceResult<TrackPage> result;
    result.value.total = total.value;
    for (int index = 0; index < items.value.size(); ++index) {
        if (!items.value.at(index).isObject()) {
            return schemaChanged<TrackPage>(
                operationId, QStringLiteral("qq.public.searchTracks.item.object"));
        }
        const ServiceResult<Track> track =
            parseSong(items.value.at(index).toObject(), operationId, true);
        if (!track.ok()) {
            return forwardError<TrackPage>(track);
        }
        result.value.items.append(track.value);
    }
    return result;
}

namespace {

ServiceResult<QVector<Track> > personalizedTracks(
    const QByteArray &payload,
    quint64 operationId,
    const QString &blockName,
    const QStringList &listNames,
    bool unwrapTrack,
    const QString &diagnosticPrefix)
{
    QJsonDocument document;
    const ServiceResult<QVector<Track> > root =
        requireJsonObjectPayload<QVector<Track> >(
            payload, operationId,
            diagnosticPrefix + QStringLiteral(".root.object"), &document);
    if (!root.ok()) {
        return root;
    }
    const QJsonValue blockValue = document.object().value(blockName);
    if (!blockValue.isObject()) {
        return schemaChanged<QVector<Track> >(
            operationId, diagnosticPrefix + QStringLiteral(".block.object"));
    }
    const QJsonValue dataValue = blockValue.toObject().value(
        QStringLiteral("data"));
    if (!dataValue.isObject()) {
        return schemaChanged<QVector<Track> >(
            operationId, diagnosticPrefix + QStringLiteral(".data.object"));
    }
    const QJsonObject data = dataValue.toObject();
    QJsonArray items;
    bool listFound = false;
    for (int index = 0; index < listNames.size(); ++index) {
        const QJsonValue candidate = data.value(listNames.at(index));
        if (candidate.isArray()) {
            items = candidate.toArray();
            listFound = true;
            break;
        }
    }
    if (!listFound) {
        return schemaChanged<QVector<Track> >(
            operationId, diagnosticPrefix + QStringLiteral(".list.array"));
    }

    ServiceResult<QVector<Track> > result;
    for (int index = 0; index < items.size(); ++index) {
        if (!items.at(index).isObject()) {
            return schemaChanged<QVector<Track> >(
                operationId, diagnosticPrefix + QStringLiteral(".item.object"));
        }
        QJsonObject song = items.at(index).toObject();
        if (unwrapTrack) {
            const QJsonValue trackValue = song.value(QStringLiteral("Track"));
            if (!trackValue.isObject()) {
                return schemaChanged<QVector<Track> >(
                    operationId,
                    diagnosticPrefix + QStringLiteral(".item.track.object"));
            }
            song = trackValue.toObject();
        }
        const ServiceResult<Track> track = parseSong(song, operationId, true);
        if (!track.ok()) {
            return forwardError<QVector<Track> >(track);
        }
        result.value.append(track.value);
    }
    return result;
}

}

ServiceResult<QVector<Track> > QqPublicParser::dailyThirtyTracks(
    const QByteArray &payload,
    quint64 operationId)
{
    return personalizedTracks(
        payload, operationId, QStringLiteral("radar"),
        QStringList() << QStringLiteral("VecSongs")
                      << QStringLiteral("songList")
                      << QStringLiteral("vec_song")
                      << QStringLiteral("tracks")
                      << QStringLiteral("List")
                      << QStringLiteral("data"),
        true, QStringLiteral("qq.personalized.daily30"));
}

ServiceResult<QVector<Track> > QqPublicParser::guessYouLikeTracks(
    const QByteArray &payload,
    quint64 operationId)
{
    return personalizedTracks(
        payload, operationId, QStringLiteral("radio"),
        QStringList() << QStringLiteral("track")
                      << QStringLiteral("songList")
                      << QStringLiteral("vec_song")
                      << QStringLiteral("tracks"),
        false, QStringLiteral("qq.personalized.guess"));
}

ServiceResult<PlaylistPage> QqPublicParser::searchPlaylists(
    const QByteArray &payload,
    quint64 operationId)
{
    QJsonDocument document;
    const ServiceResult<PlaylistPage> root =
        requireJsonObjectPayload<PlaylistPage>(
            payload, operationId,
            QStringLiteral("qq.public.searchPlaylists.root.object"), &document);
    if (!root.ok()) {
        return root;
    }
    QqJsonReader reader(document, operationId);
    const ServiceResult<QJsonArray> items = reader.requiredArray(
        QStringList() << QStringLiteral("req") << QStringLiteral("data")
                      << QStringLiteral("body") << QStringLiteral("songlist")
                      << QStringLiteral("list"));
    if (!items.ok()) {
        return forwardError<PlaylistPage>(items);
    }
    const ServiceResult<qint64> total = reader.requiredInteger(
        QStringList() << QStringLiteral("req") << QStringLiteral("data")
                      << QStringLiteral("meta") << QStringLiteral("sum"));
    if (!total.ok()) {
        return forwardError<PlaylistPage>(total);
    }
    if (total.value < 0) {
        return schemaChanged<PlaylistPage>(
            operationId,
            QStringLiteral("qq.public.searchPlaylists.total.invalid"));
    }

    ServiceResult<PlaylistPage> result;
    result.value.total = total.value;
    for (int index = 0; index < items.value.size(); ++index) {
        if (!items.value.at(index).isObject()) {
            return schemaChanged<PlaylistPage>(
                operationId,
                QStringLiteral("qq.public.searchPlaylist.item.object"));
        }
        const QJsonObject item = items.value.at(index).toObject();
        const ServiceResult<PlaylistSummary> parsed = parsePlaylistSummary(
            item, QStringLiteral("dissid"), QStringLiteral("dissname"),
            QStringLiteral("imgurl"), operationId,
            QStringLiteral("qq.public.searchPlaylist"), true);
        if (!parsed.ok()) {
            return forwardError<PlaylistPage>(parsed);
        }

        QqJsonReader itemReader(objectDocument(item), operationId);
        const ServiceResult<QJsonObject> creator = itemReader.requiredObject(
            QStringList() << QStringLiteral("creator"));
        if (!creator.ok()) {
            return forwardError<PlaylistPage>(creator);
        }
        QqJsonReader creatorReader(objectDocument(creator.value), operationId);
        const ServiceResult<QString> author = creatorReader.requiredString(
            QStringList() << QStringLiteral("name"));
        if (!author.ok()) {
            return forwardError<PlaylistPage>(author);
        }
        const QString decodedAuthor = htmlDecode(author.value);
        if (!isNonEmpty(decodedAuthor)) {
            return schemaChanged<PlaylistPage>(
                operationId,
                QStringLiteral("qq.public.searchPlaylist.author.empty"));
        }
        const ServiceResult<qint64> count = itemReader.requiredInteger(
            QStringList() << QStringLiteral("song_count"));
        if (!count.ok()) {
            return forwardError<PlaylistPage>(count);
        }
        if (count.value < 0 || count.value > 2147483647LL) {
            return schemaChanged<PlaylistPage>(
                operationId,
                QStringLiteral("qq.public.searchPlaylist.trackCount.invalid"));
        }

        PlaylistSummary playlist = parsed.value;
        playlist.author = decodedAuthor;
        playlist.trackCount = static_cast<int>(count.value);
        result.value.items.append(playlist);
    }
    return result;
}

ServiceResult<QVector<PlaylistSummary> > QqPublicParser::toplists(
    const QByteArray &payload,
    quint64 operationId)
{
    QJsonDocument document;
    const ServiceResult<QVector<PlaylistSummary> > root =
        requireJsonObjectPayload<QVector<PlaylistSummary> >(
            payload, operationId,
            QStringLiteral("qq.public.toplists.root.object"), &document);
    if (!root.ok()) {
        return root;
    }
    QqJsonReader reader(document, operationId);
    const ServiceResult<QJsonArray> items = reader.requiredArray(
        QStringList() << QStringLiteral("data") << QStringLiteral("topList"));
    if (!items.ok()) {
        return forwardError<QVector<PlaylistSummary> >(items);
    }

    ServiceResult<QVector<PlaylistSummary> > result;
    for (int index = 0; index < items.value.size(); ++index) {
        if (!items.value.at(index).isObject()) {
            return schemaChanged<QVector<PlaylistSummary> >(
                operationId, QStringLiteral("qq.public.toplist.item.object"));
        }
        QqJsonReader itemReader(
            objectDocument(items.value.at(index).toObject()), operationId);
        const ServiceResult<qint64> id = itemReader.requiredInteger(
            QStringList() << QStringLiteral("id"));
        if (!id.ok()) {
            return forwardError<QVector<PlaylistSummary> >(id);
        }
        if (id.value <= 0
                || id.value > std::numeric_limits<int>::max()) {
            return schemaChanged<QVector<PlaylistSummary> >(
                operationId, QStringLiteral("qq.public.toplist.id.invalid"));
        }
        const ServiceResult<QString> title = itemReader.requiredString(
            QStringList() << QStringLiteral("topTitle"));
        if (!title.ok()) {
            return forwardError<QVector<PlaylistSummary> >(title);
        }
        const QString decodedTitle = htmlDecode(title.value);
        if (!isNonEmpty(decodedTitle)) {
            return schemaChanged<QVector<PlaylistSummary> >(
                operationId, QStringLiteral("qq.public.toplist.title.empty"));
        }
        const ServiceResult<QString> artwork = itemReader.requiredString(
            QStringList() << QStringLiteral("picUrl"));
        if (!artwork.ok()) {
            return forwardError<QVector<PlaylistSummary> >(artwork);
        }
        QUrl artworkUrl;
        if (!normalizeArtworkUrl(artwork.value, &artworkUrl)) {
            return schemaChanged<QVector<PlaylistSummary> >(
                operationId, QStringLiteral("qq.public.toplist.artwork.invalid"));
        }

        PlaylistSummary playlist;
        playlist.id = QString::number(id.value);
        playlist.title = decodedTitle;
        playlist.author = QStringLiteral("QQ Music");
        playlist.artworkUrl = artworkUrl;
        playlist.sourceUrl = QUrl(QStringLiteral(
            "https://y.qq.com/n/ryqq/toplist/%1").arg(id.value));
        result.value.append(playlist);
    }
    return result;
}

ServiceResult<QString> QqPublicParser::toplistPeriod(const QByteArray &html,
                                                     int topId,
                                                     quint64 operationId)
{
    if (topId <= 0) {
        return schemaChanged<QString>(
            operationId, QStringLiteral("qq.public.toplistPeriod.topId.invalid"));
    }
    const QString source = QString::fromUtf8(html);
    const QRegularExpression tagExpression(
        QStringLiteral("<i(?:\\s+[^<>]*?)?>"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression tidExpression(
        QStringLiteral("^[^\\s\"<>]*/([1-9][0-9]*)$"));
    const QRegularExpression periodExpression(
        QStringLiteral("^[0-9]{4}(?:-[0-9]{2}-[0-9]{2}|_[0-9]{1,2})$"));
    QRegularExpressionMatchIterator tags = tagExpression.globalMatch(source);
    while (tags.hasNext()) {
        const QString tag = tags.next().captured(0);
        if (singleAttribute(tag, QStringLiteral("class"))
                != QStringLiteral("play_cover__btn c_tx_link js_icon_play")) {
            continue;
        }
        if (singleAttribute(tag, QStringLiteral("data-listkey")).isEmpty()
                || singleAttribute(tag, QStringLiteral("data-listname")).isEmpty()) {
            continue;
        }
        const QRegularExpressionMatch tid = tidExpression.match(
            singleAttribute(tag, QStringLiteral("data-tid")));
        if (!tid.hasMatch() || tid.captured(1).toInt() != topId) {
            continue;
        }
        const QString period = singleAttribute(tag, QStringLiteral("data-date"));
        if (!periodExpression.match(period).hasMatch()) {
            continue;
        }
        ServiceResult<QString> result;
        result.value = period;
        return result;
    }
    return schemaChanged<QString>(
        operationId, QStringLiteral("qq.public.toplistPeriod.missing"));
}

ServiceResult<PlaylistDetail> QqPublicParser::toplistDetail(
    const QByteArray &payload,
    int topId,
    quint64 operationId)
{
    if (topId <= 0) {
        return schemaChanged<PlaylistDetail>(
            operationId, QStringLiteral("qq.public.toplistDetail.topId.invalid"));
    }
    QJsonDocument document;
    const ServiceResult<PlaylistDetail> root =
        requireJsonObjectPayload<PlaylistDetail>(
            payload, operationId,
            QStringLiteral("qq.public.toplistDetail.root.object"), &document);
    if (!root.ok()) {
        return root;
    }
    QqJsonReader reader(document, operationId);
    const ServiceResult<QJsonArray> songs = reader.requiredArray(
        QStringList() << QStringLiteral("toplist") << QStringLiteral("data")
                      << QStringLiteral("songInfoList"));
    if (!songs.ok()) {
        return forwardError<PlaylistDetail>(songs);
    }
    const ServiceResult<QJsonObject> infoObject = reader.requiredObject(
        QStringList() << QStringLiteral("toplist") << QStringLiteral("data")
                      << QStringLiteral("data"));
    if (!infoObject.ok()) {
        return forwardError<PlaylistDetail>(infoObject);
    }
    QqJsonReader infoReader(objectDocument(infoObject.value), operationId);
    const ServiceResult<QString> title = infoReader.requiredString(
        QStringList() << QStringLiteral("title"));
    if (!title.ok()) {
        return forwardError<PlaylistDetail>(title);
    }
    const QString decodedTitle = htmlDecode(title.value);
    if (!isNonEmpty(decodedTitle)) {
        return schemaChanged<PlaylistDetail>(
            operationId, QStringLiteral("qq.public.toplistDetail.title.empty"));
    }
    const ServiceResult<QString> artwork = infoReader.requiredString(
        QStringList() << QStringLiteral("frontPicUrl"));
    if (!artwork.ok()) {
        return forwardError<PlaylistDetail>(artwork);
    }
    QUrl artworkUrl;
    if (!normalizeArtworkUrl(artwork.value, &artworkUrl)) {
        return schemaChanged<PlaylistDetail>(
            operationId, QStringLiteral("qq.public.toplistDetail.artwork.invalid"));
    }

    ServiceResult<PlaylistDetail> result;
    result.value.info.id = QString::number(topId);
    result.value.info.title = decodedTitle;
    result.value.info.author = QStringLiteral("QQ Music");
    result.value.info.artworkUrl = artworkUrl;
    result.value.info.sourceUrl = QUrl(QStringLiteral(
        "https://y.qq.com/n/ryqq/toplist/%1").arg(topId));
    for (int index = 0; index < songs.value.size(); ++index) {
        if (!songs.value.at(index).isObject()) {
            return schemaChanged<PlaylistDetail>(
                operationId,
                QStringLiteral("qq.public.toplistDetail.song.object"));
        }
        const ServiceResult<Track> track =
            parseSong(songs.value.at(index).toObject(), operationId, true);
        if (!track.ok()) {
            return forwardError<PlaylistDetail>(track);
        }
        result.value.tracks.append(track.value);
    }
    result.value.info.trackCount = result.value.tracks.size();
    return result;
}
