#ifndef QQPUBLICPARSER_H
#define QQPUBLICPARSER_H

#include "core/types.h"

#include <QByteArray>

struct PlaylistPage {
    QVector<PlaylistSummary> items;
    qint64 total = 0;
};

struct TrackPage {
    QVector<Track> items;
    qint64 total = 0;
};

class QqPublicParser
{
public:
    static ServiceResult<QVector<PlaylistSummary> > recommendations(
        const QByteArray &payload,
        quint64 operationId);
    static ServiceResult<QVector<CategoryGroup> > categories(
        const QByteArray &payload,
        quint64 operationId);
    static ServiceResult<PlaylistPage> playlists(const QByteArray &payload,
                                                  quint64 operationId);
    static ServiceResult<TrackPage> searchTracks(const QByteArray &payload,
                                                 quint64 operationId);
    static ServiceResult<QVector<Track> > dailyThirtyTracks(
        const QByteArray &payload,
        quint64 operationId);
    static ServiceResult<QVector<Track> > guessYouLikeTracks(
        const QByteArray &payload,
        quint64 operationId);
    static ServiceResult<PlaylistPage> searchPlaylists(const QByteArray &payload,
                                                       quint64 operationId);
    static ServiceResult<QVector<PlaylistSummary> > toplists(
        const QByteArray &payload,
        quint64 operationId);
    static ServiceResult<QString> toplistPeriod(const QByteArray &html,
                                                int topId,
                                                quint64 operationId);
    static ServiceResult<PlaylistDetail> toplistDetail(const QByteArray &payload,
                                                       int topId,
                                                       quint64 operationId);
};

#endif
