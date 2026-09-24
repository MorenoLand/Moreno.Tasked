#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QReadWriteLock>
#include <QRect>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QTimer>
#include <QVector>

class RunningAppsModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    struct Item { qulonglong window = 0; QString title; QString iconSource; bool active = false; QString appId; bool operator==(const Item &other) const { return window == other.window && title == other.title && iconSource == other.iconSource && active == other.active && appId == other.appId; } };
    enum Role { TitleRole = Qt::UserRole + 1, WindowRole, IconSourceRole, ActiveRole, AppIdRole };
    explicit RunningAppsModel(QObject *parent = nullptr);
    ~RunningAppsModel() override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE void activate(const QString &windowHandle);
    Q_INVOKABLE void activate(const QString &windowHandle, bool wasActiveAtPress);
    Q_INVOKABLE void showTaskMenu(const QString &windowHandle, int x, int y);
    Q_INVOKABLE void close(const QString &windowHandle);
    Q_INVOKABLE void move(int from, int to);
    bool isAppRunning(const QString &appId) const;
private:
    void refresh();
    QVector<Item> items;
    QTimer timer;
};

class PinnedAppsModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    struct Item { QString appId; QString title; QString iconSource; bool operator==(const Item &other) const { return appId == other.appId && title == other.title && iconSource == other.iconSource; } };
    enum Role { TitleRole = Qt::UserRole + 1, AppIdRole, IconSourceRole };
    explicit PinnedAppsModel(RunningAppsModel *running, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return rowCount(); }
    Q_INVOKABLE void launch(const QString &appId);
    Q_INVOKABLE void showContextMenu(const QString &appId, int x, int y);
signals:
    void countChanged();
private:
    void refresh();
    RunningAppsModel *running = nullptr;
    QVector<Item> items;
    QTimer timer;
};

class TaskbarAppsModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    struct Item { QString appId; QString title; QString iconSource; QString windowHandle; bool active = false; bool pinned = false; bool running = false; bool operator==(const Item &other) const { return appId == other.appId && title == other.title && iconSource == other.iconSource && windowHandle == other.windowHandle && active == other.active && pinned == other.pinned && running == other.running; } };
    enum Role { TitleRole = Qt::UserRole + 1, WindowRole, IconSourceRole, ActiveRole, AppIdRole, PinnedRole, RunningRole };
    explicit TaskbarAppsModel(RunningAppsModel *running, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return rowCount(); }
    Q_INVOKABLE void launchPinned(const QString &appId);
    Q_INVOKABLE void showPinnedTaskMenu(const QString &appId, int x, int y);
    Q_INVOKABLE void move(int from, int to);
signals:
    void countChanged();
private:
    void refresh();
    RunningAppsModel *running = nullptr;
    QStringList taskbarOrder;
    QVector<Item> items;
    QTimer timer;
};

class TrayModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(quint64 iconRevision READ iconRevision NOTIFY iconRevisionChanged)
public:
    struct Item { qulonglong key = 0; qulonglong owner = 0; quint32 id = 0; quint32 callback = 0; quint32 version = 0; quintptr icon = 0; QString tooltip; QImage iconImage; QString automationId; int ordinal = 0; quintptr automationElement = 0; QRect bounds; };
    enum Role { KeyRole = Qt::UserRole + 1, TooltipRole };
    explicit TrayModel(QObject *parent = nullptr);
    ~TrayModel() override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    QPixmap icon(qulonglong key, const QSize &requestedSize) const;
    quint64 iconRevision() const { return imageRevision; }
    Q_INVOKABLE void activate(const QString &key, int action);
    Q_INVOKABLE void showContextMenu(const QString &key, int x, int y);
    Q_INVOKABLE bool isOverflow(const QString &key) const;
    Q_INVOKABLE bool isSystemFlyoutItem(const QString &key) const;
    Q_INVOKABLE void setOverflow(const QString &key, bool enabled);
    void reorder(const QStringList &orderedKeys, bool overflowOnly);
signals:
    void overflowChanged();
    void iconRevisionChanged();
private:
    void refresh();
    void applySavedOrder(QVector<Item> &next) const;
    void saveOrder();
    QVector<Item> items;
    QStringList overflowKeys;
    QStringList trayOrder;
    mutable QReadWriteLock iconImageLock;
    QHash<qulonglong, QImage> iconImages;
    quint64 imageRevision = 0;
    QTimer timer;
};

class TrayFilterModel final : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    TrayFilterModel(TrayModel *source, bool overflow, QObject *parent = nullptr);
    int count() const { return rowCount(); }
    Q_INVOKABLE void move(int from, int to);
signals:
    void countChanged();
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
private:
    void refreshFilter();
    TrayModel *tray = nullptr;
    bool overflowOnly = false;
};
