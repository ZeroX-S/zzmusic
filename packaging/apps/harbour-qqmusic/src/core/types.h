#ifndef QQMUSIC_TYPES_H
#define QQMUSIC_TYPES_H

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QUrl>
#include <QVector>
#include <QtGlobal>

enum class ErrorKind {
    None,
    Offline,
    AuthenticationExpired,
    NotPlayable,
    RateLimited,
    SchemaChanged,
    Cancelled,
    Timeout,
    DecodeFailed,
    Internal
};

struct ServiceError {
    ErrorKind kind = ErrorKind::None;
    quint64 operationId = 0;
    QString diagnosticCode;
    int retryAfterMs = 0;
};

template<typename T>
struct ServiceResult {
    T value;
    ServiceError error;

    bool ok() const
    {
        return error.kind == ErrorKind::None;
    }
};

enum class PageState {
    Loading,
    Content,
    Empty,
    OfflineWithCache,
    Error
};

struct Track {
    QString mid;
    QString title;
    QString artist;
    QString artistMid;
    QString album;
    QString albumMid;
    QUrl artworkUrl;
    QUrl sourceUrl;
    qint64 durationMs = 0;
    bool playable = true;
};

struct PlaylistSummary {
    QString id;
    QString title;
    QString author;
    QUrl artworkUrl;
    QUrl sourceUrl;
    int trackCount = -1;
};

struct PlaylistDetail {
    PlaylistSummary info;
    QVector<Track> tracks;
    QString period;
};

struct Category {
    QString id;
    QString title;
};

struct CategoryGroup {
    QString title;
    QVector<Category> items;
};

struct UserProfile {
    QString uin;
    QString nickname;
    QUrl avatarUrl;
};

struct LyricLine {
    qint64 timeMs = 0;
    QString text;
    QString translation;
};

struct LyricDocument {
    QVector<LyricLine> lines;
};

struct PlaybackLocation {
    QUrl url;
    QString bitrate;
    QDateTime expiresAt;
    bool accountBound = false;
};

void registerQqMusicMetaTypes();

Q_DECLARE_METATYPE(ServiceError)
Q_DECLARE_METATYPE(ErrorKind)
Q_DECLARE_METATYPE(QVector<PlaylistSummary>)
Q_DECLARE_METATYPE(QVector<CategoryGroup>)
Q_DECLARE_METATYPE(QVector<Track>)
Q_DECLARE_METATYPE(PlaylistDetail)
Q_DECLARE_METATYPE(UserProfile)
Q_DECLARE_METATYPE(LyricDocument)
Q_DECLARE_METATYPE(PlaybackLocation)

#endif
