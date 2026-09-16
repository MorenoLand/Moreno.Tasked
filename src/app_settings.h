#pragma once

#include <QObject>

class TaskedSettings final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool splitMode READ splitMode WRITE setSplitMode NOTIFY splitModeChanged)
    Q_PROPERTY(bool searchEnabled READ searchEnabled WRITE setSearchEnabled NOTIFY searchEnabledChanged)
    Q_PROPERTY(bool clockEnabled READ clockEnabled WRITE setClockEnabled NOTIFY clockEnabledChanged)
    Q_PROPERTY(bool secondsEnabled READ secondsEnabled WRITE setSecondsEnabled NOTIFY secondsEnabledChanged)
    Q_PROPERTY(bool dividersEnabled READ dividersEnabled WRITE setDividersEnabled NOTIFY dividersEnabledChanged)
    Q_PROPERTY(bool taskButtonEnabled READ taskButtonEnabled WRITE setTaskButtonEnabled NOTIFY taskButtonEnabledChanged)
    Q_PROPERTY(bool startButtonEnabled READ startButtonEnabled WRITE setStartButtonEnabled NOTIFY startButtonEnabledChanged)
    Q_PROPERTY(bool labelsEnabled READ labelsEnabled WRITE setLabelsEnabled NOTIFY labelsEnabledChanged)
    Q_PROPERTY(bool trayWrapEnabled READ trayWrapEnabled WRITE setTrayWrapEnabled NOTIFY trayWrapEnabledChanged)
    Q_PROPERTY(bool trayScrollEnabled READ trayScrollEnabled WRITE setTrayScrollEnabled NOTIFY trayScrollEnabledChanged)
    Q_PROPERTY(int theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)
    Q_PROPERTY(int labelSize READ labelSize WRITE setLabelSize NOTIFY labelSizeChanged)
    Q_PROPERTY(int clockSize READ clockSize WRITE setClockSize NOTIFY clockSizeChanged)
    Q_PROPERTY(int iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(int surfaceOpacity READ surfaceOpacity WRITE setSurfaceOpacity NOTIFY surfaceOpacityChanged)
    Q_PROPERTY(int backgroundOpacity READ backgroundOpacity WRITE setBackgroundOpacity NOTIFY backgroundOpacityChanged)
    Q_PROPERTY(int cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY cornerRadiusChanged)
public:
    explicit TaskedSettings(QObject *parent = nullptr);
    bool splitMode() const;
    bool searchEnabled() const;
    bool clockEnabled() const;
    bool secondsEnabled() const;
    bool dividersEnabled() const;
    bool taskButtonEnabled() const;
    bool startButtonEnabled() const;
    bool labelsEnabled() const;
    bool trayWrapEnabled() const;
    bool trayScrollEnabled() const;
    int theme() const;
    QString fontFamily() const;
    int labelSize() const;
    int clockSize() const;
    int iconSize() const;
    int surfaceOpacity() const;
    int backgroundOpacity() const;
    int cornerRadius() const;
    Q_INVOKABLE void setSplitMode(bool enabled);
    Q_INVOKABLE void setSearchEnabled(bool enabled);
    Q_INVOKABLE void setClockEnabled(bool enabled);
    Q_INVOKABLE void setSecondsEnabled(bool enabled);
    Q_INVOKABLE void setDividersEnabled(bool enabled);
    Q_INVOKABLE void setTaskButtonEnabled(bool enabled);
    Q_INVOKABLE void setStartButtonEnabled(bool enabled);
    Q_INVOKABLE void setLabelsEnabled(bool enabled);
    Q_INVOKABLE void setTrayWrapEnabled(bool enabled);
    Q_INVOKABLE void setTrayScrollEnabled(bool enabled);
    Q_INVOKABLE void setTheme(int value);
    Q_INVOKABLE void setFontFamily(const QString &value);
    Q_INVOKABLE void setLabelSize(int value);
    Q_INVOKABLE void setClockSize(int value);
    Q_INVOKABLE void setIconSize(int value);
    Q_INVOKABLE void setSurfaceOpacity(int value);
    Q_INVOKABLE void setBackgroundOpacity(int value);
    Q_INVOKABLE void setCornerRadius(int value);
    Q_INVOKABLE void reset();
signals:
    void splitModeChanged();
    void searchEnabledChanged();
    void clockEnabledChanged();
    void secondsEnabledChanged();
    void dividersEnabledChanged();
    void taskButtonEnabledChanged();
    void startButtonEnabledChanged();
    void labelsEnabledChanged();
    void trayWrapEnabledChanged();
    void trayScrollEnabledChanged();
    void themeChanged();
    void fontFamilyChanged();
    void labelSizeChanged();
    void clockSizeChanged();
    void iconSizeChanged();
    void surfaceOpacityChanged();
    void backgroundOpacityChanged();
    void cornerRadiusChanged();
private:
    bool split = false;
    bool search = false;
    bool clock = true;
    bool seconds = false;
    bool dividers = true;
    bool taskButton = true;
    bool startButton = true;
    bool labels = true;
    bool trayWrap = false;
    bool trayScroll = true;
    int themeValue = 0;
    QString font = "Segoe UI";
    int label = 9;
    int clockFont = 20;
    int icon = 42;
    int opacity = 100;
    int background = 100;
    int radius = 22;
};
