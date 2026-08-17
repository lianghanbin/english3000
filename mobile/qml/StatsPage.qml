import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Page {
    property int total: bridge.newCount + bridge.dueCount + bridge.masteredCount
    property int articlesCount: bridge.articles().length
    property int listsCount: bridge.wordLists().length

    background: Rectangle { color: T.bg }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: col.height
        clip: true

        ColumnLayout {
            id: col
            width: parent.width
            spacing: 14

            Text {
                text: "学习循环 · 你的成长"
                font.pixelSize: 20
                font.bold: true
                color: T.textDark
                Layout.leftMargin: 16
                Layout.topMargin: 10
            }

            Item {
                Layout.preferredWidth: 230
                Layout.preferredHeight: 230
                Layout.alignment: Qt.AlignHCenter

                Rectangle {
                    anchors.centerIn: parent
                    width: 200
                    height: 200
                    radius: 100
                    color: "transparent"
                    border.color: T.green
                    border.width: 3
                    opacity: 0.3
                }
                Item {
                    id: ringSpin
                    anchors.centerIn: parent
                    width: 200
                    height: 200
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        y: -8
                        width: 34
                        height: 34
                        radius: 17
                        color: T.green
                        Text {
                            anchors.centerIn: parent
                            text: "→"
                            color: "#ffffff"
                            font.pixelSize: 16
                            font.bold: true
                        }
                    }
                    NumberAnimation on rotation {
                        from: 0
                        to: 360
                        duration: 14000
                        loops: Animation.Infinite
                    }
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 88
                    height: 88
                    radius: 44
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: T.greenBright }
                        GradientStop { position: 1.0; color: T.greenDark }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "AI"
                        font.pixelSize: 26
                        font.bold: true
                        color: "#ffffff"
                    }
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 112
                    height: 112
                    radius: 56
                    color: "transparent"
                    border.color: T.greenBright
                    border.width: 3
                    opacity: 0.5
                    SequentialAnimation on rotation {
                        running: true
                        loops: Animation.Infinite
                        PropertyAnimation { to: 360; duration: 2200 }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 8
                NodeCard {
                    label: "已掌握单词"
                    value: bridge.masteredCount + ""
                    sub: "累计掌握"
                }
                NodeCard {
                    label: "3000 覆盖率"
                    value: (total === 0 ? 0
                            : Math.round(bridge.masteredCount / total * 100))
                           + "%"
                    sub: "核心 3000"
                }
                NodeCard {
                    label: "阅读篇数"
                    value: articlesCount + ""
                    sub: "篇"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 8
                StatCard { label: "学习中"; value: bridge.dueCount + "" }
                StatCard { label: "未学"; value: bridge.newCount + "" }
                StatCard { label: "词表数"; value: listsCount + "" }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 16
                radius: 18
                color: T.card
                border.color: T.line

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    ProgressRow {
                        label: "核心 3000 掌握"
                        pct: total === 0 ? 0 : bridge.masteredCount / total
                    }
                    ProgressRow {
                        label: "连续学习(30 天目标)"
                        pct: Math.min(bridge.streak / 30, 1)
                    }
                    Text {
                        text: "单词,原来如此简单\n完全免费 · 开源"
                        font.pixelSize: 13
                        font.bold: true
                        color: T.green
                        horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                    }
                }
            }
        }
    }

    component NodeCard: Rectangle {
        id: root
        property string label: ""
        property string value: ""
        property string sub: ""
        Layout.fillWidth: true
        height: 78
        radius: 14
        color: T.card
        border.color: T.line
        Column {
            anchors.centerIn: parent
            spacing: 2
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.value
                font.pixelSize: 22
                font.bold: true
                color: T.greenDark
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.label
                font.pixelSize: 11
                font.bold: true
                color: T.textBody
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.sub
                font.pixelSize: 9
                color: T.textMuted
            }
        }
    }

    component StatCard: Rectangle {
        id: root
        property string label: ""
        property string value: ""
        Layout.fillWidth: true
        height: 64
        radius: 14
        color: T.card
        border.color: T.line
        Column {
            anchors.centerIn: parent
            spacing: 2
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.value
                font.pixelSize: 20
                font.bold: true
                color: T.green
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.label
                font.pixelSize: 10
                color: T.textMuted
            }
        }
    }

    component ProgressRow: Item {
        id: root
        property string label: ""
        property real pct: 0
        Layout.fillWidth: true
        height: 36
        Column {
            anchors.fill: parent
            spacing: 4
            Text {
                text: root.label
                font.pixelSize: 11
                font.bold: true
                color: T.textBody
            }
            Rectangle {
                width: parent.width
                height: 10
                radius: 5
                color: T.track
                Rectangle {
                    width: parent.width * root.pct
                    height: parent.height
                    radius: 5
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: T.green }
                        GradientStop { position: 1.0; color: T.greenBright }
                    }
                }
            }
        }
    }
}
