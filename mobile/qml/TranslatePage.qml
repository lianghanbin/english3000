import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Page {
    property bool translating: false

    background: Rectangle { color: T.bg }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Text {
            text: "翻译"
            font.pixelSize: 20
            font.bold: true
            color: T.textDark
        }
        Text {
            text: "生词会自动加入「翻译生词」词表,之后可在词表页学习"
            font.pixelSize: 12
            color: T.textMuted
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            height: 46
            radius: 14
            color: T.card
            border.color: T.line

            TextField {
                id: input
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 76
                background: Rectangle { color: "transparent" }
                placeholderText: "输入英文或中文"
                font.pixelSize: 15
                color: T.textDark
            }
            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                width: 64
                height: 34
                radius: 17
                color: translating ? T.greenSoft : T.green
                Text {
                    anchors.centerIn: parent
                    text: translating ? "…" : "翻译"
                    font.pixelSize: 13
                    font.bold: true
                    color: translating ? T.green : "#ffffff"
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: !translating
                    onClicked: doTranslate()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 18
            color: T.deskDark
            border.color: T.deskBorder

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Text {
                    text: "翻译结果"
                    font.pixelSize: 13
                    font.bold: true
                    color: T.deskText
                }
                Row {
                    spacing: 6
                    Repeater {
                        model: 3
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: T.deskAccent
                            opacity: 0.25
                            SequentialAnimation on opacity {
                                running: translating
                                loops: Animation.Infinite
                                NumberAnimation { to: 1; duration: 240 }
                                NumberAnimation { to: 0.25; duration: 240 }
                            }
                        }
                    }
                    Text {
                        text: translating ? "正在翻译…" : "等待输入"
                        font.pixelSize: 12
                        color: translating ? T.deskAccent : "#7b93c2"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                Text {
                    id: result
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: ""
                    font.pixelSize: 16
                    font.bold: true
                    color: T.deskAccent
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    Connections {
        target: bridge
        function onTranslationReady(t) {
            result.text = t
            translating = false
        }
        function onTranslationFailed(m) {
            result.text = "翻译失败:" + m
            translating = false
        }
    }

    function doTranslate() {
        var t = input.text.trim()
        if (t === "") return
        result.text = ""
        translating = true
        bridge.translate(t, "")
    }
}
