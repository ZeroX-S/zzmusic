#ifndef QQPLAYBACKPARSER_H
#define QQPLAYBACKPARSER_H

#include "core/types.h"

#include <QByteArray>
#include <QDateTime>

class QqPlaybackParser
{
public:
    static ServiceResult<PlaybackLocation> parse(const QByteArray &payload,
                                                 const QString &expectedTrackMid,
                                                 const QDateTime &now,
                                                 quint64 operationId);
};

#endif
