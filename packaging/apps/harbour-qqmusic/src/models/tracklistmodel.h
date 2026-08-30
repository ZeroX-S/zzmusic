#ifndef TRACKLISTMODEL_H
#define TRACKLISTMODEL_H

#include "core/types.h"

#include <QAbstractListModel>

class TrackListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        TrackMidRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        AlbumRole,
        ArtworkRole,
        DurationMsRole,
        PlayableRole,
        SourceUrlRole
    };
    Q_ENUMS(Role)

    explicit TrackListModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const;

    void replace(const QVector<Track> &items);
    void append(const QVector<Track> &items);
    Q_INVOKABLE void clear();
    QVector<Track> items() const;
    bool itemAt(int index, Track *item) const;

signals:
    void countChanged();

private:
    QVector<Track> m_items;
};

#endif
