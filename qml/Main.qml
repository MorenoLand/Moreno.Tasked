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
    property bool splitMode: false
    property string clock: Qt.formatTime(new Date(), "HH:mm")
    property string date: Qt.formatDate(new Date(), "MMM d")
    property var launchers: [
        { name: "Explorer", target: "explorer.exe", tint: "#F4C75B" },
        { name: "Terminal", target: "cmd.exe", tint: "#1C2237" },
        { name: "Notepad", target: "notepad.exe", tint: "#E8EDF5" },
        { name: "Paint", target: "mspaint.exe", tint: "#59B4D8" }
    ]

    Timer { interval: 1000; running: true; repeat: true; onTriggered: { root.clock = Qt.formatTime(new Date(), "HH:mm"); root.date = Qt.formatDate(new Date(), "MMM d") } }

    Rectangle { id: solidSurface; width: dock.width; height: 76; anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; radius: 22; color: "#E815244E"; visible: !root.splitMode }

    Row {
        id: dock
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: root.splitMode ? 10 : 0

        Rectangle {
            width: 184
            height: 76
            radius: root.splitMode ? 22 : 0
            color: root.splitMode ? "#E815244E" : "transparent"

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
                        Text { anchors.centerIn: parent; text: "T"; color: "white"; font.pixelSize: 21; font.bold: true }
                    }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; text: "Tasked"; color: "#F3F6FF"; font.pixelSize: 10 }
                    MouseArea { id: taskMouse; anchors.fill: parent; hoverEnabled: true }
                }

                Rectangle { width: 1; height: 40; anchors.verticalCenter: parent.verticalCenter; color: "#40FFFFFF" }

                Item {
                    width: 42
                    height: 64
                    Rectangle {
                        id: filesButton
                        width: 36
                        height: 36
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        radius: 10
                        color: "#E8B85C"
                        scale: filesMouse.containsMouse ? 1.1 : 1
                        Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
                        Image { anchors.fill: parent; anchors.margins: 5; source: "image://shell/explorer.exe"; fillMode: Image.PreserveAspectFit }
                    }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; text: "Files"; color: "#F3F6FF"; font.pixelSize: 10 }
                    MouseArea { id: filesMouse; anchors.fill: parent; hoverEnabled: true; onClicked: launcher.launch("explorer.exe") }
                }

                Item {
                    width: 42
                    height: 64
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
                    MouseArea { id: searchMouse; anchors.fill: parent; hoverEnabled: true }
                }
            }
        }

        Rectangle {
            width: 580
            height: 76
            radius: root.splitMode ? 22 : 0
            color: root.splitMode ? "#E815244E" : "transparent"

            Row {
                anchors.centerIn: parent
                spacing: 10

                Repeater {
                    model: root.launchers
                    delegate: Item {
                        width: 66
                        height: 66
                        Rectangle {
                            id: icon
                            width: 44
                            height: 44
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            radius: 13
                            color: modelData.tint
                            scale: mouse.containsMouse ? 1.1 : 1
                            Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
                            Image { id: appIcon; anchors.fill: parent; anchors.margins: 7; source: "image://shell/" + modelData.target; fillMode: Image.PreserveAspectFit; visible: status === Image.Ready }
                            Text { anchors.centerIn: parent; text: modelData.name.charAt(0); color: "#FFFFFF"; font.pixelSize: 18; font.bold: true; visible: appIcon.status !== Image.Ready }
                        }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; text: modelData.name; color: "#F3F6FF"; font.pixelSize: 10 }
                        MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: launcher.launch(modelData.target) }
                    }
                }

                Rectangle { width: 1; height: 40; anchors.verticalCenter: parent.verticalCenter; color: "#40FFFFFF" }

                ListView {
                    width: 250
                    height: 66
                    clip: true
                    orientation: ListView.Horizontal
                    spacing: 8
                    model: runningApps
                    delegate: Item {
                        width: 60
                        height: 66
                        Rectangle {
                            id: runningIcon
                            width: 42
                            height: 42
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            radius: 12
                            color: "#31446F"
                            scale: runningMouse.containsMouse ? 1.1 : 1
                            Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
                            Image { id: runningImage; anchors.fill: parent; anchors.margins: 6; source: model.iconSource; fillMode: Image.PreserveAspectFit; visible: status === Image.Ready }
                            Text { anchors.centerIn: parent; text: model.title.charAt(0); color: "#F3F6FF"; font.pixelSize: 17; font.bold: true; visible: runningImage.status !== Image.Ready }
                            Rectangle { width: 20; height: 3; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; radius: 2; color: model.active ? "#79B7FF" : "transparent" }
                        }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: -1; width: 60; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter; text: model.title; color: "#F3F6FF"; font.pixelSize: 9 }
                        MouseArea { id: runningMouse; anchors.fill: parent; hoverEnabled: true; onClicked: runningApps.activate(model.windowHandle) }
                    }
                }
            }
        }

        Rectangle {
            width: 250
            height: 76
            radius: root.splitMode ? 22 : 0
            color: root.splitMode ? "#E815244E" : "transparent"

            Row {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                ListView {
                    width: 148
                    height: 42
                    anchors.verticalCenter: parent.verticalCenter
                    clip: true
                    orientation: ListView.Horizontal
                    spacing: 6
                    model: trayIcons
                    delegate: Item {
                        width: 26
                        height: 34
                        Image { id: trayImage; anchors.centerIn: parent; width: 24; height: 24; source: "image://tray/" + model.key; fillMode: Image.PreserveAspectFit }
                        Text { anchors.centerIn: parent; text: model.tooltip.charAt(0); color: "#F3F6FF"; font.pixelSize: 12; font.bold: true; visible: trayImage.status !== Image.Ready }
                        MouseArea { anchors.fill: parent; hoverEnabled: true; onEntered: trayIcons.activate(model.key, 3); onClicked: trayIcons.activate(model.key, 0); onPressed: if (mouse.button === Qt.RightButton) trayIcons.activate(model.key, 1) }
                    }
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: root.clock; color: "#F3F6FF"; font.pixelSize: 20; font.bold: true }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: root.date; color: "#BFCBEE"; font.pixelSize: 10 }
                }
            }
        }
    }
}
