#include "app_settings.h"

#include <QSettings>

TaskedSettings::TaskedSettings(QObject *parent) : QObject(parent)
{
    const QSettings settings;
    split = settings.value("layout/splitMode", false).toBool();
    search = settings.value("layout/searchEnabled", false).toBool();
    clock = settings.value("layout/clockEnabled", true).toBool();
    seconds = settings.value("behavior/secondsEnabled", false).toBool();
    dividers = settings.value("layout/dividersEnabled", true).toBool();
    taskButton = settings.value("layout/taskButtonEnabled", true).toBool();
    startButton = settings.value("layout/startButtonEnabled", true).toBool();
    labels = settings.value("layout/labelsEnabled", true).toBool();
    trayWrap = settings.value("layout/trayWrapEnabled", true).toBool();
    trayScroll = settings.value("layout/trayScrollEnabled", false).toBool();
    themeValue = settings.value("style/theme", 0).toInt();
    font = settings.value("typography/fontFamily", "Segoe UI").toString();
    label = settings.value("typography/labelSize", 9).toInt();
    clockFont = settings.value("typography/clockSize", 20).toInt();
    icon = settings.value("typography/iconSize", 42).toInt();
    opacity = settings.value("style/surfaceOpacity", 100).toInt();
    background = settings.value("style/backgroundOpacity", 100).toInt();
    radius = settings.value("style/cornerRadius", 22).toInt();
}

bool TaskedSettings::splitMode() const { return split; }
bool TaskedSettings::searchEnabled() const { return search; }
bool TaskedSettings::clockEnabled() const { return clock; }
bool TaskedSettings::secondsEnabled() const { return seconds; }
bool TaskedSettings::dividersEnabled() const { return dividers; }
bool TaskedSettings::taskButtonEnabled() const { return taskButton; }
bool TaskedSettings::startButtonEnabled() const { return startButton; }
bool TaskedSettings::labelsEnabled() const { return labels; }
bool TaskedSettings::trayWrapEnabled() const { return trayWrap; }
bool TaskedSettings::trayScrollEnabled() const { return trayScroll; }
int TaskedSettings::theme() const { return themeValue; }
QString TaskedSettings::fontFamily() const { return font; }
int TaskedSettings::labelSize() const { return label; }
int TaskedSettings::clockSize() const { return clockFont; }
int TaskedSettings::iconSize() const { return icon; }
int TaskedSettings::surfaceOpacity() const { return opacity; }
int TaskedSettings::backgroundOpacity() const { return background; }
int TaskedSettings::cornerRadius() const { return radius; }

void TaskedSettings::setSplitMode(bool enabled)
{
    if (split == enabled) return;
    split = enabled;
    QSettings().setValue("layout/splitMode", split);
    emit splitModeChanged();
}

void TaskedSettings::setSearchEnabled(bool enabled)
{
    if (search == enabled) return;
    search = enabled;
    QSettings().setValue("layout/searchEnabled", search);
    emit searchEnabledChanged();
}

void TaskedSettings::setClockEnabled(bool enabled) { if (clock == enabled) return; clock = enabled; QSettings().setValue("layout/clockEnabled", clock); emit clockEnabledChanged(); }
void TaskedSettings::setSecondsEnabled(bool enabled) { if (seconds == enabled) return; seconds = enabled; QSettings().setValue("behavior/secondsEnabled", seconds); emit secondsEnabledChanged(); }
void TaskedSettings::setDividersEnabled(bool enabled) { if (dividers == enabled) return; dividers = enabled; QSettings().setValue("layout/dividersEnabled", dividers); emit dividersEnabledChanged(); }
void TaskedSettings::setTaskButtonEnabled(bool enabled) { if (taskButton == enabled) return; taskButton = enabled; QSettings().setValue("layout/taskButtonEnabled", taskButton); emit taskButtonEnabledChanged(); }
void TaskedSettings::setStartButtonEnabled(bool enabled) { if (startButton == enabled) return; startButton = enabled; QSettings().setValue("layout/startButtonEnabled", startButton); emit startButtonEnabledChanged(); }
void TaskedSettings::setLabelsEnabled(bool enabled) { if (labels == enabled) return; labels = enabled; QSettings().setValue("layout/labelsEnabled", labels); emit labelsEnabledChanged(); }
void TaskedSettings::setTrayWrapEnabled(bool enabled) { if (trayWrap == enabled) return; trayWrap = enabled; QSettings().setValue("layout/trayWrapEnabled", trayWrap); emit trayWrapEnabledChanged(); }
void TaskedSettings::setTrayScrollEnabled(bool enabled) { if (trayScroll == enabled) return; trayScroll = enabled; QSettings().setValue("layout/trayScrollEnabled", trayScroll); emit trayScrollEnabledChanged(); }
void TaskedSettings::setTheme(int value) { value = qBound(0, value, 2); if (themeValue == value) return; themeValue = value; QSettings().setValue("style/theme", themeValue); emit themeChanged(); }
void TaskedSettings::setFontFamily(const QString &value) { if (value.isEmpty() || font == value) return; font = value; QSettings().setValue("typography/fontFamily", font); emit fontFamilyChanged(); }
void TaskedSettings::setLabelSize(int value) { value = qBound(8, value, 16); if (label == value) return; label = value; QSettings().setValue("typography/labelSize", label); emit labelSizeChanged(); }
void TaskedSettings::setClockSize(int value) { value = qBound(16, value, 28); if (clockFont == value) return; clockFont = value; QSettings().setValue("typography/clockSize", clockFont); emit clockSizeChanged(); }
void TaskedSettings::setIconSize(int value) { value = qBound(34, value, 50); if (icon == value) return; icon = value; QSettings().setValue("typography/iconSize", icon); emit iconSizeChanged(); }
void TaskedSettings::setSurfaceOpacity(int value) { value = qBound(70, value, 100); if (opacity == value) return; opacity = value; QSettings().setValue("style/surfaceOpacity", opacity); emit surfaceOpacityChanged(); }
void TaskedSettings::setBackgroundOpacity(int value) { value = qBound(0, value, 100); if (background == value) return; background = value; QSettings().setValue("style/backgroundOpacity", background); emit backgroundOpacityChanged(); }
void TaskedSettings::setCornerRadius(int value) { value = qBound(10, value, 32); if (radius == value) return; radius = value; QSettings().setValue("style/cornerRadius", radius); emit cornerRadiusChanged(); }
void TaskedSettings::reset()
{
    setSplitMode(false); setSearchEnabled(false); setClockEnabled(true); setSecondsEnabled(false); setDividersEnabled(true); setTaskButtonEnabled(true); setStartButtonEnabled(true); setLabelsEnabled(true); setTrayWrapEnabled(true); setTrayScrollEnabled(false); setTheme(0); setFontFamily("Segoe UI"); setLabelSize(9); setClockSize(20); setIconSize(42); setSurfaceOpacity(100); setBackgroundOpacity(100); setCornerRadius(22);
}
