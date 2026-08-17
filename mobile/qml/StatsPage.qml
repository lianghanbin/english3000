import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Page {
    property int total: bridge.newCount + bridge.dueCount + bridge.masteredCount
    property int articlesCount: bridge.articles().length
    property int listsCount: bridge.wordLists().length
    property var coverage: []

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
                radius: 18
                color: T.card
                border.color: T.line
                implicitHeight: coverCol.implicitHeight + 28

                ColumnLayout {
                    id: coverCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 8

                    Text {
                        text: "近 7 天覆盖率"
                        font.pixelSize: 14
                        font.bold: true
                        color: T.textDark
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Repeater {
                            model: coverage
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.preferredWidth: 18
                                    Layout.preferredHeight: 60
                                    radius: 4
                                    color: T.track
                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        width: parent.width
                                        height: parent.height
                                                * (modelData.coverage / 100)
                                        radius: 4
                                        color: T.green
                                    }
                                }
                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: modelData.date
                                    font.pixelSize: 9
                                    color: T.textMuted
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 16
                radius: 18
                color: T.card
                border.color: T.line
                implicitHeight: progressCol.implicitHeight + 28

                ColumnLayout {
                    id: progressCol
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
                }
            }
        }
    }

    function refreshCoverage() {
        coverage = bridge.coverageHistory(7)
    }

    Component.onCompleted: refreshCoverage()

    Connections {
        target: bridge
        function onCountsChanged() {
            refreshCoverage()
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
