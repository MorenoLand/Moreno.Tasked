#pragma once

#include <QAbstractListModel>
#include <QImage>
#include <QPixmap>
#include <QSortFilterProxyModel>
#include <QStringList>
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
    Q_INVOKABLE void move(int from, int to);
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
    Q_INVOKABLE bool isOverflow(const QString &key) const;
    Q_INVOKABLE void setOverflow(const QString &key, bool enabled);
signals:
    void overflowChanged();
private:
    void refresh();
    QVector<Item> items;
    QStringList overflowKeys;
    QTimer timer;
};

class TrayFilterModel final : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    TrayFilterModel(TrayModel *source, bool overflow, QObject *parent = nullptr);
    int count() const { return rowCount(); }
signals:
    void countChanged();
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
private:
    void refreshFilter();
    TrayModel *tray = nullptr;
    bool overflowOnly = false;
};
