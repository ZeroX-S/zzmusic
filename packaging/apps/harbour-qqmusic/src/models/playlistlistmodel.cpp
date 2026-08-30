#include "playlistlistmodel.h"

namespace {

bool samePlaylist(const PlaylistSummary &left, const PlaylistSummary &right)
{
    return left.id == right.id
        && left.title == right.title
        && left.author == right.author
        && left.artworkUrl == right.artworkUrl
        && left.sourceUrl == right.sourceUrl
        && left.trackCount == right.trackCount;
}

}

PlaylistListModel::PlaylistListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PlaylistListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant PlaylistListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return QVariant();
    }
    const PlaylistSummary &item = m_items.at(index.row());
    switch (role) {
    case PlaylistIdRole:
        return item.id;
    case TitleRole:
        return item.title;
    case AuthorRole:
        return item.author;
    case ArtworkRole:
        return item.artworkUrl;
    case TrackCountRole:
        return item.trackCount;
    case SourceUrlRole:
        return item.sourceUrl;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> PlaylistListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(PlaylistIdRole, "playlistId");
    roles.insert(TitleRole, "title");
    roles.insert(AuthorRole, "author");
    roles.insert(ArtworkRole, "artwork");
    roles.insert(TrackCountRole, "trackCount");
    roles.insert(SourceUrlRole, "sourceUrl");
    return roles;
}

int PlaylistListModel::count() const
{
    return m_items.size();
}

void PlaylistListModel::replace(const QVector<PlaylistSummary> &items)
{
    if (m_items.size() == items.size()) {
        int firstChanged = -1;
        int lastChanged = -1;
        for (int index = 0; index < items.size(); ++index) {
            if (!samePlaylist(m_items.at(index), items.at(index))) {
                if (firstChanged < 0) {
                    firstChanged = index;
                }
                lastChanged = index;
            }
        }
        if (firstChanged < 0) {
            return;
        }
        m_items = items;
        emit dataChanged(index(firstChanged, 0), index(lastChanged, 0));
        return;
    }
    beginResetModel();
    m_items = items;
    endResetModel();
    emit countChanged();
}

void PlaylistListModel::append(const QVector<PlaylistSummary> &items)
{
    if (items.isEmpty()) {
        return;
    }
    const int first = m_items.size();
    beginInsertRows(QModelIndex(), first, first + items.size() - 1);
    m_items += items;
    endInsertRows();
    emit countChanged();
}

void PlaylistListModel::clear()
{
    if (!m_items.isEmpty()) {
        replace(QVector<PlaylistSummary>());
    }
}

QVector<PlaylistSummary> PlaylistListModel::items() const
{
    return m_items;
}
