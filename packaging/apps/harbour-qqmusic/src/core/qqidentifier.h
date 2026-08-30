#ifndef QQIDENTIFIER_H
#define QQIDENTIFIER_H

#include <QString>
#include <QUrl>

namespace QqIdentifier {

bool isTrackMid(const QString &value);
bool isNumericPlaylistId(const QString &value);
bool isSafeSourceUrl(const QUrl &url);

}

#endif
