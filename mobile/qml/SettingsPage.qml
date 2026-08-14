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
        ComboBox {
            id: providerCombo
            Layout.fillWidth: true
            model: [
                { text: "本地 Ollama", value: "ollama" },
                { text: "OpenAI 兼容（DeepSeek/通义/GLM/Kimi）", value: "openai" }
            ]
            textRole: "text"
            valueRole: "value"
            Component.onCompleted: {
                for (var i = 0; i < model.length; ++i) {
                    if (model[i].value === bridge.aiProvider) {
                        currentIndex = i
                        break
                    }
                }
            }
        }
        TextField {
            id: keyField
            Layout.fillWidth: true
            text: bridge.aiApiKey
            echoMode: TextInput.Password
            placeholderText: "API Key（本地 Ollama 可留空）"
        }
        Button {
            text: "保存"
            Layout.fillWidth: true
            onClicked: {
                bridge.setAiUrl(urlField.text)
                bridge.setAiModel(modelField.text)
                bridge.setAiProvider(providerCombo.currentValue)
                bridge.setAiApiKey(keyField.text)
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
