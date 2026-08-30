#include "types.h"

void registerQqMusicMetaTypes()
{
    qRegisterMetaType<ServiceError>("ServiceError");
    qRegisterMetaType<ErrorKind>("ErrorKind");
    qRegisterMetaType<QVector<PlaylistSummary> >("QVector<PlaylistSummary>");
    qRegisterMetaType<QVector<CategoryGroup> >("QVector<CategoryGroup>");
    qRegisterMetaType<QVector<Track> >("QVector<Track>");
    qRegisterMetaType<PlaylistDetail>("PlaylistDetail");
    qRegisterMetaType<UserProfile>("UserProfile");
    qRegisterMetaType<LyricDocument>("LyricDocument");
    qRegisterMetaType<PlaybackLocation>("PlaybackLocation");
}

Q_CONSTRUCTOR_FUNCTION(registerQqMusicMetaTypes)
