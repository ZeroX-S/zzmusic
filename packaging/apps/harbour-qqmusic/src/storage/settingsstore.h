#ifndef QQMUSIC_SETTINGSSTORE_H
#define QQMUSIC_SETTINGSSTORE_H

#include "core/types.h"

#include <QObject>
#include <QVector>

struct QueueSnapshot
{
    QVector<Track> tracks;
    QString currentMid;
    int repeatMode = 0;
    qint64 positionMs = 0;
    int schemaVersion = 1;
};

class SettingsStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool resumeEnabled READ resumeEnabled WRITE setResumeEnabled
               NOTIFY resumeEnabledChanged)
    Q_PROPERTY(qint64 cacheLimitBytes READ cacheLimitBytes WRITE setCacheLimitBytes
               NOTIFY cacheLimitBytesChanged)

public:
    explicit SettingsStore(const QString &directory = QString(),
                           QObject *parent = 0);

    static qint64 maximumCombinedCacheBytes();
    static qint64 networkCacheBytes();
    static qint64 defaultImageCacheBytes();

    bool resumeEnabled() const;
    void setResumeEnabled(bool enabled);
    qint64 cacheLimitBytes() const;
    void setCacheLimitBytes(qint64 bytes);
    QString queuePath() const;
    QString settingsPath() const;

    bool saveQueue(const QueueSnapshot &snapshot);
    QueueSnapshot loadQueue(bool *ok = 0) const;
    bool clearQueue();

signals:
    void resumeEnabledChanged();
    void cacheLimitBytesChanged();

private:
    bool ensurePrivateDirectory() const;
    void secureSettingsFile() const;

    QString m_directory;
};

#endif
