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

        SwipeView {
            id: swipe
            width: parent.width
            height: parent.height - 56
            interactive: true

            onCurrentIndexChanged: {
                if (currentIndex === 0)
                    swipe.itemAt(0).reloadIfNeeded()
            }

            StudyPage {}
            WordListsPage {}
            ReadingPage {}
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
                NavItem {
                    label: "学习"
                    active: swipe.currentIndex === 0
                    onClicked: swipe.currentIndex = 0
                }
                NavItem {
                    label: "词表"
                    active: swipe.currentIndex === 1
                    onClicked: swipe.currentIndex = 1
                }
                NavItem {
                    label: "阅读"
                    active: swipe.currentIndex === 2
                    onClicked: swipe.currentIndex = 2
                }
                NavItem {
                    label: "翻译"
                    active: swipe.currentIndex === 3
                    onClicked: swipe.currentIndex = 3
                }
                NavItem {
                    label: "数据"
                    active: swipe.currentIndex === 4
                    onClicked: swipe.currentIndex = 4
                }
                NavItem {
                    label: "设置"
                    active: swipe.currentIndex === 5
                    onClicked: swipe.currentIndex = 5
                }
            }
        }
    }

    Rectangle {
        id: splash
        anchors.fill: parent
        z: 100
        visible: true

        property var wallWords: [
            "the", "of", "and", "to", "a", "in", "for", "on",
            "that", "by", "this", "with", "from", "they", "would",
            "about", "know", "people", "time", "like", "just",
            "word", "good", "some", "come", "work", "make", "take",
            "give", "read", "learn", "word", "every", "day", "think",
            "child", "story", "world", "house", "water", "light",
            "heart", "mind", "hand", "place", "great", "small",
            "open", "begin", "always", "never", "often", "again",
            "enough", "because", "through", "between", "together",
            "important", "different", "beautiful"
        ]
        property int perCol: 42
        property int lineH: 23
        property int halfH: perCol * lineH

        function wallWord(col, idx) {
            var r = Math.floor(idx / splash.perCol)
            var i = idx % splash.perCol
            var n = (col * splash.perCol + i + r * 7) % splash.wallWords.length
            return splash.wallWords[n]
        }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#123c22" }
                GradientStop { position: 0.55; color: "#0b2a18" }
                GradientStop { position: 1.0; color: "#07170e" }
            }
        }

        Repeater {
            model: Math.ceil(splash.height / 44) + 1
            Rectangle {
                y: index * 44
                width: splash.width
                height: 2
                color: "#ffffff"
                opacity: 0.014
            }
        }

        Row {
            anchors.fill: parent
            Repeater {
                model: 4
                Item {
                    id: colItem
                    property int ci: index
                    width: parent.width / 4
                    height: parent.height
                    clip: true
                    Column {
                        width: parent.width
                        spacing: 8
                        Repeater {
                            model: splash.perCol * 2
                            Text {
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                text: splash.wallWord(parent.parent.ci, index)
                                font.pixelSize: 15
                                font.bold: true
                                color: "#d6f0de"
                                opacity: 0.78
                            }
                        }
                        NumberAnimation on y {
                            from: -colItem.ci * splash.halfH / 5
                            to: -colItem.ci * splash.halfH / 5
                                - splash.halfH
                            duration: [ 34000, 26000, 40000, 30000 ][colItem.ci]
                            loops: Animation.Infinite
                            running: true
                            easing.type: Easing.Linear
                        }
                    }
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#040e08" }
                GradientStop { position: 0.28; color: "transparent" }
                GradientStop { position: 1.0; color: "transparent" }
            }
            opacity: 0.5
        }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.72; color: "transparent" }
                GradientStop { position: 1.0; color: "#040e08" }
            }
            opacity: 0.6
        }

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 64
            spacing: 8
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "英语的核心日常词汇，其实只有三千个"
                font.pixelSize: 17
                font.bold: true
                color: "#f1fbf4"
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "三千词，是日常的地图，也是通往英文世界的第一把钥匙"
                font.pixelSize: 11
                color: "#a9d8b8"
            }
        }

        NumberAnimation on opacity {
            id: splashFade
            from: 1
            to: 0
            duration: 500
            running: false
        }
        Timer {
            interval: 3000
            running: true
            repeat: false
            onTriggered: splashFade.start()
        }
        onOpacityChanged: {
            if (opacity <= 0)
                visible = false
        }
    }

    component NavItem: Item {
        id: root
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
                color: root.active ? T.green : "transparent"
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: root.label
                font.pixelSize: 18
                font.bold: root.active
                color: root.active ? T.green : T.navInactive
            }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: root.clicked()
        }
    }
}
