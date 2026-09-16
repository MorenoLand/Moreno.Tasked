import QtQuick
import QtQuick.Window

Window {
    id: root
    visible: false
    width: 1080
    height: 88
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    color: "transparent"
    title: "Tasked"
    property bool splitMode: taskedSettings.splitMode
    property bool searchEnabled: taskedSettings.searchEnabled
    property bool settingsOpen: false
    property string clock: Qt.formatTime(new Date(), "HH:mm")
    property string date: Qt.formatDate(new Date(), "MMM d")
    Timer { interval: 1000; running: true; repeat: true; onTriggered: { root.clock = Qt.formatTime(new Date(), "HH:mm"); root.date = Qt.formatDate(new Date(), "MMM d") } }

    Rectangle {
        id: solidSurface
        width: dock.width
        height: 76
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        radius: 22
        color: "#E815244E"
        opacity: root.splitMode ? 0 : 1
        scale: root.splitMode ? 0.98 : 1
        Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.InOutCubic } }
        Behavior on scale { NumberAnimation { duration: 220; easing.type: Easing.InOutCubic } }
    }

    Row {
        id: dock
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: root.splitMode ? 10 : 0
        Behavior on spacing { NumberAnimation { duration: 220; easing.type: Easing.InOutCubic } }

        Rectangle {
            width: 120
            height: 76
            radius: root.splitMode ? 22 : 0
            color: root.splitMode ? "#E815244E" : "transparent"
            opacity: root.splitMode ? 1 : 0.98
            Behavior on radius { NumberAnimation { duration: 220; easing.type: Easing.InOutCubic } }
            Behavior on color { ColorAnimation { duration: 220 } }
            Behavior on opacity { NumberAnimation { duration: 180 } }

            Row {
                anchors.centerIn: parent
                spacing: 8

                Item {
                    width: 54
                    height: 64
                    Rectangle {
                        id: taskButton
                        width: 42
                        height: 42
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        radius: 12
                        color: "#6D8DF4"
                        scale: taskMouse.containsMouse ? 1.1 : 1
                        Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            gradient: Gradient {
                                GradientStop { position: 0; color: "#83B0FF" }
                                GradientStop { position: 1; color: "#5371DF" }
                            }
                            opacity: 0.72
                        }
                        Text { anchors.centerIn: parent; text: "T"; color: "white"; font.pixelSize: 21; font.bold: true }
                    }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; text: "Tasked"; color: "#F3F6FF"; font.pixelSize: 10 }
                    MouseArea { id: taskMouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.settingsOpen = !root.settingsOpen }
                }

                Rectangle { width: 1; height: 40; anchors.verticalCenter: parent.verticalCenter; color: "#40FFFFFF" }

                Item {
                    width: root.searchEnabled ? 42 : 0
                    height: 64
                    visible: root.searchEnabled
                    Rectangle {
                        id: searchButton
                        width: 36
                        height: 36
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        radius: 10
                        color: "#33456F"
                        scale: searchMouse.containsMouse ? 1.1 : 1
                        Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: "⌕"; color: "#F3F6FF"; font.pixelSize: 23 }
                    }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; text: "Search"; color: "#F3F6FF"; font.pixelSize: 10 }
                    MouseArea { id: searchMouse; anchors.fill: parent; hoverEnabled: true; onClicked: launcher.launch("search-ms:") }
                }
            }
        }

        Rectangle {
            width: 700
            height: 76
            radius: root.splitMode ? 22 : 0
            color: root.splitMode ? "#E815244E" : "transparent"
            opacity: root.splitMode ? 1 : 0.98
            Behavior on radius { NumberAnimation { duration: 220; easing.type: Easing.InOutCubic } }
            Behavior on color { ColorAnimation { duration: 220 } }
            Behavior on opacity { NumberAnimation { duration: 180 } }

            Row {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                ListView {
                    id: runningList
                    width: parent.width
                    height: 66
                    clip: true
                    orientation: ListView.Horizontal
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    model: runningApps
                    add: Transition { NumberAnimation { properties: "x,opacity"; from: 16; to: 0; duration: 220; easing.type: Easing.OutCubic } }
                    displaced: Transition { NumberAnimation { properties: "x"; duration: 180; easing.type: Easing.InOutCubic } }
                    delegate: Item {
                        id: runningDelegate
                        width: 60
                        height: 66
                        property bool dragged: false
                        Rectangle {
                            id: runningIcon
                            width: 42
                            height: 42
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 6
                            radius: 12
                            color: "#31446F"
                            opacity: runningMouse.containsMouse ? 1 : 0.94
                            scale: runningMouse.containsMouse ? 1.1 : 1
                            Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
                            Behavior on opacity { NumberAnimation { duration: 130 } }
                            Image { id: runningImage; anchors.fill: parent; anchors.margins: 6; source: model.iconSource; fillMode: Image.PreserveAspectFit; visible: status === Image.Ready }
                            Text { anchors.centerIn: parent; text: model.title.charAt(0); color: "#F3F6FF"; font.pixelSize: 17; font.bold: true; visible: runningImage.status !== Image.Ready }
                            Rectangle { width: 20; height: 3; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; radius: 2; color: model.active ? "#79B7FF" : "transparent" }
                        }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 6; width: 60; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter; text: model.title; color: "#F3F6FF"; font.pixelSize: 9 }
                        MouseArea {
                            id: runningMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            drag.target: runningDelegate
                            drag.axis: Drag.XAxis
                            property real pressX: 0
                            onPressed: { pressX = mouse.x; runningDelegate.dragged = false }
                            onPositionChanged: if (Math.abs(mouse.x - pressX) > 6) runningDelegate.dragged = true
                            onReleased: if (runningDelegate.dragged) { var target = Math.round((runningDelegate.x + runningDelegate.width / 2) / (runningDelegate.width + runningList.spacing)); target = Math.max(0, Math.min(runningList.count - 1, target)); runningApps.move(index, target) }
                            onClicked: { if (!runningDelegate.dragged) runningApps.activate(model.windowHandle); runningDelegate.dragged = false }
                        }
                    }
                }
            }
        }

        Rectangle {
            width: 240
            height: 76
            radius: root.splitMode ? 22 : 0
            color: root.splitMode ? "#E815244E" : "transparent"
            opacity: root.splitMode ? 1 : 0.98
            Behavior on radius { NumberAnimation { duration: 220; easing.type: Easing.InOutCubic } }
            Behavior on color { ColorAnimation { duration: 220 } }
            Behavior on opacity { NumberAnimation { duration: 180 } }

            Row {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                ListView {
                    width: 168
                    height: 42
                    anchors.verticalCenter: parent.verticalCenter
                    clip: true
                    orientation: ListView.Horizontal
                    spacing: 4
                    model: trayIcons
                    delegate: Item {
                        width: 30
                        height: 34
                        scale: trayMouse.containsMouse ? 1.12 : 1
                        Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
                        Rectangle { anchors.centerIn: parent; width: 30; height: 30; radius: 9; color: "#263960" }
                        Image { id: trayImage; anchors.centerIn: parent; width: 26; height: 26; source: "image://tray/" + model.key; fillMode: Image.PreserveAspectFit; smooth: true }
                        Text { anchors.centerIn: parent; text: model.tooltip.charAt(0); color: "#F3F6FF"; font.pixelSize: 12; font.bold: true; visible: trayImage.status !== Image.Ready }
                        MouseArea { id: trayMouse; anchors.fill: parent; hoverEnabled: true; onEntered: trayIcons.activate(model.key, 3); onClicked: trayIcons.activate(model.key, 0); onPressed: if (mouse.button === Qt.RightButton) trayIcons.activate(model.key, 1) }
                    }
                }

                Column {
                    width: 50
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1
                    Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: root.clock; color: "#F3F6FF"; font.pixelSize: 20; font.bold: true }
                    Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: root.date; color: "#BFCBEE"; font.pixelSize: 10 }
                }
            }
        }
    }

    onSettingsOpenChanged: if (settingsOpen) settingsWindow.openPanel(); else settingsWindow.closePanel()

    Window {
        id: settingsWindow
        visible: false
        width: 430
        height: 365
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
        color: "transparent"
        transientParent: root

        function positionPanel() { x = root.x + (root.width - width) / 2; y = root.y - height - 12 }
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
            NumberAnimation { target: card; property: "opacity"; to: 1; duration: 180; easing.type: Easing.OutCubic }
            NumberAnimation { target: card; property: "scale"; to: 1; duration: 220; easing.type: Easing.OutBack }
        }

        SequentialAnimation {
            id: closeAnimation
            ParallelAnimation {
                NumberAnimation { target: card; property: "opacity"; to: 0; duration: 120; easing.type: Easing.InCubic }
                NumberAnimation { target: card; property: "scale"; to: 0.96; duration: 120; easing.type: Easing.InCubic }
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
            Behavior on scale { NumberAnimation { duration: 220; easing.type: Easing.OutBack } }

            Rectangle { anchors.fill: parent; anchors.margins: -7; radius: 31; color: "#50050B19"; z: -1 }

            Column {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                anchors.topMargin: 22
                anchors.bottomMargin: 20
                spacing: 16

                Row {
                    width: parent.width
                    height: 34
                    Text { text: "Settings"; color: "#F3F6FF"; font.pixelSize: 22; font.bold: true }
                    Item { width: parent.width - 110; height: 1 }
                    Text { text: "Styling"; color: "#93B9FF"; font.pixelSize: 12; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                }

                Rectangle { width: parent.width; height: 1; color: "#20FFFFFF" }

                Column {
                    width: parent.width
                    spacing: 8
                    Text { text: "Dock layout"; color: "#F3F6FF"; font.pixelSize: 14; font.bold: true }
                    Text { text: "Choose one surface or three coordinated sections."; color: "#AAB8D8"; font.pixelSize: 11 }
                    Row {
                        spacing: 10
                        Rectangle {
                            width: 180
                            height: 78
                            radius: 16
                            color: root.splitMode ? "#182947" : "#315A96"
                            scale: combinedMouse.containsMouse ? 1.02 : 1
                            Behavior on color { ColorAnimation { duration: 160 } }
                            Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
                            Column {
                                anchors.centerIn: parent
                                spacing: 8
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Combined"; color: "#F3F6FF"; font.pixelSize: 12; font.bold: true }
                                Rectangle { width: 108; height: 10; radius: 5; color: "#9BBEFF" }
                            }
                            MouseArea { id: combinedMouse; anchors.fill: parent; hoverEnabled: true; onClicked: taskedSettings.setSplitMode(false) }
                        }
                        Rectangle {
                            width: 180
                            height: 78
                            radius: 16
                            color: root.splitMode ? "#315A96" : "#182947"
                            scale: splitMouse.containsMouse ? 1.02 : 1
                            Behavior on color { ColorAnimation { duration: 160 } }
                            Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
                            Column {
                                anchors.centerIn: parent
                                spacing: 8
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Split"; color: "#F3F6FF"; font.pixelSize: 12; font.bold: true }
                                Row {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    spacing: 4
                                    Rectangle { width: 30; height: 10; radius: 5; color: "#9BBEFF" }
                                    Rectangle { width: 30; height: 10; radius: 5; color: "#82A8ED" }
                                    Rectangle { width: 30; height: 10; radius: 5; color: "#6F94D7" }
                                }
                            }
                            MouseArea { id: splitMouse; anchors.fill: parent; hoverEnabled: true; onClicked: taskedSettings.setSplitMode(true) }
                        }
                    }
                }

                Row {
                    width: parent.width
                    height: 34
                    Column {
                        spacing: 2
                        Text { text: "Search button"; color: "#F3F6FF"; font.pixelSize: 13; font.bold: true }
                        Text { text: "Keep it out of the dock unless enabled."; color: "#AAB8D8"; font.pixelSize: 10 }
                    }
                    Item { width: parent.width - 190; height: 1 }
                    Rectangle {
                        width: 44
                        height: 24
                        radius: 12
                        anchors.verticalCenter: parent.verticalCenter
                        color: taskedSettings.searchEnabled ? "#79A9FF" : "#263A61"
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Rectangle {
                            width: 18
                            height: 18
                            y: 3
                            x: taskedSettings.searchEnabled ? 23 : 3
                            radius: 9
                            color: "#F3F6FF"
                            Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        }
                        MouseArea { anchors.fill: parent; onClicked: taskedSettings.setSearchEnabled(!taskedSettings.searchEnabled) }
                    }
                }

                Item { width: parent.width; height: 1 }
                Rectangle {
                    width: parent.width
                    height: 38
                    radius: 12
                    color: "#182947"
                    Text { anchors.centerIn: parent; text: "Done"; color: "#C7D8FF"; font.pixelSize: 12; font.bold: true }
                    MouseArea { anchors.fill: parent; onClicked: root.settingsOpen = false }
                }
            }
        }
    }
}
