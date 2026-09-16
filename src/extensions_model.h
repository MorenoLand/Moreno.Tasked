#pragma once

#include <QAbstractListModel>
#include <QVariantList>

class ExtensionModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    struct Item { QString id; QString name; QString version; QString description; QString path; bool enabled = true; QVariantList settings; };
    enum Role { IdRole = Qt::UserRole + 1, NameRole, VersionRole, DescriptionRole, PathRole, EnabledRole, SettingsRole };
    explicit ExtensionModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setEnabled(const QString &id, bool enabled);
    Q_INVOKABLE void setSetting(const QString &id, const QString &key, const QVariant &value);
private:
    QVector<Item> scan() const;
    QVector<Item> items;
};
