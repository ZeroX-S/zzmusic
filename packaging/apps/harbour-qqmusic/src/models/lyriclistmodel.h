#ifndef LYRICLISTMODEL_H
#define LYRICLISTMODEL_H

#include "core/types.h"

#include <QAbstractListModel>

class LyricListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)

public:
    enum Role {
        TimeMsRole = Qt::UserRole + 1,
        TextRole,
        TranslationRole,
        CurrentRole
    };
    Q_ENUMS(Role)

    explicit LyricListModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const;
    int currentIndex() const;

    void replace(const LyricDocument &document);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setPosition(qint64 positionMs);

signals:
    void countChanged();
    void currentIndexChanged();

private:
    QVector<LyricLine> m_lines;
    int m_currentIndex;
};

#endif
