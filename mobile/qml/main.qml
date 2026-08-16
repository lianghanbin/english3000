import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

ApplicationWindow {
    id: win
    visible: true
    width: 420
    height: 760
    title: qsTr("English 3000")
    color: T.bg

    Column {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            width: parent.width
            height: 32
            color: T.bg
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                Text {
                    id: clockText
                    font.pixelSize: 19
                    font.bold: true
                    color: "#1a1a1a"
                    function update() {
                        var d = new Date()
                        var h = d.getHours()
                        var m = d.getMinutes()
                        text = (h < 10 ? "0" : "") + h + ":"
                               + (m < 10 ? "0" : "") + m
                    }
                    Timer {
                        interval: 15000
                        repeat: true
                        running: true
                        onTriggered: clockText.update()
                    }
                    Component.onCompleted: update()
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: "▮▮▮ 100%"
                    font.pixelSize: 17
                    color: "#333333"
                }
            }
        }

        SwipeView {
            id: swipe
            width: parent.width
            height: parent.height - 32 - 56
            interactive: true

            StudyPage {}
            ReadingPage {}
            WordListsPage {}
            TranslatePage {}
            StatsPage {}
            SettingsPage {}
        }

        Rectangle {
            width: parent.width
            height: 56
            color: T.card
            border.color: T.line

            Row {
                anchors.fill: parent
                Repeater {
                    model: ["学习", "阅读", "词表", "翻译", "数据", "设置"]
                    NavItem {
                        label: modelData
                        active: swipe.currentIndex === index
                        onClicked: swipe.currentIndex = index
                    }
                }
            }
        }
    }

    component NavItem: Item {
        property string label: ""
        property bool active: false
        signal clicked()
        width: parent.width / 6
        height: parent.height

        Column {
            anchors.centerIn: parent
            spacing: 3
            Rectangle {
                width: 6
                height: 6
                radius: 3
                color: active ? T.green : "transparent"
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: label
                font.pixelSize: 18
                font.bold: active
                color: active ? T.green : T.navInactive
            }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: parent.clicked()
        }
    }
}
