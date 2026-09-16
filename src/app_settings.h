#pragma once

#include <QObject>

class TaskedSettings final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool splitMode READ splitMode WRITE setSplitMode NOTIFY splitModeChanged)
    Q_PROPERTY(bool searchEnabled READ searchEnabled WRITE setSearchEnabled NOTIFY searchEnabledChanged)
public:
    explicit TaskedSettings(QObject *parent = nullptr);
    bool splitMode() const;
    bool searchEnabled() const;
    Q_INVOKABLE void setSplitMode(bool enabled);
    Q_INVOKABLE void setSearchEnabled(bool enabled);
signals:
    void splitModeChanged();
    void searchEnabledChanged();
private:
    bool split = false;
    bool search = false;
};
