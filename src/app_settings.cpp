#include "app_settings.h"

#include <QSettings>

TaskedSettings::TaskedSettings(QObject *parent) : QObject(parent)
{
    const QSettings settings;
    split = settings.value("style/splitMode", false).toBool();
    search = settings.value("style/searchEnabled", false).toBool();
}

bool TaskedSettings::splitMode() const { return split; }
bool TaskedSettings::searchEnabled() const { return search; }

void TaskedSettings::setSplitMode(bool enabled)
{
    if (split == enabled) return;
    split = enabled;
    QSettings().setValue("style/splitMode", split);
    emit splitModeChanged();
}

void TaskedSettings::setSearchEnabled(bool enabled)
{
    if (search == enabled) return;
    search = enabled;
    QSettings().setValue("style/searchEnabled", search);
    emit searchEnabledChanged();
}
