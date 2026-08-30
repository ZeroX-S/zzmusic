#include "tracklistmodel.h"

namespace {

bool sameTrack(const Track &left, const Track &right)
{
    return left.mid == right.mid
        && left.title == right.title
        && left.artist == right.artist
        && left.artistMid == right.artistMid
        && left.album == right.album
        && left.albumMid == right.albumMid
        && left.artworkUrl == right.artworkUrl
        && left.sourceUrl == right.sourceUrl
        && left.durationMs == right.durationMs
        && left.playable == right.playable;
}

}

TrackListModel::TrackListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TrackListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant TrackListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return QVariant();
    }
    const Track &item = m_items.at(index.row());
    switch (role) {
    case TrackMidRole:
        return item.mid;
    case TitleRole:
        return item.title;
    case ArtistRole:
        return item.artist;
    case AlbumRole:
        return item.album;
    case ArtworkRole:
        return item.artworkUrl;
    case DurationMsRole:
        return item.durationMs;
    case PlayableRole:
        return item.playable;
    case SourceUrlRole:
        return item.sourceUrl;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> TrackListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(TrackMidRole, "trackMid");
    roles.insert(TitleRole, "title");
    roles.insert(ArtistRole, "artist");
    roles.insert(AlbumRole, "album");
    roles.insert(ArtworkRole, "artwork");
    roles.insert(DurationMsRole, "durationMs");
    roles.insert(PlayableRole, "playable");
    roles.insert(SourceUrlRole, "sourceUrl");
    return roles;
}

int TrackListModel::count() const
{
    return m_items.size();
}

void TrackListModel::replace(const QVector<Track> &items)
{
    if (m_items.size() == items.size()) {
        int firstChanged = -1;
        int lastChanged = -1;
        for (int index = 0; index < items.size(); ++index) {
            if (!sameTrack(m_items.at(index), items.at(index))) {
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

void TrackListModel::append(const QVector<Track> &items)
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

void TrackListModel::clear()
{
    if (!m_items.isEmpty()) {
        replace(QVector<Track>());
    }
}

QVector<Track> TrackListModel::items() const
{
    return m_items;
}

bool TrackListModel::itemAt(int index, Track *item) const
{
    if (!item || index < 0 || index >= m_items.size()) {
        return false;
    }
    *item = m_items.at(index);
    return true;
}
