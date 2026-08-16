import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Page {
    property var lists: []
    property var rows: []
    property int currentId: -1
    property string currentName: ""

    background: Rectangle { color: T.bg }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 128
            Layout.fillHeight: true
            radius: 14
            color: T.card
            border.color: T.line

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6

                Text {
                    text: "我的词表"
                    font.pixelSize: 14
                    font.bold: true
                    color: T.textDark
                }

                ListView {
                    id: sideList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: lists
                    delegate: Rectangle {
                        width: sideList.width
                        height: 46
                        radius: 8
                        color: modelData.current ? T.greenSoft : "transparent"
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            spacing: 1
                            Text {
                                text: modelData.name
                                font.pixelSize: 13
                                font.bold: modelData.current
                                color: T.textDark
                            }
                            Text {
                                text: modelData.wordCount + " 词"
                                font.pixelSize: 10
                                color: T.textMuted
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                bridge.setCurrentList(modelData.id)
                                bridge.refresh()
                                refresh()
                            }
                        }
                    }
                }

                SideBtn {
                    text: "✦ AI 生成词表"
                    onClicked: showToast("AI 生成词表请先在桌面版使用,手机版稍后支持")
                }
                SideBtn {
                    text: "＋ AI 补充词表"
                    onClicked: showToast("AI 补充词表请先在桌面版使用,手机版稍后支持")
                }
                Text {
                    text: "词表即书\n进度独立记录"
                    font.pixelSize: 10
                    color: T.textMuted
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: currentName === "" ? "词表" : currentName
                    font.pixelSize: 17
                    font.bold: true
                    color: T.textDark
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: "未学 " + bridge.newCount + " · 已掌握 "
                          + bridge.masteredCount
                    font.pixelSize: 11
                    color: T.textMuted
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 30
                radius: 8
                color: "#f2f7f2"
                Row {
                    anchors.fill: parent
                    HeadCell { text: "单词"; w: 0.28 }
                    HeadCell { text: "释义"; w: 0.5 }
                    HeadCell { text: "状态"; w: 0.22 }
                }
            }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentHeight: rowsCol.height
                Column {
                    id: rowsCol
                    width: parent.width
                    Repeater {
                        model: rows
                        delegate: RowItem {}
                    }
                }
            }
        }
    }

    Rectangle {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        width: toastText.implicitWidth + 28
        height: 34
        radius: 17
        color: "#111827"
        opacity: 0
        z: 50
        Text {
            id: toastText
            anchors.centerIn: parent
            color: "#ffffff"
            font.pixelSize: 13
        }
        NumberAnimation on opacity {
            id: toastAnim
            from: 0
            to: 1
            duration: 200
        }
        Timer {
            id: toastTimer
            interval: 1400
            onTriggered: toast.opacity = 0
        }
    }

    Connections {
        target: bridge
        function onCountsChanged() { refresh() }
    }

    function showToast(msg) {
        toastText.text = msg
        toastAnim.start()
        toastTimer.start()
    }

    function refresh() {
        lists = bridge.wordLists()
        currentId = -1
        for (var i = 0; i < lists.length; ++i) {
            if (lists[i].current) {
                currentId = lists[i].id
                currentName = lists[i].name
                break
            }
        }
        rows = currentId >= 0 ? bridge.wordListRows(currentId, 40) : []
    }

    Component.onCompleted: refresh()

    component SideBtn: Rectangle {
        id: root
        property string text: ""
        signal clicked()
        height: 32
        radius: 8
        color: T.greenSoft
        Layout.fillWidth: true
        Text {
            anchors.centerIn: parent
            text: root.text
            font.pixelSize: 11
            font.bold: true
            color: T.greenDark
        }
        MouseArea {
            anchors.fill: parent
            onClicked: root.clicked()
        }
    }

    component HeadCell: Item {
        property string text: ""
        property real w: 1
        width: parent.width * w
        height: parent.height
        Text {
            anchors.centerIn: parent
            text: parent.text
            font.pixelSize: 11
            font.bold: true
            color: T.textMuted
        }
    }

    component RowItem: Rectangle {
        id: row
        width: rowsCol.width
        height: 42
        color: index % 2 === 0 ? "#fbfdfb" : "#ffffff"
        border.color: T.line
        border.width: 0

        Row {
            anchors.fill: parent
            Item {
                width: parent.width * 0.28
                height: parent.height
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    spacing: 4
                    Text {
                        text: modelData.word
                        font.pixelSize: 13
                        font.bold: true
                        color: T.greenDark
                    }
                    Text {
                        visible: rowTapTimer.running
                        text: "♪"
                        font.pixelSize: 12
                        color: T.green
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
            Item {
                width: parent.width * 0.5
                height: parent.height
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    width: parent.width - 8
                    elide: Text.ElideRight
                    text: modelData.meaning
                    font.pixelSize: 12
                    color: T.textBody
                }
            }
            Item {
                width: parent.width * 0.22
                height: parent.height
                Rectangle {
                    anchors.centerIn: parent
                    width: chipText.implicitWidth + 16
                    height: 20
                    radius: 10
                    color: chipBg(modelData.status)
                    Text {
                        id: chipText
                        anchors.centerIn: parent
                        text: chipLabel(modelData.status)
                        font.pixelSize: 10
                        font.bold: true
                        color: chipFg(modelData.status)
                    }
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                bridge.speak(modelData.word)
                rowPop.start()
                rowTapTimer.restart()
            }
        }
        SequentialAnimation {
            id: rowPop
            NumberAnimation {
                target: row
                property: "scale"
                to: 1.02
                duration: 90
            }
            NumberAnimation {
                target: row
                property: "scale"
                to: 1
                duration: 140
            }
        }
        Timer {
            id: rowTapTimer
            interval: 1300
        }
    }

    function chipLabel(status) {
        if (status === "mastered") return "已掌握"
        if (status === "learning") return "学习中"
        return "新词"
    }
    function chipBg(status) {
        if (status === "mastered") return T.greenSoft
        if (status === "learning") return T.amberSoft
        return T.redSoft
    }
    function chipFg(status) {
        if (status === "mastered") return T.green
        if (status === "learning") return T.amber
        return T.red
    }
}
