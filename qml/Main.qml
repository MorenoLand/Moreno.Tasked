import QtQuick
import QtQuick.Window

Window {
    id: root
    visible: false
    width: verticalDock ? 88 : preferredDockWidth
    height: verticalDock ? preferredDockHeight : 88
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    color: "transparent"
    title: "Tasked"
    property bool splitMode: taskedSettings.splitMode
    property bool spacedMode: taskedSettings.spacedMode
    property bool dockLocked: taskedSettings.dockLocked
    property int dockPosition: taskedSettings.dockPosition
    property bool verticalDock: dockPosition > 1
    property bool searchEnabled: taskedSettings.searchEnabled
    property bool clockEnabled: taskedSettings.clockEnabled
    property bool secondsEnabled: taskedSettings.secondsEnabled
    property bool dividersEnabled: taskedSettings.dividersEnabled
    property bool taskButtonEnabled: taskedSettings.taskButtonEnabled
    property bool startButtonEnabled: taskedSettings.startButtonEnabled
    property bool labelsEnabled: taskedSettings.labelsEnabled
    property bool trayEnabled: taskedSettings.trayEnabled
    property bool trayWrapEnabled: taskedSettings.trayWrapEnabled
    property bool trayScrollEnabled: taskedSettings.trayScrollEnabled
    property bool previewsEnabled: taskedSettings.previewsEnabled
    property bool animationsEnabled: taskedSettings.animationsEnabled
    property int animationDuration: animationsEnabled ? 220 : 0
    property int fastAnimationDuration: animationsEnabled ? 130 : 0
    property int fadeAnimationDuration: animationsEnabled ? 180 : 0
    property int shortAnimationDuration: animationsEnabled ? 150 : 0
    property int quickAnimationDuration: animationsEnabled ? 140 : 0
    property int closeAnimationDuration: animationsEnabled ? 120 : 0
    property bool settingsOpen: false
    property int settingsTab: 0
    property int theme: taskedSettings.theme
    property string fontFamily: taskedSettings.fontFamily
    property int labelSize: taskedSettings.labelSize
    property int clockSize: taskedSettings.clockSize
    property int iconSize: taskedSettings.iconSize
    property int trayIconSize: taskedSettings.trayIconSize
    property real trayScale: taskedSettings.trayScale / 100
    property real dockOpacity: taskedSettings.surfaceOpacity / 100
    property real backgroundOpacity: taskedSettings.backgroundOpacity / 100
    property int cornerRadius: taskedSettings.cornerRadius
    property color accentColor: theme === 1 ? "#58D3E8" : theme === 2 ? "#C28CFF" : "#6D8DF4"
    property color surfaceColor: theme === 1 ? "#E8124B5C" : theme === 2 ? "#E82B1D55" : "#E815244E"
    property color sectionColor: theme === 1 ? "#E8124B5C" : theme === 2 ? "#E82B1D55" : "#E815244E"
    property color surfaceBackgroundColor: Qt.rgba(surfaceColor.r, surfaceColor.g, surfaceColor.b, surfaceColor.a * dockOpacity * backgroundOpacity)
    property color sectionBackgroundColor: Qt.rgba(sectionColor.r, sectionColor.g, sectionColor.b, sectionColor.a * dockOpacity * backgroundOpacity)
    property color iconSurfaceColor: theme === 1 ? "#24526A" : theme === 2 ? "#4B3178" : "#31446F"
    property int dockSidePadding: 14
    property int buttonWidth: Math.max(60, iconSize + 18)
    property int visibleButtonCount: (startButtonEnabled ? 1 : 0) + (taskButtonEnabled ? 1 : 0) + (searchEnabled ? 1 : 0)
    property bool dividerOneVisible: dividersEnabled && startButtonEnabled && (taskButtonEnabled || searchEnabled)
    property bool dividerTwoVisible: dividersEnabled && taskButtonEnabled && searchEnabled
    property int leftWidth: verticalDock ? (visibleButtonCount > 0 ? 76 : 0) : (visibleButtonCount > 0 ? buttonWidth * visibleButtonCount + (dividerOneVisible ? 1 : 0) + (dividerTwoVisible ? 1 : 0) + Math.max(0, visibleButtonCount - 1) * 8 + dockSidePadding * 2 : 0)
    property int leftHeight: verticalDock ? (visibleButtonCount > 0 ? Math.max(76, visibleButtonCount * 76 + (dividerOneVisible ? 1 : 0) + (dividerTwoVisible ? 1 : 0) + Math.max(0, visibleButtonCount + (dividerOneVisible ? 1 : 0) + (dividerTwoVisible ? 1 : 0) - 1) * 8 + dockSidePadding * 2) : 0) : 76
    property int clockWidth: clockEnabled ? Math.max(secondsEnabled ? 112 : 88, Math.round(clockSize * (secondsEnabled ? 5.2 : 3.8) + 8)) : 0
    property int trayPixelSize: Math.max(12, Math.round(trayIconSize * trayScale))
    property int trayCellSize: trayPixelSize + 4
    property int overflowButtonWidth: overflowTrayIcons.count > 0 || !dockLocked ? trayCellSize : 0
    property int unwrappedTrayWidth: Math.max(0, dockTrayIcons.count * trayCellSize + Math.max(0, dockTrayIcons.count - 1) * 4)
    property int trayListHeight: Math.max(0, dockTrayIcons.count * trayCellSize + Math.max(0, dockTrayIcons.count - 1) * 4)
    property bool clockSectionVisible: clockEnabled && (splitMode || !trayEnabled)
    property int clockSectionWidth: clockSectionVisible && !verticalDock ? clockWidth : 0
    property int trayControlGaps: (overflowButtonWidth > 0 ? 1 : 0) + (!splitMode && clockEnabled ? 1 : 0)
    property int rightWidth: trayEnabled ? (trayWrapEnabled || trayScrollEnabled ? 480 : Math.max(180, 32 + unwrappedTrayWidth + overflowButtonWidth + (splitMode ? 0 : clockWidth) + trayControlGaps * 12)) : 0
    property int trayListWidth: trayEnabled ? (verticalDock ? 76 : (trayWrapEnabled || trayScrollEnabled ? Math.max(0, rightWidth - 32 - overflowButtonWidth - (splitMode ? 0 : clockWidth) - trayControlGaps * 12) : unwrappedTrayWidth)) : 0
    property int trayHeight: verticalDock && trayEnabled ? Math.max(76, trayListHeight + dockSidePadding * 2 + (overflowButtonWidth > 0 ? trayCellSize + 12 : 0) + (!splitMode && clockEnabled ? 76 + 12 : 0)) : 76
    property int middleWidth: Math.max(320, runningList.count * (buttonWidth + 8) - 8 + 28)
    property int middleHeight: Math.max(320, runningList.count * 76 + 28)
    property int sectionGap: splitMode && spacedMode ? 10 : 0
    property int horizontalSectionCount: (visibleButtonCount > 0 ? 1 : 0) + 1 + (trayEnabled ? 1 : 0) + (clockSectionVisible ? 1 : 0)
    property int verticalSectionCount: (visibleButtonCount > 0 ? 1 : 0) + 1 + (trayEnabled ? 1 : 0) + (clockSectionVisible ? 1 : 0)
    property int iconTopMargin: labelsEnabled ? 11 : 18
    property int preferredDockWidth: Math.max(1080, leftWidth + middleWidth + rightWidth + clockSectionWidth + sectionGap * Math.max(0, horizontalSectionCount - 1))
    property int preferredDockHeight: Math.max(320, leftHeight + middleHeight + (trayEnabled ? trayHeight : 0) + (clockSectionVisible ? 76 : 0) + sectionGap * Math.max(0, verticalSectionCount - 1))
    property string clock: Qt.formatTime(new Date(), secondsEnabled ? "HH:mm:ss" : "HH:mm")
    property string date: Qt.formatDate(new Date(), "MMM d")
    property string previewHandle: ""
    property string previewTitle: ""
    property string trayContextKey: ""
    function openContextMenu() { trayContextKey = ""; contextMenu.openMenu() }
    function openContextMenuAt(item, x, y) { var point = item.mapToGlobal(x, y); trayContextKey = ""; contextMenu.openMenu(Math.round(point.x), Math.round(point.y)) }
    function openTrayContextMenu(key) { trayContextKey = key; contextMenu.openMenu() }
    function showSystemTrayFlyout() { var point = trayPanel.mapToGlobal(0, 0); launcher.showSystemTrayFlyout(Math.round(point.x), Math.round(point.y), Math.round(trayPanel.width), Math.round(trayPanel.height)) }
    function showStartMenu() { var point = startButton.mapToGlobal(0, 0); launcher.showStartMenu(Math.round(point.x), Math.round(point.y), Math.round(startButton.width), Math.round(startButton.height)) }
    function schedulePreview(handle, item, title) { if (!previewsEnabled) return; previewHandle = handle; previewTitle = title; previewItem = item; previewCloseTimer.stop(); previewOpenTimer.restart() }
    function openPreview() { if (!previewItem) return; var point = previewItem.mapToGlobal(0, 0); var x = point.x + (previewItem.width - previewWindow.width) / 2; var y = point.y - previewWindow.height - 12; if (y < 8) y = point.y + previewItem.height + 12; previewWindow.x = Math.round(x); previewWindow.y = Math.round(y); previewWindow.show(); previewWindow.raise(); previewController.show(previewHandle, previewWindow, previewWindow.width, previewWindow.height) }
    function closePreview() { previewOpenTimer.stop(); previewCloseTimer.restart() }
    property var previewItem: null
    Timer { id: previewOpenTimer; interval: 280; onTriggered: root.openPreview() }
    Timer { id: previewCloseTimer; interval: 140; onTriggered: { previewController.hide(); previewWindow.hide() } }
    Timer { interval: 1000; running: true; repeat: true; onTriggered: { root.clock = Qt.formatTime(new Date(), root.secondsEnabled ? "HH:mm:ss" : "HH:mm"); root.date = Qt.formatDate(new Date(), "MMM d") } }
    onPreviewsEnabledChanged: if (!previewsEnabled) root.closePreview()
    onPreferredDockWidthChanged: if (!verticalDock && width !== preferredDockWidth) width = preferredDockWidth
    onPreferredDockHeightChanged: if (verticalDock && height !== preferredDockHeight) height = preferredDockHeight

    Rectangle {
        id: solidSurface
        width: dock.width
        height: dock.height
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        radius: root.cornerRadius
        color: root.surfaceBackgroundColor
        opacity: root.splitMode ? 0 : 1
        scale: root.splitMode ? 0.98 : 1
        Behavior on opacity { NumberAnimation { duration: root.animationDuration; easing.type: Easing.InOutCubic } }
        Behavior on scale { NumberAnimation { duration: root.animationDuration; easing.type: Easing.InOutCubic } }
    }

    Grid {
        id: dock
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        columns: root.verticalDock ? 1 : 4
        columnSpacing: root.verticalDock ? 0 : root.sectionGap
        rowSpacing: root.verticalDock ? root.sectionGap : 0

        Rectangle {
            id: leftPanel
            width: root.verticalDock ? 76 : root.leftWidth
            height: root.verticalDock ? root.leftHeight : 76
            radius: root.splitMode ? root.cornerRadius : 0
            color: root.splitMode ? root.sectionBackgroundColor : "transparent"
            opacity: 1
            visible: root.visibleButtonCount > 0
            property real freeX: taskedSettings.sectionOffsetX("left")
            property real freeY: taskedSettings.sectionOffsetY("left")
            transform: Translate { x: root.dockLocked ? 0 : leftPanel.freeX; y: root.dockLocked ? 0 : leftPanel.freeY }
            Behavior on radius { NumberAnimation { duration: root.animationDuration; easing.type: Easing.InOutCubic } }
            Behavior on color { ColorAnimation { duration: root.animationDuration } }
            Behavior on opacity { NumberAnimation { duration: root.fadeAnimationDuration } }

            Grid {
                anchors.fill: parent
                anchors.leftMargin: root.verticalDock ? 0 : root.dockSidePadding
                anchors.rightMargin: root.verticalDock ? 0 : root.dockSidePadding
                anchors.topMargin: root.verticalDock ? root.dockSidePadding : 0
                anchors.bottomMargin: root.verticalDock ? root.dockSidePadding : 0
                columns: root.verticalDock ? 1 : Math.max(1, root.visibleButtonCount + (root.dividerOneVisible ? 1 : 0) + (root.dividerTwoVisible ? 1 : 0))
                columnSpacing: root.verticalDock ? 0 : 8
                rowSpacing: root.verticalDock ? 8 : 0
                spacing: 8

                Item {
                    width: root.verticalDock ? 76 : (root.startButtonEnabled ? root.buttonWidth : 0)
                    height: 76
                    visible: root.startButtonEnabled
                    Rectangle {
                        id: startButton
                        width: root.iconSize
                        height: root.iconSize
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: root.iconTopMargin
                        radius: Math.min(16, Math.max(10, Math.round(root.iconSize * 0.28)))
                        color: root.accentColor
                        scale: startMouse.containsMouse ? 1.1 : 1
                        Behavior on scale { NumberAnimation { duration: root.fastAnimationDuration; easing.type: Easing.OutCubic } }
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            gradient: Gradient {
                                GradientStop { position: 0; color: Qt.lighter(root.accentColor, 1.3) }
                                GradientStop { position: 1; color: Qt.darker(root.accentColor, 1.25) }
                            }
                            opacity: 0.72
                        }
                        Grid {
                            anchors.centerIn: parent
                            columns: 2
                            spacing: 3
                            Repeater { model: 4; delegate: Rectangle { width: 9; height: 9; radius: 2; color: "#F3F6FF" } }
                        }
                    }
                    Text { visible: root.labelsEnabled; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 6; text: "Start"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: root.labelSize }
                    MouseArea { id: startMouse; anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton; hoverEnabled: true; onPressed: if (mouse.button === Qt.RightButton) root.openContextMenuAt(startMouse, mouse.x, mouse.y); onClicked: if (mouse.button === Qt.LeftButton) root.showStartMenu() }
                }

                Rectangle { width: root.dividerOneVisible ? (root.verticalDock ? 40 : 1) : 0; height: root.verticalDock ? 1 : 40; visible: root.dividerOneVisible; color: "#40FFFFFF" }

                Item {
                    width: root.verticalDock ? 76 : (root.taskButtonEnabled ? root.buttonWidth : 0)
                    height: 76
                    visible: root.taskButtonEnabled
                    Rectangle {
                        id: taskButton
                        width: root.iconSize
                        height: root.iconSize
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: root.iconTopMargin
                        radius: Math.min(16, Math.max(10, Math.round(root.iconSize * 0.28)))
                        color: root.accentColor
                        scale: taskMouse.containsMouse ? 1.1 : 1
                        Behavior on scale { NumberAnimation { duration: root.fastAnimationDuration; easing.type: Easing.OutCubic } }
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            gradient: Gradient {
                                GradientStop { position: 0; color: Qt.lighter(root.accentColor, 1.3) }
                                GradientStop { position: 1; color: Qt.darker(root.accentColor, 1.25) }
                            }
                            opacity: 0.72
                        }
                        Image { id: taskedImage; anchors.fill: parent; anchors.margins: 5; source: "qrc:/Tasked-icon.png"; fillMode: Image.PreserveAspectFit; smooth: true }
                        Text { anchors.centerIn: parent; text: "T"; color: "#FFFFFF"; font.family: root.fontFamily; font.pixelSize: Math.max(18, Math.round(root.iconSize * 0.5)); font.bold: true; visible: taskedImage.status !== Image.Ready }
                    }
                    Text { visible: root.labelsEnabled; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 6; text: "Tasked"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: root.labelSize }
                    MouseArea { id: taskMouse; anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton; hoverEnabled: true; onPressed: if (mouse.button === Qt.RightButton) root.openContextMenuAt(taskMouse, mouse.x, mouse.y); onClicked: if (mouse.button === Qt.LeftButton) root.settingsOpen = !root.settingsOpen }
                }

                Rectangle { width: root.dividerTwoVisible ? (root.verticalDock ? 40 : 1) : 0; height: root.verticalDock ? 1 : 40; visible: root.dividerTwoVisible; color: "#40FFFFFF" }

                Item {
                    width: root.verticalDock ? 76 : (root.searchEnabled ? root.buttonWidth : 0)
                    height: 76
                    visible: root.searchEnabled
                    Rectangle {
                        id: searchButton
                        width: root.iconSize
                        height: root.iconSize
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: root.iconTopMargin
                        radius: Math.min(16, Math.max(10, Math.round(root.iconSize * 0.28)))
                        color: root.iconSurfaceColor
                        scale: searchMouse.containsMouse ? 1.1 : 1
                        Behavior on scale { NumberAnimation { duration: root.fastAnimationDuration; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: "⌕"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: Math.max(20, Math.round(root.iconSize * 0.55)) }
                    }
                    Text { visible: root.labelsEnabled; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 6; text: "Search"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: root.labelSize }
                    MouseArea { id: searchMouse; anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton; hoverEnabled: true; onPressed: if (mouse.button === Qt.RightButton) root.openContextMenuAt(searchMouse, mouse.x, mouse.y); onClicked: if (mouse.button === Qt.LeftButton) launcher.launch("search-ms:") }
                }
            }
            MouseArea { anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; width: Math.min(parent.width, 120); height: 10; visible: !root.dockLocked; z: 10; property real lastX: 0; property real lastY: 0; onPressed: { lastX = mouse.x; lastY = mouse.y } onPositionChanged: { var dx = mouse.x - lastX; var dy = mouse.y - lastY; leftPanel.freeX += dx; leftPanel.freeY += dy; lastX = mouse.x; lastY = mouse.y; taskedSettings.setSectionOffset("left", Math.round(leftPanel.freeX), Math.round(leftPanel.freeY)) } }
        }

        Rectangle {
            id: middlePanel
            width: root.verticalDock ? 76 : root.middleWidth
            height: root.verticalDock ? root.middleHeight : 76
            radius: root.splitMode ? root.cornerRadius : 0
            color: root.splitMode ? root.sectionBackgroundColor : "transparent"
            opacity: 1
            property real freeX: taskedSettings.sectionOffsetX("middle")
            property real freeY: taskedSettings.sectionOffsetY("middle")
            transform: Translate { x: root.dockLocked ? 0 : middlePanel.freeX; y: root.dockLocked ? 0 : middlePanel.freeY }
            Behavior on radius { NumberAnimation { duration: root.animationDuration; easing.type: Easing.InOutCubic } }
            Behavior on color { ColorAnimation { duration: root.animationDuration } }
            Behavior on opacity { NumberAnimation { duration: root.fadeAnimationDuration } }

            Row {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                ListView {
                    id: runningList
                    width: parent.width
                    height: parent.height
                    clip: true
                    orientation: root.verticalDock ? ListView.Vertical : ListView.Horizontal
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    model: runningApps
                    add: Transition { NumberAnimation { properties: "x,opacity"; from: 16; to: 0; duration: root.animationDuration; easing.type: Easing.OutCubic } }
                    displaced: Transition { NumberAnimation { properties: "x"; duration: root.fadeAnimationDuration; easing.type: Easing.InOutCubic } }
                    delegate: Item {
                        id: runningDelegate
                        width: root.verticalDock ? 76 : root.buttonWidth
                        height: 76
                        property bool dragged: false
                        Rectangle {
                            id: runningIcon
                            width: root.iconSize
                            height: root.iconSize
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: root.iconTopMargin
                            radius: Math.min(16, Math.max(10, Math.round(root.iconSize * 0.28)))
                            color: root.iconSurfaceColor
                            opacity: runningMouse.containsMouse ? 1 : 0.94
                            scale: runningMouse.containsMouse ? 1.1 : 1
                            Behavior on scale { NumberAnimation { duration: root.fastAnimationDuration; easing.type: Easing.OutCubic } }
                            Behavior on opacity { NumberAnimation { duration: root.fastAnimationDuration } }
                            Image { id: runningImage; anchors.fill: parent; anchors.margins: 6; source: model.iconSource; fillMode: Image.PreserveAspectFit; visible: status === Image.Ready }
                            Text { anchors.centerIn: parent; text: model.title.charAt(0); color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: Math.max(17, Math.round(root.iconSize * 0.4)); font.bold: true; visible: runningImage.status !== Image.Ready }
                            Rectangle { width: Math.max(18, Math.round(root.iconSize * 0.48)); height: 3; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; radius: 2; color: model.active ? root.accentColor : "transparent" }
                        }
                        Text { visible: root.labelsEnabled; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 6; width: runningDelegate.width; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter; text: model.title; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: root.labelSize }
                        MouseArea {
                            id: runningMouse
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            hoverEnabled: true
                            drag.target: runningDelegate
                            drag.axis: Drag.XAxis
                            property real pressX: 0
                            onEntered: root.schedulePreview(model.windowHandle, runningDelegate, model.title)
                            onExited: root.closePreview()
                            onPressed: { if (mouse.button === Qt.RightButton) { var point = runningMouse.mapToGlobal(mouse.x, mouse.y); runningApps.showTaskMenu(model.windowHandle, Math.round(point.x), Math.round(point.y)); return } pressX = mouse.x; runningDelegate.dragged = false }
                            onPositionChanged: if (Math.abs(mouse.x - pressX) > 6) runningDelegate.dragged = true
                            onReleased: if (mouse.button === Qt.LeftButton && runningDelegate.dragged) { var target = Math.round((runningDelegate.x + runningDelegate.width / 2) / (runningDelegate.width + runningList.spacing)); target = Math.max(0, Math.min(runningList.count - 1, target)); runningApps.move(index, target) }
                            onClicked: { root.closePreview(); if (mouse.button === Qt.LeftButton && !runningDelegate.dragged) runningApps.activate(model.windowHandle); runningDelegate.dragged = false }
                        }
                    }
                }
            }
            MouseArea { anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; width: Math.min(parent.width, 120); height: 10; visible: !root.dockLocked; z: 10; property real lastX: 0; property real lastY: 0; onPressed: { lastX = mouse.x; lastY = mouse.y } onPositionChanged: { var dx = mouse.x - lastX; var dy = mouse.y - lastY; middlePanel.freeX += dx; middlePanel.freeY += dy; lastX = mouse.x; lastY = mouse.y; taskedSettings.setSectionOffset("middle", Math.round(middlePanel.freeX), Math.round(middlePanel.freeY)) } }
        }

        Rectangle {
            id: trayPanel
            width: root.verticalDock ? 76 : root.rightWidth
            height: root.verticalDock ? root.trayHeight : 76
            radius: root.splitMode ? root.cornerRadius : 0
            color: root.splitMode ? root.sectionBackgroundColor : "transparent"
            opacity: 1
            clip: true
            visible: root.trayEnabled
            property real freeX: taskedSettings.sectionOffsetX("tray")
            property real freeY: taskedSettings.sectionOffsetY("tray")
            transform: Translate { x: root.dockLocked ? 0 : trayPanel.freeX; y: root.dockLocked ? 0 : trayPanel.freeY }
            Behavior on radius { NumberAnimation { duration: root.animationDuration; easing.type: Easing.InOutCubic } }
            Behavior on color { ColorAnimation { duration: root.animationDuration } }
            Behavior on opacity { NumberAnimation { duration: root.fadeAnimationDuration } }

            Component {
                id: trayDelegate
                Item {
                    id: trayItem
                    width: root.verticalDock ? root.trayListWidth : root.trayCellSize
                    height: root.trayCellSize
                    property bool dragged: false
                    property real pressX: 0
                    property real pressY: 0
                    scale: trayMouse.containsMouse ? 1.12 : 1
                    Behavior on scale { NumberAnimation { duration: root.fastAnimationDuration; easing.type: Easing.OutCubic } }
                    Rectangle { anchors.centerIn: parent; width: root.trayCellSize; height: root.trayCellSize; radius: 7; color: root.iconSurfaceColor }
                    Image { id: trayImage; anchors.centerIn: parent; width: root.trayPixelSize; height: root.trayPixelSize; source: "image://tray/" + model.key; fillMode: Image.PreserveAspectFit; smooth: true; opacity: 1 }
                    Text { anchors.centerIn: parent; text: model.tooltip.charAt(0); color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: root.labelSize + 2; font.bold: true; visible: trayImage.status !== Image.Ready }
                    MouseArea {
                        id: trayMouse
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        hoverEnabled: true
                        preventStealing: true
                        drag.target: trayItem
                        drag.axis: root.verticalDock ? Drag.YAxis : (root.trayWrapEnabled ? Drag.XAndYAxis : Drag.XAxis)
                        onEntered: trayIcons.activate(model.key, 3)
                        onPressed: { if (mouse.button === Qt.RightButton) { var point = trayMouse.mapToGlobal(mouse.x, mouse.y); trayIcons.showContextMenu(model.key, Math.round(point.x), Math.round(point.y)); return } trayItem.pressX = mouse.x; trayItem.pressY = mouse.y; trayItem.dragged = false }
                        onPositionChanged: if (Math.abs(mouse.x - trayItem.pressX) > 5 || Math.abs(mouse.y - trayItem.pressY) > 5) trayItem.dragged = true
                        onReleased: if (mouse.button === Qt.LeftButton && trayItem.dragged) { var overflowPoint = trayItem.mapToItem(trayOverflowButton, trayItem.width / 2, trayItem.height / 2); if (trayOverflowButton.visible && overflowPoint.x >= 0 && overflowPoint.x <= trayOverflowButton.width && overflowPoint.y >= 0 && overflowPoint.y <= trayOverflowButton.height) trayIcons.setOverflow(model.key, true); else { var position = root.verticalDock ? trayItem.y + trayList.contentY : (root.trayWrapEnabled ? trayItem.x : trayItem.x + trayList.contentX); var columns = Math.max(1, Math.floor((root.trayListWidth + 3) / (trayItem.width + (root.trayWrapEnabled ? 3 : 4)))); var target = root.verticalDock ? Math.round((position + trayItem.height / 2) / (trayItem.height + 4)) : (root.trayWrapEnabled ? Math.round(trayItem.y / (trayItem.height + 3)) * columns + Math.round(position / (trayItem.width + 3)) : Math.round((position + trayItem.width / 2) / (trayItem.width + 4))); target = Math.max(0, Math.min(dockTrayIcons.count - 1, target)); dockTrayIcons.move(index, target) } }
                        onClicked: { if (mouse.button === Qt.LeftButton && !trayItem.dragged) { if (trayIcons.isSystemFlyoutItem(model.key)) root.showSystemTrayFlyout(); else trayIcons.activate(model.key, 0) } trayItem.dragged = false }
                    }
                }
            }

            Grid {
                anchors.fill: parent
                anchors.leftMargin: root.verticalDock ? 0 : 16
                anchors.rightMargin: root.verticalDock ? 0 : 16
                anchors.topMargin: root.verticalDock ? root.dockSidePadding : 0
                anchors.bottomMargin: root.verticalDock ? root.dockSidePadding : 0
                columns: root.verticalDock ? 1 : 3
                columnSpacing: root.verticalDock ? 0 : 12
                rowSpacing: root.verticalDock ? 12 : 0

                ListView {
                    width: root.verticalDock ? 76 : root.trayListWidth
                    height: root.verticalDock ? Math.max(1, root.trayListHeight) : 42
                    visible: !root.trayWrapEnabled
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: root.labelsEnabled ? 0 : 6
                    anchors.horizontalCenter: root.verticalDock ? parent.horizontalCenter : undefined
                    clip: true
                    interactive: root.trayScrollEnabled
                    boundsBehavior: Flickable.StopAtBounds
                    orientation: root.verticalDock ? ListView.Vertical : ListView.Horizontal
                    spacing: 4
                    id: trayList
                    model: dockTrayIcons
                    delegate: trayDelegate
                }

                Flow {
                    width: root.verticalDock ? 76 : root.trayListWidth
                    height: root.verticalDock ? Math.max(52, root.trayListHeight) : 52
                    visible: root.trayWrapEnabled
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: root.labelsEnabled ? 0 : 6
                    flow: root.verticalDock ? Flow.TopToBottom : Flow.LeftToRight
                    clip: true
                    spacing: 3
                    Repeater { model: dockTrayIcons; delegate: trayDelegate }
                }

                Item {
                    id: trayOverflowButton
                    width: root.verticalDock ? 76 : root.overflowButtonWidth
                    height: root.trayCellSize
                    visible: overflowTrayIcons.count > 0 || !root.dockLocked
                    anchors.verticalCenterOffset: root.labelsEnabled ? 0 : 6
                    Rectangle { anchors.fill: parent; radius: 7; color: root.iconSurfaceColor }
                    Text { anchors.centerIn: parent; text: "⌃"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 15; font.bold: true }
                    MouseArea { anchors.fill: parent; onClicked: trayOverflowWindow.openPanel() }
                }

                Item {
                    height: 76
                    width: root.verticalDock ? 76 : root.clockWidth
                    visible: root.trayEnabled && !root.splitMode && root.clockEnabled
                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        height: childrenRect.height
                        spacing: 1
                        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: root.clock; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: root.clockSize; font.bold: true }
                        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: root.date; color: "#BFCBEE"; font.family: root.fontFamily; font.pixelSize: root.labelSize }
                    }
                    MouseArea { anchors.fill: parent; acceptedButtons: Qt.RightButton; onClicked: root.openContextMenuAt(parent, mouse.x, mouse.y) }
                }

            }
            MouseArea { anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; width: Math.min(parent.width, 120); height: 10; visible: !root.dockLocked; z: 10; property real lastX: 0; property real lastY: 0; onPressed: { lastX = mouse.x; lastY = mouse.y } onPositionChanged: { var dx = mouse.x - lastX; var dy = mouse.y - lastY; trayPanel.freeX += dx; trayPanel.freeY += dy; lastX = mouse.x; lastY = mouse.y; taskedSettings.setSectionOffset("tray", Math.round(trayPanel.freeX), Math.round(trayPanel.freeY)) } }
        }

        Rectangle {
            id: clockPanel
            width: root.verticalDock ? 76 : root.clockSectionWidth
            height: 76
            visible: root.clockSectionVisible
            radius: root.splitMode ? root.cornerRadius : 0
            color: root.sectionBackgroundColor
            opacity: 1
            property real freeX: taskedSettings.sectionOffsetX("clock")
            property real freeY: taskedSettings.sectionOffsetY("clock")
            transform: Translate { x: root.dockLocked ? 0 : clockPanel.freeX; y: root.dockLocked ? 0 : clockPanel.freeY }
            Behavior on radius { NumberAnimation { duration: root.animationDuration; easing.type: Easing.InOutCubic } }
            Behavior on color { ColorAnimation { duration: root.animationDuration } }
            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                height: childrenRect.height
                spacing: 1
                Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: root.clock; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: root.clockSize; font.bold: true }
                Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: root.date; color: "#BFCBEE"; font.family: root.fontFamily; font.pixelSize: root.labelSize }
            }
            MouseArea { anchors.fill: parent; z: -1; acceptedButtons: Qt.RightButton; onClicked: root.openContextMenuAt(parent, mouse.x, mouse.y) }
            MouseArea { anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; width: Math.min(parent.width, 120); height: 10; visible: !root.dockLocked; z: 10; property real lastX: 0; property real lastY: 0; onPressed: { lastX = mouse.x; lastY = mouse.y } onPositionChanged: { var dx = mouse.x - lastX; var dy = mouse.y - lastY; clockPanel.freeX += dx; clockPanel.freeY += dy; lastX = mouse.x; lastY = mouse.y; taskedSettings.setSectionOffset("clock", Math.round(clockPanel.freeX), Math.round(clockPanel.freeY)) } }
        }
    }

    MouseArea { id: dockContextMouse; anchors.fill: dock; z: -1; acceptedButtons: Qt.RightButton; onClicked: root.openContextMenuAt(dockContextMouse, mouse.x, mouse.y) }

    onSettingsOpenChanged: if (settingsOpen) settingsWindow.openPanel(); else settingsWindow.closePanel()

    Window {
        id: settingsWindow
        visible: false
        width: 560
        height: 500
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
        color: "transparent"
        transientParent: root

        function positionPanel() { if (root.dockPosition === 1) { x = root.x + (root.width - width) / 2; y = root.y + root.height + 12 } else if (root.dockPosition === 2) { x = root.x + root.width + 12; y = root.y + (root.height - height) / 2 } else if (root.dockPosition === 3) { x = root.x - width - 12; y = root.y + (root.height - height) / 2 } else { x = root.x + (root.width - width) / 2; y = root.y - height - 12 } }
        function openPanel() { positionPanel(); show(); raise(); requestActivate(); card.opacity = 0; card.scale = 0.96; openAnimation.restart() }
        function closePanel() { if (visible) closeAnimation.restart() }
        onClosing: root.settingsOpen = false

        Connections {
            target: root
            function onXChanged() { if (settingsWindow.visible) settingsWindow.positionPanel() }
            function onYChanged() { if (settingsWindow.visible) settingsWindow.positionPanel() }
            function onWidthChanged() { if (settingsWindow.visible) settingsWindow.positionPanel() }
        }

        ParallelAnimation {
            id: openAnimation
            NumberAnimation { target: card; property: "opacity"; to: 1; duration: root.fadeAnimationDuration; easing.type: Easing.OutCubic }
            NumberAnimation { target: card; property: "scale"; to: 1; duration: root.animationDuration; easing.type: Easing.OutBack }
        }

        SequentialAnimation {
            id: closeAnimation
            ParallelAnimation {
                NumberAnimation { target: card; property: "opacity"; to: 0; duration: root.closeAnimationDuration; easing.type: Easing.InCubic }
                NumberAnimation { target: card; property: "scale"; to: 0.96; duration: root.closeAnimationDuration; easing.type: Easing.InCubic }
            }
            ScriptAction { script: settingsWindow.visible = false }
        }

        Rectangle {
            id: card
            anchors.fill: parent
            anchors.margins: 5
            radius: 24
            color: "#F018274D"
            opacity: 0
            scale: 0.96
            gradient: Gradient {
                GradientStop { position: 0; color: "#F021355F" }
                GradientStop { position: 1; color: "#F0121E3D" }
            }
            Behavior on scale { NumberAnimation { duration: root.animationDuration; easing.type: Easing.OutBack } }

            Rectangle { anchors.fill: parent; anchors.margins: -7; radius: 31; color: "#50050B19"; z: -1 }

            Column {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                anchors.topMargin: 20
                anchors.bottomMargin: 20
                spacing: 8

                Item {
                    width: parent.width
                    height: 32
                    Text { text: "Settings"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 22; font.bold: true }
                }

                Row {
                    width: parent.width
                    height: 32
                    spacing: 6
                    Repeater {
                        model: ["Styling", "Layout", "Behavior", "Typography", "Extensions"]
                        delegate: Rectangle {
                            width: (parent.width - 24) / 5
                            height: 32
                            radius: 10
                            color: root.settingsTab === index ? root.accentColor : "#182947"
                            Behavior on color { ColorAnimation { duration: root.shortAnimationDuration } }
                            Text { anchors.centerIn: parent; text: modelData; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11; font.bold: true }
                            MouseArea { anchors.fill: parent; onClicked: root.settingsTab = index }
                        }
                    }
                }

                Rectangle { width: parent.width; height: 1; color: "#20FFFFFF" }

                Item {
                    width: parent.width
                    height: 315
                    clip: true

                    Item {
                        anchors.fill: parent
                        visible: root.settingsTab === 0
                        Column {
                            anchors.fill: parent
                            spacing: 10
                            Text { text: "Theme"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 15; font.bold: true }
                            Text { text: "Change the dock palette without changing its behavior."; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 11 }
                            Row {
                                width: parent.width
                                height: 78
                                spacing: 10
                                Repeater {
                                    model: ["Midnight", "Ocean", "Violet"]
                                    delegate: Rectangle {
                                        width: 158
                                        height: 78
                                        radius: 16
                                        color: root.theme === index ? root.accentColor : "#182947"
                                        border.width: root.theme === index ? 2 : 0
                                        border.color: "#F3F6FF"
                                        Behavior on color { ColorAnimation { duration: root.shortAnimationDuration } }
                                        Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.top; anchors.topMargin: 14; text: modelData; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 12; font.bold: true }
                                        Rectangle { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 15; width: 96; height: 9; radius: 5; color: root.theme === index ? "#F3F6FF" : index === 0 ? "#6D8DF4" : index === 1 ? "#58D3E8" : "#C28CFF" }
                                        MouseArea { anchors.fill: parent; onClicked: taskedSettings.setTheme(index) }
                                    }
                                }
                            }
                            Row {
                                width: parent.width
                                height: 30
                                Text { width: 110; text: "Surface opacity"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                                Item { width: parent.width - 246; height: 1 }
                                Text { width: 48; text: taskedSettings.surfaceOpacity + "%"; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 11; horizontalAlignment: Text.AlignRight; anchors.verticalCenter: parent.verticalCenter }
                                Rectangle { width: 28; height: 28; radius: 8; color: "#182947"; Text { anchors.centerIn: parent; text: "−"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setSurfaceOpacity(taskedSettings.surfaceOpacity - 5) } }
                                Rectangle { width: 28; height: 28; radius: 8; color: "#182947"; Text { anchors.centerIn: parent; text: "+"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setSurfaceOpacity(taskedSettings.surfaceOpacity + 5) } }
                            }
                            Row {
                                width: parent.width
                                height: 30
                                Text { width: 110; text: "Background opacity"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                                Item { width: parent.width - 246; height: 1 }
                                Text { width: 48; text: taskedSettings.backgroundOpacity + "%"; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 11; horizontalAlignment: Text.AlignRight; anchors.verticalCenter: parent.verticalCenter }
                                Rectangle { width: 28; height: 28; radius: 8; color: "#182947"; Text { anchors.centerIn: parent; text: "−"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setBackgroundOpacity(taskedSettings.backgroundOpacity - 5) } }
                                Rectangle { width: 28; height: 28; radius: 8; color: "#182947"; Text { anchors.centerIn: parent; text: "+"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setBackgroundOpacity(taskedSettings.backgroundOpacity + 5) } }
                            }
                            Row {
                                width: parent.width
                                height: 30
                                Text { width: 110; text: "Corner radius"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                                Item { width: parent.width - 246; height: 1 }
                                Text { width: 48; text: taskedSettings.cornerRadius + " px"; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 11; horizontalAlignment: Text.AlignRight; anchors.verticalCenter: parent.verticalCenter }
                                Rectangle { width: 28; height: 28; radius: 8; color: "#182947"; Text { anchors.centerIn: parent; text: "−"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setCornerRadius(taskedSettings.cornerRadius - 2) } }
                                Rectangle { width: 28; height: 28; radius: 8; color: "#182947"; Text { anchors.centerIn: parent; text: "+"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setCornerRadius(taskedSettings.cornerRadius + 2) } }
                            }
                        }
                    }

                    Item {
                        anchors.fill: parent
                        visible: root.settingsTab === 1
                        Column {
                            anchors.fill: parent
                            spacing: 4
                            Text { text: "Dock layout"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 15; font.bold: true }
                            Text { text: "Choose the surface and which controls occupy it."; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 11 }
                            Row {
                                width: parent.width
                                height: 44
                                spacing: 10
                                Rectangle { width: 242; height: 44; radius: 15; color: root.splitMode ? "#182947" : root.accentColor; Text { anchors.centerIn: parent; text: "Combined"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 12; font.bold: true } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setSplitMode(false) } }
                                Rectangle { width: 242; height: 44; radius: 15; color: root.splitMode ? root.accentColor : "#182947"; Text { anchors.centerIn: parent; text: "Split"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 12; font.bold: true } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setSplitMode(true) } }
                            }
                            Grid {
                                columns: 2
                                columnSpacing: 18
                                rowSpacing: 2
                                Item { width: 238; height: 34; Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Windows menu"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 44; height: 24; radius: 12; color: taskedSettings.startButtonEnabled ? root.accentColor : "#182947"; Rectangle { width: 18; height: 18; y: 3; x: taskedSettings.startButtonEnabled ? 23 : 3; radius: 9; color: "#F3F6FF"; Behavior on x { NumberAnimation { duration: 140 } } } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setStartButtonEnabled(!taskedSettings.startButtonEnabled) } } }
                                Item { width: 238; height: 34; Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Tasked button"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 44; height: 24; radius: 12; color: taskedSettings.taskButtonEnabled ? root.accentColor : "#182947"; Rectangle { width: 18; height: 18; y: 3; x: taskedSettings.taskButtonEnabled ? 23 : 3; radius: 9; color: "#F3F6FF"; Behavior on x { NumberAnimation { duration: 140 } } } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setTaskButtonEnabled(!taskedSettings.taskButtonEnabled) } } }
                                Item { width: 238; height: 34; Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Search"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 44; height: 24; radius: 12; color: taskedSettings.searchEnabled ? root.accentColor : "#182947"; Rectangle { width: 18; height: 18; y: 3; x: taskedSettings.searchEnabled ? 23 : 3; radius: 9; color: "#F3F6FF"; Behavior on x { NumberAnimation { duration: 140 } } } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setSearchEnabled(!taskedSettings.searchEnabled) } } }
                                Item { width: 238; height: 34; Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Clock"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 44; height: 24; radius: 12; color: taskedSettings.clockEnabled ? root.accentColor : "#182947"; Rectangle { width: 18; height: 18; y: 3; x: taskedSettings.clockEnabled ? 23 : 3; radius: 9; color: "#F3F6FF"; Behavior on x { NumberAnimation { duration: 140 } } } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setClockEnabled(!taskedSettings.clockEnabled) } } }
                                Item { width: 238; height: 34; Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Dividers"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 44; height: 24; radius: 12; color: taskedSettings.dividersEnabled ? root.accentColor : "#182947"; Rectangle { width: 18; height: 18; y: 3; x: taskedSettings.dividersEnabled ? 23 : 3; radius: 9; color: "#F3F6FF"; Behavior on x { NumberAnimation { duration: 140 } } } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setDividersEnabled(!taskedSettings.dividersEnabled) } } }
                                 Item { width: 238; height: 34; Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Wrap tray icons"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 44; height: 24; radius: 12; color: taskedSettings.trayWrapEnabled ? root.accentColor : "#182947"; Rectangle { width: 18; height: 18; y: 3; x: taskedSettings.trayWrapEnabled ? 23 : 3; radius: 9; color: "#F3F6FF"; Behavior on x { NumberAnimation { duration: 140 } } } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setTrayWrapEnabled(!taskedSettings.trayWrapEnabled) } } }
                                 Item { width: 238; height: 34; Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Scroll tray overflow"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 44; height: 24; radius: 12; color: taskedSettings.trayScrollEnabled ? root.accentColor : "#182947"; Rectangle { width: 18; height: 18; y: 3; x: taskedSettings.trayScrollEnabled ? 23 : 3; radius: 9; color: "#F3F6FF"; Behavior on x { NumberAnimation { duration: 140 } } } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setTrayScrollEnabled(!taskedSettings.trayScrollEnabled) } } }
                                 Item { width: 238; height: 34; Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Labels"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 44; height: 24; radius: 12; color: taskedSettings.labelsEnabled ? root.accentColor : "#182947"; Rectangle { width: 18; height: 18; y: 3; x: taskedSettings.labelsEnabled ? 23 : 3; radius: 9; color: "#F3F6FF"; Behavior on x { NumberAnimation { duration: 140 } } } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setLabelsEnabled(!taskedSettings.labelsEnabled) } } }
                                 Item { width: 238; height: 34; Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "System tray"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 44; height: 24; radius: 12; color: taskedSettings.trayEnabled ? root.accentColor : "#182947"; Rectangle { width: 18; height: 18; y: 3; x: taskedSettings.trayEnabled ? 23 : 3; radius: 9; color: "#F3F6FF"; Behavior on x { NumberAnimation { duration: 140 } } } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setTrayEnabled(!taskedSettings.trayEnabled) } } }
                                 Item { width: 238; height: 34; Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Dock position"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 100; height: 28; radius: 8; color: root.accentColor; Text { anchors.centerIn: parent; text: ["Bottom", "Top", "Left", "Right"][taskedSettings.dockPosition]; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 10; font.bold: true } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setDockPosition((taskedSettings.dockPosition + 1) % 4) } } }
                                 Item { width: 238; height: 34; Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Spaced sections"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 44; height: 24; radius: 12; color: taskedSettings.spacedMode ? root.accentColor : "#182947"; Rectangle { width: 18; height: 18; y: 3; x: taskedSettings.spacedMode ? 23 : 3; radius: 9; color: "#F3F6FF" } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setSpacedMode(!taskedSettings.spacedMode) } } }
                                 Item { width: 238; height: 34; Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Lock layout"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 44; height: 24; radius: 12; color: taskedSettings.dockLocked ? root.accentColor : "#182947"; Rectangle { width: 18; height: 18; y: 3; x: taskedSettings.dockLocked ? 23 : 3; radius: 9; color: "#F3F6FF" } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setDockLocked(!taskedSettings.dockLocked) } } }
                            }
                        }
                    }

                    Item {
                        anchors.fill: parent
                        visible: root.settingsTab === 2
                        Column {
                            anchors.fill: parent
                            spacing: 12
                            Text { text: "Behavior"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 15; font.bold: true }
                            Text { text: "Control what the dock shows and how it responds."; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 11 }
                            Item {
                                width: parent.width
                                height: 38
                                Column { anchors.left: parent.left; anchors.right: parent.right; anchors.rightMargin: 64; anchors.verticalCenter: parent.verticalCenter; spacing: 2; Text { text: "Seconds in clock"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 12; font.bold: true } Text { text: "Show HH:mm:ss instead of HH:mm."; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 10 } }
                                Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 44; height: 24; radius: 12; color: taskedSettings.secondsEnabled ? root.accentColor : "#182947"; Rectangle { width: 18; height: 18; y: 3; x: taskedSettings.secondsEnabled ? 23 : 3; radius: 9; color: "#F3F6FF" } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setSecondsEnabled(!taskedSettings.secondsEnabled) } }
                            }
                            Grid { width: parent.width; columns: 2; columnSpacing: 18; rowSpacing: 8; Item { width: 238; height: 34; Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Task previews"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 44; height: 24; radius: 12; color: taskedSettings.previewsEnabled ? root.accentColor : "#182947"; Rectangle { width: 18; height: 18; y: 3; x: taskedSettings.previewsEnabled ? 23 : 3; radius: 9; color: "#F3F6FF" } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setPreviewsEnabled(!taskedSettings.previewsEnabled) } } } Item { width: 238; height: 34; Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Animations"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 44; height: 24; radius: 12; color: taskedSettings.animationsEnabled ? root.accentColor : "#182947"; Rectangle { width: 18; height: 18; y: 3; x: taskedSettings.animationsEnabled ? 23 : 3; radius: 9; color: "#F3F6FF" } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setAnimationsEnabled(!taskedSettings.animationsEnabled) } } } }
                            Rectangle { width: parent.width; height: 70; radius: 14; color: "#182947"; Column { anchors.fill: parent; anchors.margins: 16; spacing: 5; Text { text: "Active task click"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 12; font.bold: true } Text { text: "Clicking the foreground task minimizes it; clicking another task activates it."; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 11; wrapMode: Text.WordWrap; width: parent.width } } }
                            Rectangle { width: parent.width; height: 70; radius: 14; color: "#182947"; Column { anchors.fill: parent; anchors.margins: 16; spacing: 5; Text { text: "Tray ordering"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 12; font.bold: true } Text { text: "Task activation never reorders tray or running-app items. Drag a task to change its order manually."; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 11; wrapMode: Text.WordWrap; width: parent.width } } }
                        }
                    }

                    Item {
                        anchors.fill: parent
                        visible: root.settingsTab === 3
                        Column {
                            anchors.fill: parent
                            spacing: 10
                            Text { text: "Typography"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 15; font.bold: true }
                            Text { text: "Tune the type and icon scale independently."; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 11 }
                            Row { width: parent.width; height: 34; Text { width: 110; text: "Font family"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } Item { width: parent.width - 210; height: 1 } Rectangle { width: 100; height: 30; radius: 9; color: root.accentColor; Text { anchors.centerIn: parent; text: taskedSettings.fontFamily; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 10; elide: Text.ElideRight } MouseArea { anchors.fill: parent; onClicked: { var choices = ["Segoe UI", "Arial", "Consolas"]; taskedSettings.setFontFamily(choices[(choices.indexOf(taskedSettings.fontFamily) + 1) % choices.length]) } } } }
                            Row { width: parent.width; height: 30; Text { width: 110; text: "Label size"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } Item { width: parent.width - 246; height: 1 } Text { width: 48; text: taskedSettings.labelSize + " px"; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 11; horizontalAlignment: Text.AlignRight; anchors.verticalCenter: parent.verticalCenter } Rectangle { width: 28; height: 28; radius: 8; color: "#182947"; Text { anchors.centerIn: parent; text: "−"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setLabelSize(taskedSettings.labelSize - 1) } } Rectangle { width: 28; height: 28; radius: 8; color: "#182947"; Text { anchors.centerIn: parent; text: "+"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setLabelSize(taskedSettings.labelSize + 1) } } }
                            Row { width: parent.width; height: 30; Text { width: 110; text: "Clock size"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } Item { width: parent.width - 246; height: 1 } Text { width: 48; text: taskedSettings.clockSize + " px"; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 11; horizontalAlignment: Text.AlignRight; anchors.verticalCenter: parent.verticalCenter } Rectangle { width: 28; height: 28; radius: 8; color: "#182947"; Text { anchors.centerIn: parent; text: "−"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setClockSize(taskedSettings.clockSize - 1) } } Rectangle { width: 28; height: 28; radius: 8; color: "#182947"; Text { anchors.centerIn: parent; text: "+"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setClockSize(taskedSettings.clockSize + 1) } } }
                            Row { width: parent.width; height: 30; Text { width: 110; text: "Icon size"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } Item { width: parent.width - 246; height: 1 } Text { width: 48; text: taskedSettings.iconSize + " px"; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 11; horizontalAlignment: Text.AlignRight; anchors.verticalCenter: parent.verticalCenter } Rectangle { width: 28; height: 28; radius: 8; color: "#182947"; Text { anchors.centerIn: parent; text: "−"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setIconSize(taskedSettings.iconSize - 2) } } Rectangle { width: 28; height: 28; radius: 8; color: "#182947"; Text { anchors.centerIn: parent; text: "+"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setIconSize(taskedSettings.iconSize + 2) } } }
                            Row { width: parent.width; height: 30; Text { width: 110; text: "Tray icon size"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } Item { width: parent.width - 246; height: 1 } Text { width: 48; text: taskedSettings.trayIconSize + " px"; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 11; horizontalAlignment: Text.AlignRight; anchors.verticalCenter: parent.verticalCenter } Rectangle { width: 28; height: 28; radius: 8; color: "#182947"; Text { anchors.centerIn: parent; text: "−"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setTrayIconSize(taskedSettings.trayIconSize - 1) } } Rectangle { width: 28; height: 28; radius: 8; color: "#182947"; Text { anchors.centerIn: parent; text: "+"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setTrayIconSize(taskedSettings.trayIconSize + 1) } } }
                            Row { width: parent.width; height: 30; Text { width: 110; text: "Tray scale"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter } Item { width: parent.width - 246; height: 1 } Text { width: 48; text: taskedSettings.trayScale + "%"; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 11; horizontalAlignment: Text.AlignRight; anchors.verticalCenter: parent.verticalCenter } Rectangle { width: 28; height: 28; radius: 8; color: "#182947"; Text { anchors.centerIn: parent; text: "−"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setTrayScale(taskedSettings.trayScale - 5) } } Rectangle { width: 28; height: 28; radius: 8; color: "#182947"; Text { anchors.centerIn: parent; text: "+"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setTrayScale(taskedSettings.trayScale + 5) } } }
                        }
                    }

                    Item {
                        anchors.fill: parent
                        visible: root.settingsTab === 4
                        Column {
                            anchors.fill: parent
                            spacing: 8
                            Text { text: "Extensions"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 15; font.bold: true }
                            Text { text: "Manage optional extensions discovered beside the executable."; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 11 }
                            Row { width: parent.width; height: 30; Text { width: parent.width - 92; text: "bin/extensions"; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 10; elide: Text.ElideMiddle; anchors.verticalCenter: parent.verticalCenter } Rectangle { width: 80; height: 28; radius: 9; color: root.accentColor; Text { anchors.centerIn: parent; text: "Rescan"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 10; font.bold: true } MouseArea { anchors.fill: parent; onClicked: extensions.refresh() } } }
                            ListView {
                                id: extensionList
                                width: parent.width
                                height: 195
                                clip: true
                                spacing: 8
                                model: extensions
                                delegate: Rectangle {
                                    id: extensionCard
                                    width: extensionList.width
                                    height: 76 + (expanded && extensionSettings.length > 0 ? extensionSettings.length * 36 + 8 : 0)
                                    radius: 14
                                    color: "#182947"
                                    property bool expanded: false
                                    property string extensionId: model.id
                                    property string extensionName: model.name
                                    property string extensionVersion: model.version
                                    property bool extensionEnabled: model.enabled
                                    property string extensionDescription: model.description
                                    property var extensionSettings: model.settings || []
                                    Column {
                                        anchors.fill: parent
                                        anchors.margins: 12
                                        spacing: 6
                                        Row {
                                            width: parent.width
                                            height: 28
                                            Column {
                                                width: parent.width - 92
                                                Text { width: parent.width; text: extensionCard.extensionName + (extensionCard.extensionVersion.length > 0 ? "  ·  v" + extensionCard.extensionVersion : ""); color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11; font.bold: true; elide: Text.ElideRight }
                                                Text { width: parent.width; text: extensionCard.extensionEnabled ? "Enabled" : "Disabled"; color: extensionCard.extensionEnabled ? root.accentColor : "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 9 }
                                            }
                                            Rectangle { width: 44; height: 24; radius: 12; anchors.verticalCenter: parent.verticalCenter; color: extensionCard.extensionEnabled ? root.accentColor : "#0F1B33"; Rectangle { width: 18; height: 18; y: 3; x: extensionCard.extensionEnabled ? 23 : 3; radius: 9; color: "#F3F6FF"; Behavior on x { NumberAnimation { duration: 140 } } } MouseArea { anchors.fill: parent; onClicked: extensions.setEnabled(extensionCard.extensionId, !extensionCard.extensionEnabled) } }
                                            Rectangle { width: 28; height: 28; radius: 8; color: "#243A60"; Text { anchors.centerIn: parent; text: extensionCard.expanded ? "−" : "+"; color: "#F3F6FF"; font.pixelSize: 16 } MouseArea { anchors.fill: parent; onClicked: extensionCard.expanded = !extensionCard.expanded } }
                                        }
                                        Text { visible: extensionCard.extensionDescription.length > 0; width: parent.width; text: extensionCard.extensionDescription; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 9; elide: Text.ElideRight }
                                        Column {
                                            visible: extensionCard.expanded
                                            width: parent.width
                                            spacing: 6
                                            Repeater {
                                                model: extensionCard.extensionSettings
                                                delegate: Item {
                                                    width: parent.width
                                                    height: 30
                                                    property var setting: modelData
                                                    Text { width: parent.width - 120; text: setting.label; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter; elide: Text.ElideRight }
                                                    Rectangle { visible: setting.type === "bool"; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 44; height: 24; radius: 12; color: setting.value ? root.accentColor : "#0F1B33"; Rectangle { width: 18; height: 18; y: 3; x: setting.value ? 23 : 3; radius: 9; color: "#F3F6FF" } MouseArea { anchors.fill: parent; onClicked: extensions.setSetting(extensionCard.extensionId, setting.key, !setting.value) } }
                                                    Row { visible: setting.type === "number"; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; spacing: 4; Text { width: 38; text: Number(setting.value).toFixed(1); color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 10; horizontalAlignment: Text.AlignRight } Rectangle { width: 25; height: 25; radius: 7; color: "#243A60"; Text { anchors.centerIn: parent; text: "−"; color: "#F3F6FF"; font.pixelSize: 14 } MouseArea { anchors.fill: parent; onClicked: extensions.setSetting(extensionCard.extensionId, setting.key, Number(setting.value) - Number(setting.step || 1)) } } Rectangle { width: 25; height: 25; radius: 7; color: "#243A60"; Text { anchors.centerIn: parent; text: "+"; color: "#F3F6FF"; font.pixelSize: 14 } MouseArea { anchors.fill: parent; onClicked: extensions.setSetting(extensionCard.extensionId, setting.key, Number(setting.value) + Number(setting.step || 1)) } } }
                                                    Rectangle { visible: setting.type === "enum"; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 110; height: 27; radius: 8; color: "#243A60"; Text { anchors.centerIn: parent; width: parent.width - 12; text: setting.value; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 9; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight } MouseArea { anchors.fill: parent; onClicked: { var options = setting.options; var current = options.indexOf(setting.value); extensions.setSetting(extensionCard.extensionId, setting.key, options[(current + 1) % options.length]) } } }
                                                    Rectangle { visible: setting.type === "string"; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 110; height: 27; radius: 8; color: "#243A60"; TextInput { anchors.fill: parent; anchors.margins: 7; text: setting.value; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 9; selectByMouse: true; onEditingFinished: extensions.setSetting(extensionCard.extensionId, setting.key, text) } }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            Text { visible: extensionList.count === 0; text: "No extensions found in bin/extensions."; color: "#AAB8D8"; font.family: root.fontFamily; font.pixelSize: 11 }
                        }
                    }
                }

                Row {
                    width: parent.width
                    height: 38
                    spacing: 8
                    Rectangle { width: 100; height: 38; radius: 11; color: "#182947"; Text { anchors.centerIn: parent; text: "Reset"; color: "#C7D8FF"; font.family: root.fontFamily; font.pixelSize: 11; font.bold: true } MouseArea { anchors.fill: parent; onClicked: taskedSettings.reset() } }
                    Item { width: parent.width - 216; height: 1 }
                    Rectangle { width: 100; height: 38; radius: 11; color: root.accentColor; Text { anchors.centerIn: parent; text: "Done"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11; font.bold: true } MouseArea { anchors.fill: parent; onClicked: root.settingsOpen = false } }
                }
            }
        }
    }

    Window {
        id: trayOverflowWindow
        visible: false
        width: Math.min(320, Math.max(126, Math.min(8, overflowTrayIcons.count) * (root.trayCellSize + 4) + 24))
        height: Math.max(82, Math.ceil(overflowTrayIcons.count / 8) * (root.trayCellSize + 4) + 54)
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
        color: "transparent"
        transientParent: root

        function openPanel() { var point = trayOverflowButton.mapToGlobal(0, 0); x = Math.round(point.x + (trayOverflowButton.width - width) / 2); y = Math.round(point.y - height - 10); if (y < 8) y = Math.round(point.y + trayOverflowButton.height + 10); show(); raise(); requestActivate() }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 5
            radius: 18
            color: root.surfaceColor
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.08)
            Text { anchors.left: parent.left; anchors.leftMargin: 16; anchors.top: parent.top; anchors.topMargin: 10; text: "Tray"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 14; font.bold: true }
            Flow {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.topMargin: 36
                anchors.bottomMargin: 12
                spacing: 6
                Repeater {
                    model: overflowTrayIcons
                    delegate: Item {
                        width: root.trayCellSize
                        height: root.trayCellSize
                        Rectangle { anchors.fill: parent; radius: 7; color: root.iconSurfaceColor }
                        Image { anchors.centerIn: parent; width: root.trayPixelSize; height: root.trayPixelSize; source: "image://tray/" + model.key; fillMode: Image.PreserveAspectFit; smooth: true }
                        MouseArea { anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton; onPressed: { if (mouse.button === Qt.RightButton) { var point = parent.mapToGlobal(mouse.x, mouse.y); trayIcons.showContextMenu(model.key, Math.round(point.x), Math.round(point.y)) } } onDoubleClicked: if (mouse.button === Qt.LeftButton) trayIcons.setOverflow(model.key, false); onClicked: if (mouse.button === Qt.LeftButton) { if (trayIcons.isSystemFlyoutItem(model.key)) root.showSystemTrayFlyout(); else trayIcons.activate(model.key, 0) } }
                    }
                }
            }
        }
    }

    Window {
        id: contextMenu
        visible: false
        width: 260
        height: root.trayContextKey.length > 0 ? 360 : 320
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
        color: "transparent"
        transientParent: root
        onActiveChanged: if (!active && visible) close()

        function openMenu(globalX, globalY) { var pointX = globalX === undefined ? root.x + (root.width - width) / 2 : globalX - width / 2; var pointY = root.dockPosition === 1 ? root.y + root.height + 10 : root.verticalDock ? (globalY === undefined ? root.y + (root.height - height) / 2 : globalY - height / 2) : root.y - height - 10; x = Math.round(Math.max(8, pointX)); y = Math.round(Math.max(8, pointY)); show(); raise(); requestActivate() }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 5
            radius: 20
            color: root.surfaceColor
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.08)

            Column {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 6
                Item { width: parent.width; height: 30; Text { text: "Tasked"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 18; font.bold: true } Text { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: "Menu"; color: root.accentColor; font.family: root.fontFamily; font.pixelSize: 11; font.bold: true } }
                Rectangle { width: parent.width; height: 1; color: "#20FFFFFF" }
                Rectangle { width: parent.width; height: 32; radius: 9; color: "#182947"; Text { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; text: "Settings"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } MouseArea { anchors.fill: parent; onClicked: { contextMenu.close(); root.settingsOpen = true } } }
                Rectangle { width: parent.width; height: 32; radius: 9; color: "#182947"; Text { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; text: "Seconds"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Text { anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter; text: taskedSettings.secondsEnabled ? "On" : "Off"; color: root.accentColor; font.family: root.fontFamily; font.pixelSize: 10 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setSecondsEnabled(!taskedSettings.secondsEnabled) } }
                Rectangle { width: parent.width; height: 32; radius: 9; color: "#182947"; Text { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; text: "Clock"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Text { anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter; text: taskedSettings.clockEnabled ? "Shown" : "Hidden"; color: root.accentColor; font.family: root.fontFamily; font.pixelSize: 10 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setClockEnabled(!taskedSettings.clockEnabled) } }
                Rectangle { width: parent.width; height: 32; radius: 9; color: "#182947"; Text { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; text: "Tray wrapping"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } Text { anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter; text: taskedSettings.trayWrapEnabled ? "On" : "Off"; color: root.accentColor; font.family: root.fontFamily; font.pixelSize: 10 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.setTrayWrapEnabled(!taskedSettings.trayWrapEnabled) } }
                Rectangle { visible: root.trayContextKey.length > 0; width: parent.width; height: 32; radius: 9; color: "#182947"; Text { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; text: trayIcons.isOverflow(root.trayContextKey) ? "Show tray icon" : "Move to overflow"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } MouseArea { anchors.fill: parent; onClicked: { trayIcons.setOverflow(root.trayContextKey, !trayIcons.isOverflow(root.trayContextKey)); root.trayContextKey = "" } } }
                Rectangle { width: parent.width; height: 32; radius: 9; color: "#182947"; Text { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; text: "Reset settings"; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: 11 } MouseArea { anchors.fill: parent; onClicked: taskedSettings.reset() } }
                Rectangle { width: parent.width; height: 32; radius: 9; color: "#42243A"; Text { anchors.centerIn: parent; text: "Exit Tasked"; color: "#FFD6E2"; font.family: root.fontFamily; font.pixelSize: 11; font.bold: true } MouseArea { anchors.fill: parent; onClicked: Qt.quit() } }
            }
        }
    }

    Window {
        id: previewWindow
        visible: false
        width: 360
        height: 240
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
        color: "transparent"
        transientParent: root
        onVisibleChanged: if (!visible) previewController.hide()

        Rectangle {
            anchors.fill: parent
            anchors.margins: 4
            radius: 20
            clip: true
            color: "#50182747"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.12)
            Text { anchors.left: parent.left; anchors.leftMargin: 16; anchors.top: parent.top; anchors.topMargin: 8; text: root.previewTitle; color: "#F3F6FF"; font.family: root.fontFamily; font.pixelSize: root.labelSize + 2; font.bold: true; elide: Text.ElideRight; width: parent.width - 32 }
            MouseArea { anchors.fill: parent; hoverEnabled: true; onEntered: previewCloseTimer.stop(); onExited: root.closePreview(); onClicked: { previewController.hide(); previewWindow.hide(); runningApps.activate(root.previewHandle) } }
        }
    }
}
