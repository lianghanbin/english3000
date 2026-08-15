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

        Button {
            text: "赞助支持"
            Layout.fillWidth: true
            onClicked: donatePopup.open()
        }

        Button {
            text: "使用说明"
            Layout.fillWidth: true
            onClicked: guidePopup.open()
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

    Popup {
        id: donatePopup
        anchors.centerIn: parent
        width: parent.width * 0.9
        modal: true
        focus: true
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Label {
                text: "如果 English 3000 对你有帮助，欢迎赞助支持，让项目可以继续走下去。"
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Image {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 160
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/assets/donate/qr1.jpg"
                }
                Image {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 160
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/assets/donate/qr2.jpg"
                }
            }
            Label {
                text: "扫一扫上面的二维码即可支持。感谢每一位使用者。"
                color: "#888888"
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
            Button {
                text: "关闭"
                Layout.fillWidth: true
                onClicked: donatePopup.close()
            }
        }
    }

    Popup {
        id: guidePopup
        anchors.centerIn: parent
        width: parent.width * 0.92
        height: parent.height * 0.9
        modal: true
        focus: true
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            Label {
                text: "使用说明"
                font.bold: true
                font.pixelSize: 18
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: content.implicitHeight
                clip: true
                Column {
                    id: content
                    width: parent.width
                    spacing: 12
                    GuideItem { title: "快速开始"; body: "选一个词表，点「开始学习」进入卡片流；空格显示释义，认识/不认识一键切换，Esc 随时退出。" }
                    GuideItem { title: "学习与复习"; body: "学习=没学过的词；点「不认识」的词自动进复习队列，点「开始复习」再学。没有每日上限，进度自动保存。" }
                    GuideItem { title: "词表"; body: "每个词表独立记录未学/待复习/已掌握；AI 生成、从文章提取、导入 CSV 都可以建新词表。" }
                    GuideItem { title: "阅读"; body: "红色=未入词表，蓝色=其他词表，绿色=当前词表，黑色=已掌握；点单词发音、加入阅读词表。" }
                    GuideItem { title: "翻译"; body: "翻译遇到的生词会自动收集到「翻译生词」词表，之后可以在词表页选中它来学习。" }
                    GuideItem { title: "AI 设置"; body: "本地 Ollama 或 OpenAI 兼容（DeepSeek/通义/GLM/Kimi）；默认模型 qwen2.5:1.5b，日常够用。" }
                    GuideItem { title: "更新与数据"; body: "所有数据保存在本机；设置页可检查更新，发现新版本可一键更新。" }
                }
            }
            Button {
                text: "关闭"
                Layout.fillWidth: true
                onClicked: guidePopup.close()
            }
        }
    }

    component GuideItem: Column {
        property string title: ""
        property string body: ""
        spacing: 3
        Label {
            text: title
            font.bold: true
            color: "#1565c0"
        }
        Label {
            text: body
            wrapMode: Text.Wrap
            width: parent.width
            color: "#cccccc"
        }
    }
}
