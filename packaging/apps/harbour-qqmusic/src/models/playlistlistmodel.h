#ifndef PLAYLISTLISTMODEL_H
#define PLAYLISTLISTMODEL_H

#include "core/types.h"

#include <QAbstractListModel>

class PlaylistListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        PlaylistIdRole = Qt::UserRole + 1,
        TitleRole,
        AuthorRole,
        ArtworkRole,
        TrackCountRole,
        SourceUrlRole
    };
    Q_ENUMS(Role)

    explicit PlaylistListModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const;

    void replace(const QVector<PlaylistSummary> &items);
    void append(const QVector<PlaylistSummary> &items);
    Q_INVOKABLE void clear();
    QVector<PlaylistSummary> items() const;

signals:
    void countChanged();

private:
    QVector<PlaylistSummary> m_items;
};

#endif
