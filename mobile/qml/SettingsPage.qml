import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Page {
    property string url: bridge.aiUrl()
    property string model: bridge.aiModel()
    property string provider: bridge.aiProvider
    property string key: bridge.aiApiKey
    property string engineLabel: ""
    property string preset: bridge.aiPreset()
    property bool isCloudPreset: false
    property bool advancedOpen: false

    Component.onCompleted: initAi()

    background: Rectangle { color: T.bg }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: col.height
        clip: true

        ColumnLayout {
            id: col
            width: parent.width
            spacing: 12

            Text {
                text: "设置"
                font.pixelSize: 20
                font.bold: true
                color: T.textDark
                Layout.leftMargin: 16
                Layout.topMargin: 10
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                radius: 18
                color: T.card
                border.color: T.line
                implicitHeight: aiCardCol.implicitHeight + 28

                ColumnLayout {
                    id: aiCardCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Text {
                        text: "AI 设置"
                        font.pixelSize: 14
                        font.bold: true
                        color: T.textDark
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "当前引擎: " + engineLabel
                            font.pixelSize: 12
                            color: T.textMuted
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            width: 84
                            height: 30
                            radius: 15
                            color: T.greenSoft
                            border.color: T.greenBorder
                            border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: "重新检测"
                                font.pixelSize: 11
                                font.bold: true
                                color: T.greenDark
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    engineLabel = "检测中…"
                                    bridge.aiProbe()
                                }
                            }
                        }
                    }
                    Text {
                        text: "翻译、例句、文章生成都会用它；不好用就换下面这个。"
                        font.pixelSize: 11
                        color: T.textMuted
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            text: "选择 AI"
                            font.pixelSize: 11
                            color: T.textMuted
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 42
                            radius: 10
                            color: "#f7faf7"
                            border.color: T.line
                            ComboBox {
                                id: presetCombo
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                background: Rectangle { color: "transparent" }
                                model: [
                                    { text: "自动选择（推荐）", value: "auto" },
                                    { text: "本地小模型（免费离线）", value: "local" },
                                    { text: "本地 Ollama", value: "ollama" },
                                    { text: "DeepSeek（云端）", value: "deepseek" },
                                    { text: "通义千问（云端）", value: "dashscope" },
                                    { text: "智谱 GLM（云端）", value: "glm" },
                                    { text: "Kimi（云端）", value: "moonshot" },
                                    { text: "OpenAI（云端）", value: "openai" },
                                    { text: "自定义…", value: "custom" }
                                ]
                                textRole: "text"
                                valueRole: "value"
                                font.pixelSize: 13
                                onActivated: applyPreset(currentValue)
                            }
                        }
                    }
                    Field {
                        id: keyField
                        visible: isCloudPreset
                        label: "API Key（云端必填，本地可留空）"
                        text: key
                        placeholder: "sk-..."
                        password: true
                        onEdited: key = value
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Rectangle {
                            Layout.fillWidth: true
                            height: 40
                            radius: 10
                            color: "transparent"
                            Text {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: "高级设置（地址 / 模型名）"
                                font.pixelSize: 12
                                font.bold: true
                                color: T.blue
                            }
                            Text {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: advancedOpen ? "收起" : "展开"
                                font.pixelSize: 11
                                color: T.textMuted
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: advancedOpen = !advancedOpen
                            }
                        }
                        ColumnLayout {
                            visible: advancedOpen
                            Layout.fillWidth: true
                            spacing: 10
                            Field {
                                label: "服务地址"
                                text: url
                                placeholder: "http://192.168.1.100:11434"
                                onEdited: url = value
                            }
                            Field {
                                label: "模型名"
                                text: model
                                placeholder: "qwen2.5:1.5b"
                                onEdited: model = value
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Text {
                                    text: "服务类型"
                                    font.pixelSize: 11
                                    color: T.textMuted
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 40
                                    radius: 10
                                    color: "#f7faf7"
                                    border.color: T.line
                                    ComboBox {
                                        id: providerCombo
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        background: Rectangle { color: "transparent" }
                                        model: [
                                            { text: "本地 Ollama", value: "ollama" },
                                            { text: "OpenAI 兼容(DeepSeek/通义/GLM/Kimi)", value: "openai" }
                                        ]
                                        textRole: "text"
                                        valueRole: "value"
                                        font.pixelSize: 13
                                        onActivated: provider = currentValue
                                        Component.onCompleted: {
                                            for (var i = 0; i < model.length; ++i) {
                                                if (model[i].value === provider) {
                                                    currentIndex = i
                                                    break
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 42
                                radius: 21
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: T.greenBright }
                                    GradientStop { position: 1.0; color: T.green }
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: "保存"
                                    font.pixelSize: 15
                                    font.bold: true
                                    color: "#ffffff"
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: save()
                                }
                            }
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 10
                ChipBtn {
                    text: "赞助支持"
                    Layout.fillWidth: true
                    onClicked: donatePopup.open()
                }
                ChipBtn {
                    text: "使用说明"
                    Layout.fillWidth: true
                    onClicked: guidePopup.open()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                radius: 18
                color: T.card
                border.color: T.line
                implicitHeight: dataCardCol.implicitHeight + 28

                ColumnLayout {
                    id: dataCardCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Text {
                        text: "数据管理"
                        font.pixelSize: 14
                        font.bold: true
                        color: T.textDark
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        ChipBtn {
                            text: "重新导入内置词表"
                            Layout.fillWidth: true
                            onClicked: confirmPopup.mode = "reimport"
                        }
                        ChipBtn {
                            text: "重置全部进度"
                            Layout.fillWidth: true
                            onClicked: confirmPopup.mode = "reset"
                        }
                    }
                    Text {
                        text: "重新导入会更新词库和读音，重置会把所有词条退回未学状态。"
                        font.pixelSize: 11
                        color: T.textMuted
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }
            }

            Text {
                text: "提示:安卓端 AI 需要连接电脑或服务器的 ollama 地址,"
                      + "例如 http://192.168.1.100:11434"
                font.pixelSize: 11
                color: T.textMuted
                wrapMode: Text.Wrap
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 16
                Layout.fillWidth: true
            }
        }
    }

    Rectangle {
        id: toast
        visible: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        width: toastText.implicitWidth + 28
        height: 34
        radius: 17
        color: "#111827"
        opacity: 0
        z: 80
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
            onTriggered: {
                toastAnim.stop()
                toast.opacity = 0
                toast.visible = false
            }
        }
    }

    Popup {
        id: confirmPopup
        property string mode: ""
        anchors.centerIn: parent
        width: parent.width * 0.86
        modal: true
        focus: true
        background: Rectangle { radius: 18; color: T.card }
        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: confirmPopup.mode === "reimport"
                      ? "重新导入内置词表?" : "重置全部进度?"
                font.pixelSize: 16
                font.bold: true
                color: T.textDark
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: confirmPopup.mode === "reimport"
                      ? "会重新导入核心 3000 词库、词形和音标,不会清空学习记录。"
                      : "所有词条将退回「未学」状态,此操作不可恢复。"
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
                font.pixelSize: 12
                color: T.textBody
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                ChipBtn {
                    text: "取消"
                    Layout.fillWidth: true
                    onClicked: confirmPopup.close()
                }
                ChipBtn {
                    text: "确认"
                    Layout.fillWidth: true
                    onClicked: {
                        if (confirmPopup.mode === "reimport")
                            bridge.reimportBuiltin()
                        else
                            bridge.resetAllProgress()
                        confirmPopup.close()
                        showToast("完成")
                    }
                }
            }
        }
    }

    Popup {
        id: donatePopup
        anchors.centerIn: parent
        width: parent.width * 0.9
        modal: true
        focus: true
        background: Rectangle { radius: 18; color: T.card }
        contentItem: ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Text {
                text: "如果 English 3000 对你有帮助,欢迎赞助支持,让项目可以继续走下去。"
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
                font.pixelSize: 14
                color: T.textBody
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Image {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 150
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/assets/donate/qr1.jpg"
                }
                Image {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 150
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/assets/donate/qr2.jpg"
                }
            }
            Text {
                text: "扫一扫上面的二维码即可支持。感谢每一位使用者。"
                color: T.textMuted
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
                font.pixelSize: 12
            }
            ChipBtn {
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
        background: Rectangle { radius: 18; color: T.card }
        contentItem: ColumnLayout {
            anchors.fill: parent
            spacing: 10
            Text {
                text: "使用说明"
                font.bold: true
                font.pixelSize: 18
                color: T.textDark
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
                    GuideItem {
                        title: "快速开始"
                        body: "选一个词表,学习页自动进入卡片流;点卡片显示释义,认识/不认识一键记录。"
                    }
                    GuideItem {
                        title: "学习与复习"
                        body: "学习=没学过的词;点「不认识」的词自动进复习队列,点「开始复习」再学。没有每日上限,进度自动保存。"
                    }
                    GuideItem {
                        title: "词表"
                        body: "每个词表独立记录未学/待复习/已掌握;AI 生成、从文章提取、导入 CSV 都可以建新词表。"
                    }
                    GuideItem {
                        title: "阅读"
                        body: "红色=未入词表,蓝色=其他词表,绿色=当前词表,黑色=已掌握;点单词发音、加入阅读词表。"
                    }
                    GuideItem {
                        title: "翻译"
                        body: "翻译遇到的生词会自动收集到「翻译生词」词表,之后可以在词表页选中它来学习。"
                    }
                    GuideItem {
                        title: "AI 设置"
                        body: "本地 Ollama 或 OpenAI 兼容(DeepSeek/通义/GLM/Kimi);默认模型 qwen2.5:1.5b,日常够用。"
                    }
                    GuideItem {
                        title: "更新与数据"
                        body: "所有数据保存在本机;设置页可重新导入内置词表或重置学习进度。"
                    }
                }
            }
            ChipBtn {
                text: "关闭"
                Layout.fillWidth: true
                onClicked: guidePopup.close()
            }
        }
    }

    Connections {
        target: bridge
        function onAiProbeFinished(label) {
            engineLabel = label
            showToast("AI 检测完成")
        }
    }

    function initAi() {
        selectPreset(preset)
        if (isCloudPreset && key.length === 0)
            showToast("云端 AI 需要填写 API Key")
    }

    function selectPreset(v) {
        preset = v
        isCloudPreset = (v === "deepseek" || v === "dashscope"
                         || v === "glm" || v === "moonshot"
                         || v === "openai")
        for (var i = 0; i < presetCombo.model.length; ++i) {
            if (presetCombo.model[i].value === v) {
                presetCombo.currentIndex = i
                break
            }
        }
        if (v === "auto") {
            engineLabel = "检测中…"
        } else {
            engineLabel = bridge.aiProvider === "openai"
                          ? bridge.aiModel() : "本地 " + bridge.aiModel()
        }
    }

    function applyPreset(v) {
        preset = v
        isCloudPreset = (v === "deepseek" || v === "dashscope"
                         || v === "glm" || v === "moonshot"
                         || v === "openai")
        if (v === "auto")
            engineLabel = "检测中…"
        bridge.setAiPreset(v)
        engineLabel = bridge.aiProvider === "openai"
                      ? bridge.aiModel() : "本地 " + bridge.aiModel()
        showToast("已切换到" + presetCombo.currentText)
    }

    function showToast(msg) {
        toastText.text = msg
        toast.visible = true
        toastAnim.start()
        toastTimer.start()
    }

    function save() {
        bridge.setAiUrl(url)
        bridge.setAiModel(model)
        bridge.setAiProvider(provider)
        bridge.setAiApiKey(key)
        showToast("已保存")
    }

    component Field: ColumnLayout {
        id: root
        property string label: ""
        property string text: ""
        property string placeholder: ""
        property bool password: false
        signal edited(string value)
        spacing: 4
        Text {
            text: root.label
            font.pixelSize: 11
            color: T.textMuted
        }
        Rectangle {
            Layout.fillWidth: true
            height: 40
            radius: 10
            color: "#f7faf7"
            border.color: T.line
            TextField {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                background: Rectangle { color: "transparent" }
                text: root.text
                placeholderText: root.placeholder
                echoMode: root.password ? TextInput.Password : TextInput.Normal
                font.pixelSize: 13
                color: T.textDark
                onEditingFinished: root.edited(text)
            }
        }
    }

    component ChipBtn: Rectangle {
        id: root
        property string text: ""
        signal clicked()
        height: 40
        radius: 20
        color: T.greenSoft
        border.color: T.greenBorder
        border.width: 1
        Text {
            anchors.centerIn: parent
            text: root.text
            font.pixelSize: 13
            font.bold: true
            color: T.greenDark
        }
        MouseArea {
            anchors.fill: parent
            onClicked: root.clicked()
        }
    }

    component GuideItem: Column {
        property string title: ""
        property string body: ""
        spacing: 3
        Text {
            text: title
            font.bold: true
            font.pixelSize: 13
            color: T.blue
        }
        Text {
            text: body
            wrapMode: Text.Wrap
            width: parent.width
            font.pixelSize: 12
            color: T.textBody
        }
    }
}
