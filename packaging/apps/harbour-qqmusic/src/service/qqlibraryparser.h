#ifndef QQLIBRARYPARSER_H
#define QQLIBRARYPARSER_H

#include "core/types.h"

#include <QByteArray>

struct UserLibrary {
    QVector<PlaylistSummary> liked;
    QVector<PlaylistSummary> created;
    QVector<PlaylistSummary> collected;
};

class QqLibraryParser
{
public:
    static ServiceResult<PlaylistDetail> playlist(
        const QByteArray &payload,
        const QString &expectedNumericId,
        quint64 operationId);
    static ServiceResult<UserProfile> user(const QByteArray &payload,
                                           const QString &expectedUin,
                                           quint64 operationId);
    static ServiceResult<UserLibrary> userCreated(
        const QByteArray &payload,
        quint64 operationId);
    static ServiceResult<UserLibrary> userCollected(
        const QByteArray &payload,
        quint64 operationId);
    static ServiceResult<UserLibrary> userLibrary(
        const QByteArray &createdPayload,
        const QByteArray &collectedPayload,
        quint64 operationId);
};

#endif
