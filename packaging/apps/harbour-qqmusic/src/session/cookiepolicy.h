#ifndef COOKIEPOLICY_H
#define COOKIEPOLICY_H

#include "core/types.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVector>

struct CookieRecord
{
    QByteArray name;
    QByteArray value;
    QString domain;
    QString path;
    bool secure = true;
    QDateTime expiresAt;
};

typedef ServiceResult<CookieRecord> CookieValidation;

class CookiePolicy
{
public:
    static int maximumCookieBytes();
    static int maximumPayloadBytes();
    static bool isAllowedName(const QByteArray &name);
    static CookieValidation validate(const CookieRecord &record,
                                     const QDateTime &nowUtc);
    static ServiceResult<QVector<CookieRecord> > validateAll(
        const QVector<CookieRecord> &records,
        const QDateTime &nowUtc);
    static QByteArray serialize(const QVector<CookieRecord> &records);
    static ServiceResult<QVector<CookieRecord> > deserialize(
        const QByteArray &payload,
        const QDateTime &nowUtc);
};

#endif
