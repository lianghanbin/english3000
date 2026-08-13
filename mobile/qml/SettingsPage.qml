import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label { text: "AI 设置"; font.bold: true }

        TextField {
            id: urlField
            Layout.fillWidth: true
            text: bridge.aiUrl()
            placeholderText: "AI 服务地址"
        }
        TextField {
            id: modelField
            Layout.fillWidth: true
            text: bridge.aiModel()
            placeholderText: "模型名"
        }
        Button {
            text: "保存"
            Layout.fillWidth: true
            onClicked: {
                bridge.setAiUrl(urlField.text)
                bridge.setAiModel(modelField.text)
            }
        }

        Label {
            Layout.topMargin: 20
            color: "#888888"
            text: "提示：安卓端 AI 需要连接电脑或服务器的 ollama 地址，"
                  + "例如 http://192.168.1.100:11434"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }
}
