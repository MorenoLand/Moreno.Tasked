#include "app_settings.h"

#include <QSettings>

TaskedSettings::TaskedSettings(QObject *parent) : QObject(parent)
{
    const QSettings settings;
    split = settings.value("layout/splitMode", false).toBool();
    spaced = settings.value("layout/spacedMode", true).toBool();
    dockLock = settings.value("layout/dockLocked", true).toBool();
    position = settings.value("layout/dockPosition", 0).toInt();
    search = settings.value("layout/searchEnabled", false).toBool();
    clock = settings.value("layout/clockEnabled", true).toBool();
    clock24 = settings.value("layout/clock24Hour", true).toBool();
    seconds = settings.value("behavior/secondsEnabled", false).toBool();
    dividers = settings.value("layout/dividersEnabled", true).toBool();
    taskButton = settings.value("layout/taskButtonEnabled", true).toBool();
    startButton = settings.value("layout/startButtonEnabled", true).toBool();
    labels = settings.value("layout/labelsEnabled", true).toBool();
    tray = settings.value("layout/trayEnabled", true).toBool();
    trayWrap = settings.value("layout/trayWrapEnabled", true).toBool();
    trayScroll = settings.value("layout/trayScrollEnabled", false).toBool();
    previews = settings.value("behavior/previewsEnabled", true).toBool();
    animations = settings.value("behavior/animationsEnabled", true).toBool();
    themeValue = settings.value("style/theme", 0).toInt();
    font = settings.value("typography/fontFamily", "Segoe UI").toString();
    label = settings.value("typography/labelSize", 9).toInt();
    clockFont = settings.value("typography/clockSize", 20).toInt();
    icon = settings.value("typography/iconSize", 42).toInt();
    trayIcon = settings.value("typography/trayIconSize", 24).toInt();
    trayScaleValue = settings.value("typography/trayScale", 100).toInt();
    opacity = settings.value("style/surfaceOpacity", 100).toInt();
    background = settings.value("style/backgroundOpacity", 100).toInt();
    radius = settings.value("style/cornerRadius", 22).toInt();
    shadow = qBound(0, settings.value("style/shadowStyle", 0).toInt(), 2);
}

bool TaskedSettings::splitMode() const { return split; }
bool TaskedSettings::spacedMode() const { return spaced; }
bool TaskedSettings::dockLocked() const { return dockLock; }
int TaskedSettings::dockPosition() const { return position; }
bool TaskedSettings::searchEnabled() const { return search; }
bool TaskedSettings::clockEnabled() const { return clock; }
bool TaskedSettings::clock24Hour() const { return clock24; }
bool TaskedSettings::secondsEnabled() const { return seconds; }
bool TaskedSettings::dividersEnabled() const { return dividers; }
bool TaskedSettings::taskButtonEnabled() const { return taskButton; }
bool TaskedSettings::startButtonEnabled() const { return startButton; }
bool TaskedSettings::labelsEnabled() const { return labels; }
bool TaskedSettings::trayEnabled() const { return tray; }
bool TaskedSettings::trayWrapEnabled() const { return trayWrap; }
bool TaskedSettings::trayScrollEnabled() const { return trayScroll; }
bool TaskedSettings::previewsEnabled() const { return previews; }
bool TaskedSettings::animationsEnabled() const { return animations; }
int TaskedSettings::theme() const { return themeValue; }
QString TaskedSettings::fontFamily() const { return font; }
int TaskedSettings::labelSize() const { return label; }
int TaskedSettings::clockSize() const { return clockFont; }
int TaskedSettings::iconSize() const { return icon; }
int TaskedSettings::trayIconSize() const { return trayIcon; }
int TaskedSettings::trayScale() const { return trayScaleValue; }
int TaskedSettings::surfaceOpacity() const { return opacity; }
int TaskedSettings::backgroundOpacity() const { return background; }
int TaskedSettings::cornerRadius() const { return radius; }
int TaskedSettings::shadowStyle() const { return shadow; }

void TaskedSettings::setSplitMode(bool enabled)
{
    if (split == enabled) return;
    split = enabled;
    QSettings().setValue("layout/splitMode", split);
    emit splitModeChanged();
}

void TaskedSettings::setSpacedMode(bool enabled) { if (spaced == enabled) return; spaced = enabled; QSettings().setValue("layout/spacedMode", spaced); emit spacedModeChanged(); }
void TaskedSettings::setDockLocked(bool enabled) { if (dockLock == enabled) return; dockLock = enabled; QSettings().setValue("layout/dockLocked", dockLock); emit dockLockedChanged(); }
void TaskedSettings::setDockPosition(int value) { value = qBound(0, value, 3); if (position == value) return; position = value; QSettings().setValue("layout/dockPosition", position); emit dockPositionChanged(); }

void TaskedSettings::setSearchEnabled(bool enabled)
{
    if (search == enabled) return;
    search = enabled;
    QSettings().setValue("layout/searchEnabled", search);
    emit searchEnabledChanged();
}

void TaskedSettings::setClockEnabled(bool enabled) { if (clock == enabled) return; clock = enabled; QSettings().setValue("layout/clockEnabled", clock); emit clockEnabledChanged(); }
void TaskedSettings::setClock24Hour(bool enabled) { if (clock24 == enabled) return; clock24 = enabled; QSettings().setValue("layout/clock24Hour", clock24); emit clock24HourChanged(); }
void TaskedSettings::setSecondsEnabled(bool enabled) { if (seconds == enabled) return; seconds = enabled; QSettings().setValue("behavior/secondsEnabled", seconds); emit secondsEnabledChanged(); }
void TaskedSettings::setDividersEnabled(bool enabled) { if (dividers == enabled) return; dividers = enabled; QSettings().setValue("layout/dividersEnabled", dividers); emit dividersEnabledChanged(); }
void TaskedSettings::setTaskButtonEnabled(bool enabled) { if (taskButton == enabled) return; taskButton = enabled; QSettings().setValue("layout/taskButtonEnabled", taskButton); emit taskButtonEnabledChanged(); }
void TaskedSettings::setStartButtonEnabled(bool enabled) { if (startButton == enabled) return; startButton = enabled; QSettings().setValue("layout/startButtonEnabled", startButton); emit startButtonEnabledChanged(); }
void TaskedSettings::setLabelsEnabled(bool enabled) { if (labels == enabled) return; labels = enabled; QSettings().setValue("layout/labelsEnabled", labels); emit labelsEnabledChanged(); }
void TaskedSettings::setTrayEnabled(bool enabled) { if (tray == enabled) return; tray = enabled; QSettings().setValue("layout/trayEnabled", tray); emit trayEnabledChanged(); }
void TaskedSettings::setTrayWrapEnabled(bool enabled) { if (trayWrap == enabled) return; trayWrap = enabled; QSettings().setValue("layout/trayWrapEnabled", trayWrap); emit trayWrapEnabledChanged(); }
void TaskedSettings::setTrayScrollEnabled(bool enabled) { if (trayScroll == enabled) return; trayScroll = enabled; QSettings().setValue("layout/trayScrollEnabled", trayScroll); emit trayScrollEnabledChanged(); }
void TaskedSettings::setPreviewsEnabled(bool enabled) { if (previews == enabled) return; previews = enabled; QSettings().setValue("behavior/previewsEnabled", previews); emit previewsEnabledChanged(); }
void TaskedSettings::setAnimationsEnabled(bool enabled) { if (animations == enabled) return; animations = enabled; QSettings().setValue("behavior/animationsEnabled", animations); emit animationsEnabledChanged(); }
int TaskedSettings::sectionOffsetX(const QString &section) const { return QSettings().value("layout/sections/" + section + "/x", 0).toInt(); }
int TaskedSettings::sectionOffsetY(const QString &section) const { return QSettings().value("layout/sections/" + section + "/y", 0).toInt(); }
void TaskedSettings::setSectionOffset(const QString &section, int x, int y) { QSettings settings; settings.setValue("layout/sections/" + section + "/x", x); settings.setValue("layout/sections/" + section + "/y", y); }
void TaskedSettings::setTheme(int value) { value = qBound(0, value, 2); if (themeValue == value) return; themeValue = value; QSettings().setValue("style/theme", themeValue); emit themeChanged(); }
void TaskedSettings::setFontFamily(const QString &value) { if (value.isEmpty() || font == value) return; font = value; QSettings().setValue("typography/fontFamily", font); emit fontFamilyChanged(); }
void TaskedSettings::setLabelSize(int value) { value = qBound(8, value, 16); if (label == value) return; label = value; QSettings().setValue("typography/labelSize", label); emit labelSizeChanged(); }
void TaskedSettings::setClockSize(int value) { value = qBound(16, value, 28); if (clockFont == value) return; clockFont = value; QSettings().setValue("typography/clockSize", clockFont); emit clockSizeChanged(); }
void TaskedSettings::setIconSize(int value) { value = qBound(34, value, 50); if (icon == value) return; icon = value; QSettings().setValue("typography/iconSize", icon); emit iconSizeChanged(); }
void TaskedSettings::setTrayIconSize(int value) { value = qBound(16, value, 28); if (trayIcon == value) return; trayIcon = value; QSettings().setValue("typography/trayIconSize", trayIcon); emit trayIconSizeChanged(); }
void TaskedSettings::setTrayScale(int value) { value = qBound(80, value, 130); if (trayScaleValue == value) return; trayScaleValue = value; QSettings().setValue("typography/trayScale", trayScaleValue); emit trayScaleChanged(); }
void TaskedSettings::setSurfaceOpacity(int value) { value = qBound(70, value, 100); if (opacity == value) return; opacity = value; QSettings().setValue("style/surfaceOpacity", opacity); emit surfaceOpacityChanged(); }
void TaskedSettings::setBackgroundOpacity(int value) { value = qBound(0, value, 100); if (background == value) return; background = value; QSettings().setValue("style/backgroundOpacity", background); emit backgroundOpacityChanged(); }
void TaskedSettings::setCornerRadius(int value) { value = qBound(10, value, 32); if (radius == value) return; radius = value; QSettings().setValue("style/cornerRadius", radius); emit cornerRadiusChanged(); }
void TaskedSettings::setShadowStyle(int value) { value = qBound(0, value, 2); if (shadow == value) return; shadow = value; QSettings().setValue("style/shadowStyle", shadow); emit shadowStyleChanged(); }
void TaskedSettings::reset()
{
    setSplitMode(false); setSpacedMode(true); setDockLocked(true); setDockPosition(0); setSectionOffset("left", 0, 0); setSectionOffset("middle", 0, 0); setSectionOffset("tray", 0, 0); setSectionOffset("clock", 0, 0); setSearchEnabled(false); setClockEnabled(true); setClock24Hour(true); setSecondsEnabled(false); setDividersEnabled(true); setTaskButtonEnabled(true); setStartButtonEnabled(true); setLabelsEnabled(true); setTrayEnabled(true); setTrayWrapEnabled(true); setTrayScrollEnabled(false); setPreviewsEnabled(true); setAnimationsEnabled(true); setTheme(0); setFontFamily("Segoe UI"); setLabelSize(9); setClockSize(20); setIconSize(42); setTrayIconSize(24); setTrayScale(100); setSurfaceOpacity(100); setBackgroundOpacity(100); setCornerRadius(22); setShadowStyle(0);
    emit sectionOffsetsChanged();
}
