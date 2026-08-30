#include "appmetadata.h"

QString AppMetadata::applicationId()
{
    return QStringLiteral("harbour-qqmusic");
}

QString AppMetadata::displayName()
{
    return QStringLiteral("ZZ Music");
}

QString AppMetadata::organizationName()
{
    return QStringLiteral("org.bonito.qqmusic");
}

int AppMetadata::schemaVersion()
{
    return 1;
}
