#pragma once

#include <QAbstractListModel>
#include <QImage>
#include <QPixmap>
#include <QTimer>
#include <QVector>

class RunningAppsModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    struct Item { qulonglong window = 0; QString title; QString iconSource; bool active = false; bool operator==(const Item &other) const { return window == other.window && title == other.title && iconSource == other.iconSource && active == other.active; } };
    enum Role { TitleRole = Qt::UserRole + 1, WindowRole, IconSourceRole, ActiveRole };
    explicit RunningAppsModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE void activate(const QString &windowHandle);
    Q_INVOKABLE void close(const QString &windowHandle);
private:
    void refresh();
    QVector<Item> items;
    QTimer timer;
};

class TrayModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    struct Item { qulonglong key = 0; qulonglong owner = 0; quint32 id = 0; quint32 callback = 0; quintptr icon = 0; QString tooltip; QImage image; QString automationId; int ordinal = 0; quintptr automationElement = 0; };
    enum Role { KeyRole = Qt::UserRole + 1, TooltipRole };
    explicit TrayModel(QObject *parent = nullptr);
    ~TrayModel() override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    QPixmap icon(qulonglong key, const QSize &requestedSize) const;
    Q_INVOKABLE void activate(const QString &key, int action);
private:
    void refresh();
    QVector<Item> items;
    QTimer timer;
};
