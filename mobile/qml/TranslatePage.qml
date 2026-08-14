import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: "翻译（生词自动加入「翻译生词」词表）"
            font.bold: true
        }

        RowLayout {
            TextField {
                id: input
                Layout.fillWidth: true
                placeholderText: "输入英文"
            }
            Button {
                text: "翻译"
                onClicked: {
                    if (input.text.trim() !== "")
                        bridge.translate(input.text, "qwen2.5:1.5b")
                }
            }
        }

        TextArea {
            id: result
            Layout.fillWidth: true
            Layout.fillHeight: true
            readOnly: true
            placeholderText: "译文显示在这里"
        }

        Connections {
            target: bridge
            function onTranslationReady(t) { result.text = t }
            function onTranslationFailed(m) { result.text = "翻译失败：" + m }
        }
    }
}
