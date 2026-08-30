#include "settingsstore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace {

const int QueueSchemaVersion = 1;

bool exactKeys(const QJsonObject &object, const QStringList &keys)
{
    if (object.size() != keys.size()) {
        return false;
    }
    for (const QString &key : keys) {
        if (!object.contains(key)) {
            return false;
        }
    }
    return true;
}

QJsonObject trackObject(const Track &track)
{
    QJsonObject object;
    object.insert(QStringLiteral("mid"), track.mid);
    object.insert(QStringLiteral("title"), track.title);
    object.insert(QStringLiteral("artist"), track.artist);
    object.insert(QStringLiteral("artistMid"), track.artistMid);
    object.insert(QStringLiteral("album"), track.album);
    object.insert(QStringLiteral("albumMid"), track.albumMid);
    object.insert(QStringLiteral("artworkUrl"), track.artworkUrl.toString());
    object.insert(QStringLiteral("sourceUrl"), track.sourceUrl.toString());
    object.insert(QStringLiteral("durationMs"), double(track.durationMs));
    object.insert(QStringLiteral("playable"), track.playable);
    return object;
}

bool jsonInteger(const QJsonValue &value, qint64 minimum, qint64 maximum,
                 qint64 *result)
{
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble(-1);
    const qint64 integer = qint64(number);
    if (number != double(integer) || integer < minimum || integer > maximum) {
        return false;
    }
    if (result) {
        *result = integer;
    }
    return true;
}

bool parseTrack(const QJsonValue &value, Track *track)
{
    if (!track || !value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    const QStringList keys = QStringList()
        << QStringLiteral("mid") << QStringLiteral("title")
        << QStringLiteral("artist") << QStringLiteral("artistMid")
        << QStringLiteral("album") << QStringLiteral("albumMid")
        << QStringLiteral("artworkUrl") << QStringLiteral("sourceUrl")
        << QStringLiteral("durationMs") << QStringLiteral("playable");
    if (!exactKeys(object, keys)) {
        return false;
    }
    const QStringList stringKeys = QStringList()
        << QStringLiteral("mid") << QStringLiteral("title")
        << QStringLiteral("artist") << QStringLiteral("artistMid")
        << QStringLiteral("album") << QStringLiteral("albumMid")
        << QStringLiteral("artworkUrl") << QStringLiteral("sourceUrl");
    for (const QString &key : stringKeys) {
        if (!object.value(key).isString()) {
            return false;
        }
    }
    qint64 duration = 0;
    if (!object.value(QStringLiteral("playable")).isBool()
            || !jsonInteger(object.value(QStringLiteral("durationMs")),
                            0, 24 * 60 * 60 * 1000, &duration)) {
        return false;
    }
    Track parsed;
    parsed.mid = object.value(QStringLiteral("mid")).toString();
    parsed.title = object.value(QStringLiteral("title")).toString();
    parsed.artist = object.value(QStringLiteral("artist")).toString();
    parsed.artistMid = object.value(QStringLiteral("artistMid")).toString();
    parsed.album = object.value(QStringLiteral("album")).toString();
    parsed.albumMid = object.value(QStringLiteral("albumMid")).toString();
    parsed.artworkUrl = QUrl(object.value(QStringLiteral("artworkUrl")).toString());
    parsed.sourceUrl = QUrl(object.value(QStringLiteral("sourceUrl")).toString());
    parsed.durationMs = duration;
    parsed.playable = object.value(QStringLiteral("playable")).toBool();
    if (parsed.mid.isEmpty() || parsed.title.isEmpty()
            || (!parsed.artworkUrl.isEmpty()
                && parsed.artworkUrl.scheme() != QStringLiteral("https"))
            || (!parsed.sourceUrl.isEmpty()
                && parsed.sourceUrl.scheme() != QStringLiteral("https"))) {
        return false;
    }
    *track = parsed;
    return true;
}

}

SettingsStore::SettingsStore(const QString &directory, QObject *parent)
    : QObject(parent)
    , m_directory(directory.isEmpty()
          ? QDir(QStandardPaths::writableLocation(
                    QStandardPaths::AppConfigLocation))
                .filePath(QStringLiteral("harbour-qqmusic"))
          : directory)
{
    ensurePrivateDirectory();
}

qint64 SettingsStore::maximumCombinedCacheBytes()
{
    return qint64(128) * 1024 * 1024;
}

qint64 SettingsStore::networkCacheBytes()
{
    return qint64(32) * 1024 * 1024;
}

qint64 SettingsStore::defaultImageCacheBytes()
{
    return maximumCombinedCacheBytes() - networkCacheBytes();
}

bool SettingsStore::resumeEnabled() const
{
    QSettings settings(settingsPath(), QSettings::IniFormat);
    return settings.value(QStringLiteral("playback/resumeEnabled"), true).toBool();
}

void SettingsStore::setResumeEnabled(bool enabled)
{
    if (resumeEnabled() == enabled || !ensurePrivateDirectory()) {
        return;
    }
    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("playback/resumeEnabled"), enabled);
    settings.sync();
    secureSettingsFile();
    emit resumeEnabledChanged();
}

qint64 SettingsStore::cacheLimitBytes() const
{
    QSettings settings(settingsPath(), QSettings::IniFormat);
    const qint64 configured = settings.value(
        QStringLiteral("cache/combinedLimit"),
        maximumCombinedCacheBytes()).toLongLong();
    return qBound<qint64>(1, configured, maximumCombinedCacheBytes());
}

void SettingsStore::setCacheLimitBytes(qint64 bytes)
{
    const qint64 bounded = qBound<qint64>(1, bytes,
                                         maximumCombinedCacheBytes());
    if (cacheLimitBytes() == bounded || !ensurePrivateDirectory()) {
        return;
    }
    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("cache/combinedLimit"), bounded);
    settings.sync();
    secureSettingsFile();
    emit cacheLimitBytesChanged();
}

QString SettingsStore::queuePath() const
{
    return QDir(m_directory).filePath(QStringLiteral("queue-v1.json"));
}

QString SettingsStore::settingsPath() const
{
    return QDir(m_directory).filePath(QStringLiteral("settings.ini"));
}

bool SettingsStore::saveQueue(const QueueSnapshot &snapshot)
{
    if (!ensurePrivateDirectory()
            || snapshot.schemaVersion != QueueSchemaVersion
            || snapshot.repeatMode < 0 || snapshot.repeatMode > 3
            || snapshot.positionMs < 0) {
        return false;
    }
    QJsonArray tracks;
    for (const Track &track : snapshot.tracks) {
        if (track.mid.isEmpty() || track.title.isEmpty()) {
            return false;
        }
        tracks.append(trackObject(track));
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), QueueSchemaVersion);
    root.insert(QStringLiteral("currentMid"), snapshot.currentMid);
    root.insert(QStringLiteral("repeatMode"), snapshot.repeatMode);
    root.insert(QStringLiteral("positionMs"), double(snapshot.positionMs));
    root.insert(QStringLiteral("tracks"), tracks);
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (bytes.contains(".stream.qqmusic.qq.com")
            || bytes.contains("aqqmusic.tc.qq.com")
            || bytes.contains("qqmusic_key")
            || bytes.contains("mediaUrl")) {
        return false;
    }

    QSaveFile file(queuePath());
    if (!file.open(QIODevice::WriteOnly)
            || !file.setPermissions(QFileDevice::ReadOwner
                                    | QFileDevice::WriteOwner)
            || file.write(bytes) != bytes.size()
            || !file.commit()) {
        return false;
    }
    return QFile::setPermissions(queuePath(), QFileDevice::ReadOwner
                                              | QFileDevice::WriteOwner);
}

QueueSnapshot SettingsStore::loadQueue(bool *ok) const
{
    if (ok) {
        *ok = false;
    }
    QueueSnapshot snapshot;
    QFile file(queuePath());
    if (!file.exists()) {
        if (ok) {
            *ok = true;
        }
        return snapshot;
    }
    if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) {
        return snapshot;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return snapshot;
    }
    const QJsonObject root = document.object();
    const QStringList keys = QStringList()
        << QStringLiteral("version") << QStringLiteral("currentMid")
        << QStringLiteral("repeatMode") << QStringLiteral("positionMs")
        << QStringLiteral("tracks");
    qint64 version = 0;
    qint64 repeat = 0;
    qint64 position = 0;
    if (!exactKeys(root, keys)
            || !jsonInteger(root.value(QStringLiteral("version")),
                            QueueSchemaVersion, QueueSchemaVersion, &version)
            || !root.value(QStringLiteral("currentMid")).isString()
            || !jsonInteger(root.value(QStringLiteral("repeatMode")), 0, 3,
                            &repeat)
            || !jsonInteger(root.value(QStringLiteral("positionMs")), 0,
                            24 * 60 * 60 * 1000, &position)
            || !root.value(QStringLiteral("tracks")).isArray()) {
        return snapshot;
    }
    QVector<Track> parsedTracks;
    const QJsonArray tracks = root.value(QStringLiteral("tracks")).toArray();
    for (const QJsonValue &value : tracks) {
        Track track;
        if (!parseTrack(value, &track)) {
            return QueueSnapshot();
        }
        parsedTracks.append(track);
    }
    snapshot.tracks = parsedTracks;
    snapshot.currentMid = root.value(QStringLiteral("currentMid")).toString();
    snapshot.repeatMode = int(repeat);
    snapshot.positionMs = position;
    snapshot.schemaVersion = int(version);
    if (!snapshot.currentMid.isEmpty()) {
        bool found = false;
        for (const Track &track : snapshot.tracks) {
            found = found || track.mid == snapshot.currentMid;
        }
        if (!found) {
            return QueueSnapshot();
        }
    }
    if (ok) {
        *ok = true;
    }
    return snapshot;
}

bool SettingsStore::clearQueue()
{
    return !QFile::exists(queuePath()) || QFile::remove(queuePath());
}

bool SettingsStore::ensurePrivateDirectory() const
{
    if (!QDir().mkpath(m_directory)) {
        return false;
    }
    return QFile::setPermissions(m_directory,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner
        | QFileDevice::ExeOwner);
}

void SettingsStore::secureSettingsFile() const
{
    QFile::setPermissions(settingsPath(), QFileDevice::ReadOwner
                                          | QFileDevice::WriteOwner);
}
