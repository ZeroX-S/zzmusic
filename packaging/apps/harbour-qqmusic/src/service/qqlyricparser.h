#ifndef QQLYRICPARSER_H
#define QQLYRICPARSER_H

#include "core/types.h"

#include <QByteArray>
#include <QString>

class QqLyricParser
{
public:
    static qint64 parseTimestamp(const QString &timestamp);
    static QVector<LyricLine> parseLrc(const QString &lrc);
    static int currentIndex(const QVector<LyricLine> &lines,
                            qint64 positionMs);
    static ServiceResult<LyricDocument> parse(const QByteArray &payload,
                                              quint64 operationId);
};

#endif
