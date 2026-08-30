#ifndef APPMETADATA_H
#define APPMETADATA_H

#include <QString>

class AppMetadata
{
public:
    static QString applicationId();
    static QString displayName();
    static QString organizationName();
    static int schemaVersion();
};

#endif
