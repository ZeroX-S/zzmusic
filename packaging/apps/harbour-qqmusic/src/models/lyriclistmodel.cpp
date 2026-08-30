#include "lyriclistmodel.h"

LyricListModel::LyricListModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_currentIndex(-1)
{
}

int LyricListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_lines.size();
}

QVariant LyricListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lines.size()) {
        return QVariant();
    }
    const LyricLine &line = m_lines.at(index.row());
    switch (role) {
    case TimeMsRole:
        return line.timeMs;
    case TextRole:
        return line.text;
    case TranslationRole:
        return line.translation;
    case CurrentRole:
        return index.row() == m_currentIndex;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> LyricListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(TimeMsRole, "timeMs");
    roles.insert(TextRole, "text");
    roles.insert(TranslationRole, "translation");
    roles.insert(CurrentRole, "current");
    return roles;
}

int LyricListModel::count() const
{
    return m_lines.size();
}

int LyricListModel::currentIndex() const
{
    return m_currentIndex;
}

void LyricListModel::replace(const LyricDocument &document)
{
    beginResetModel();
    m_lines = document.lines;
    m_currentIndex = -1;
    endResetModel();
    emit countChanged();
    emit currentIndexChanged();
}

void LyricListModel::clear()
{
    if (!m_lines.isEmpty() || m_currentIndex != -1) {
        replace(LyricDocument());
    }
}

void LyricListModel::setPosition(qint64 positionMs)
{
    int first = 0;
    int pastLast = m_lines.size();
    while (first < pastLast) {
        const int middle = first + (pastLast - first) / 2;
        if (m_lines.at(middle).timeMs <= positionMs) {
            first = middle + 1;
        } else {
            pastLast = middle;
        }
    }
    const int next = first - 1;
    if (next == m_currentIndex) {
        return;
    }
    const int previous = m_currentIndex;
    m_currentIndex = next;
    const QVector<int> roles = QVector<int>() << CurrentRole;
    if (previous >= 0) {
        emit dataChanged(index(previous, 0), index(previous, 0), roles);
    }
    if (m_currentIndex >= 0) {
        emit dataChanged(index(m_currentIndex, 0),
                         index(m_currentIndex, 0),
                         roles);
    }
    emit currentIndexChanged();
}
