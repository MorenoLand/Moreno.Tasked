#pragma once

#include <QObject>

class TaskedSettings final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool splitMode READ splitMode WRITE setSplitMode NOTIFY splitModeChanged)
    Q_PROPERTY(bool spacedMode READ spacedMode WRITE setSpacedMode NOTIFY spacedModeChanged)
    Q_PROPERTY(bool dockLocked READ dockLocked WRITE setDockLocked NOTIFY dockLockedChanged)
    Q_PROPERTY(int dockPosition READ dockPosition WRITE setDockPosition NOTIFY dockPositionChanged)
    Q_PROPERTY(bool searchEnabled READ searchEnabled WRITE setSearchEnabled NOTIFY searchEnabledChanged)
    Q_PROPERTY(bool clockEnabled READ clockEnabled WRITE setClockEnabled NOTIFY clockEnabledChanged)
    Q_PROPERTY(bool clock24Hour READ clock24Hour WRITE setClock24Hour NOTIFY clock24HourChanged)
    Q_PROPERTY(bool secondsEnabled READ secondsEnabled WRITE setSecondsEnabled NOTIFY secondsEnabledChanged)
    Q_PROPERTY(bool dividersEnabled READ dividersEnabled WRITE setDividersEnabled NOTIFY dividersEnabledChanged)
    Q_PROPERTY(bool taskButtonEnabled READ taskButtonEnabled WRITE setTaskButtonEnabled NOTIFY taskButtonEnabledChanged)
    Q_PROPERTY(bool startButtonEnabled READ startButtonEnabled WRITE setStartButtonEnabled NOTIFY startButtonEnabledChanged)
    Q_PROPERTY(bool labelsEnabled READ labelsEnabled WRITE setLabelsEnabled NOTIFY labelsEnabledChanged)
    Q_PROPERTY(bool trayEnabled READ trayEnabled WRITE setTrayEnabled NOTIFY trayEnabledChanged)
    Q_PROPERTY(bool trayWrapEnabled READ trayWrapEnabled WRITE setTrayWrapEnabled NOTIFY trayWrapEnabledChanged)
    Q_PROPERTY(bool trayScrollEnabled READ trayScrollEnabled WRITE setTrayScrollEnabled NOTIFY trayScrollEnabledChanged)
    Q_PROPERTY(bool previewsEnabled READ previewsEnabled WRITE setPreviewsEnabled NOTIFY previewsEnabledChanged)
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled WRITE setAnimationsEnabled NOTIFY animationsEnabledChanged)
    Q_PROPERTY(int theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)
    Q_PROPERTY(int labelSize READ labelSize WRITE setLabelSize NOTIFY labelSizeChanged)
    Q_PROPERTY(int clockSize READ clockSize WRITE setClockSize NOTIFY clockSizeChanged)
    Q_PROPERTY(int iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(int trayIconSize READ trayIconSize WRITE setTrayIconSize NOTIFY trayIconSizeChanged)
    Q_PROPERTY(int trayScale READ trayScale WRITE setTrayScale NOTIFY trayScaleChanged)
    Q_PROPERTY(int surfaceOpacity READ surfaceOpacity WRITE setSurfaceOpacity NOTIFY surfaceOpacityChanged)
    Q_PROPERTY(int backgroundOpacity READ backgroundOpacity WRITE setBackgroundOpacity NOTIFY backgroundOpacityChanged)
    Q_PROPERTY(int cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY cornerRadiusChanged)
    Q_PROPERTY(int shadowStyle READ shadowStyle WRITE setShadowStyle NOTIFY shadowStyleChanged)
public:
    explicit TaskedSettings(QObject *parent = nullptr);
    bool splitMode() const;
    bool spacedMode() const;
    bool dockLocked() const;
    int dockPosition() const;
    bool searchEnabled() const;
    bool clockEnabled() const;
    bool clock24Hour() const;
    bool secondsEnabled() const;
    bool dividersEnabled() const;
    bool taskButtonEnabled() const;
    bool startButtonEnabled() const;
    bool labelsEnabled() const;
    bool trayEnabled() const;
    bool trayWrapEnabled() const;
    bool trayScrollEnabled() const;
    bool previewsEnabled() const;
    bool animationsEnabled() const;
    int theme() const;
    QString fontFamily() const;
    int labelSize() const;
    int clockSize() const;
    int iconSize() const;
    int trayIconSize() const;
    int trayScale() const;
    int surfaceOpacity() const;
    int backgroundOpacity() const;
    int cornerRadius() const;
    int shadowStyle() const;
    Q_INVOKABLE void setSplitMode(bool enabled);
    Q_INVOKABLE void setSpacedMode(bool enabled);
    Q_INVOKABLE void setDockLocked(bool enabled);
    Q_INVOKABLE void setDockPosition(int value);
    Q_INVOKABLE void setSearchEnabled(bool enabled);
    Q_INVOKABLE void setClockEnabled(bool enabled);
    Q_INVOKABLE void setClock24Hour(bool enabled);
    Q_INVOKABLE void setSecondsEnabled(bool enabled);
    Q_INVOKABLE void setDividersEnabled(bool enabled);
    Q_INVOKABLE void setTaskButtonEnabled(bool enabled);
    Q_INVOKABLE void setStartButtonEnabled(bool enabled);
    Q_INVOKABLE void setLabelsEnabled(bool enabled);
    Q_INVOKABLE void setTrayEnabled(bool enabled);
    Q_INVOKABLE void setTrayWrapEnabled(bool enabled);
    Q_INVOKABLE void setTrayScrollEnabled(bool enabled);
    Q_INVOKABLE void setPreviewsEnabled(bool enabled);
    Q_INVOKABLE void setAnimationsEnabled(bool enabled);
    Q_INVOKABLE int sectionOffsetX(const QString &section) const;
    Q_INVOKABLE int sectionOffsetY(const QString &section) const;
    Q_INVOKABLE void setSectionOffset(const QString &section, int x, int y);
    Q_INVOKABLE void setTheme(int value);
    Q_INVOKABLE void setFontFamily(const QString &value);
    Q_INVOKABLE void setLabelSize(int value);
    Q_INVOKABLE void setClockSize(int value);
    Q_INVOKABLE void setIconSize(int value);
    Q_INVOKABLE void setTrayIconSize(int value);
    Q_INVOKABLE void setTrayScale(int value);
    Q_INVOKABLE void setSurfaceOpacity(int value);
    Q_INVOKABLE void setBackgroundOpacity(int value);
    Q_INVOKABLE void setCornerRadius(int value);
    Q_INVOKABLE void setShadowStyle(int value);
    Q_INVOKABLE void reset();
signals:
    void splitModeChanged();
    void spacedModeChanged();
    void dockLockedChanged();
    void dockPositionChanged();
    void searchEnabledChanged();
    void clockEnabledChanged();
    void clock24HourChanged();
    void secondsEnabledChanged();
    void dividersEnabledChanged();
    void taskButtonEnabledChanged();
    void startButtonEnabledChanged();
    void labelsEnabledChanged();
    void trayEnabledChanged();
    void trayWrapEnabledChanged();
    void trayScrollEnabledChanged();
    void previewsEnabledChanged();
    void animationsEnabledChanged();
    void themeChanged();
    void fontFamilyChanged();
    void labelSizeChanged();
    void clockSizeChanged();
    void iconSizeChanged();
    void trayIconSizeChanged();
    void trayScaleChanged();
    void surfaceOpacityChanged();
    void backgroundOpacityChanged();
    void cornerRadiusChanged();
    void shadowStyleChanged();
    void sectionOffsetsChanged();
private:
    bool split = false;
    bool spaced = true;
    bool dockLock = true;
    int position = 0;
    bool search = false;
    bool clock = true;
    bool clock24 = true;
    bool seconds = false;
    bool dividers = true;
    bool taskButton = true;
    bool startButton = true;
    bool labels = true;
    bool tray = true;
    bool trayWrap = false;
    bool trayScroll = false;
    bool previews = true;
    bool animations = true;
    int themeValue = 0;
    QString font = "Segoe UI";
    int label = 9;
    int clockFont = 20;
    int icon = 42;
    int trayIcon = 24;
    int trayScaleValue = 100;
    int opacity = 100;
    int background = 100;
    int radius = 22;
    int shadow = 0;
};
