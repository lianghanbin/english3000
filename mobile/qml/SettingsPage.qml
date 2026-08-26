import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Page {
    property string url: bridge.aiUrl()
    property string model: bridge.aiModel()
    property string provider: bridge.aiProvider
    property string key: bridge.aiApiKey
    property string preset: bridge.aiPreset()
    property bool isCloudPreset: false
    property bool advancedOpen: false
    property bool testing: false
    property bool testOk: false
    property string testMessage: ""
    // 当前发音音色,由 bridge.ttsVoiceChanged() 驱动刷新,
    // 避免直接绑定 C++ 方法导致界面不更新
    property string voiceId: bridge.ttsVoice()
    // 发音预下载进度(-1=空闲,否则 0..total)
    property int preloadDone: -1
    property int preloadTotal: 0
    // 当前词表待下载发音的预估占用空间
    property string preloadEstimate: ""
    // 当前选中云端服务商的名称/获取 Key 地址,供引导卡片使用
    readonly property string currentProviderName: ({
        deepseek: "DeepSeek", dashscope: "通义千问",
        glm: "智谱 GLM", moonshot: "Kimi", openai: "OpenAI"
    })[preset] || "服务商"
    readonly property string currentKeyUrl: ({
        deepseek: "https://platform.deepseek.com/api_keys",
        dashscope: "https://bailian.console.aliyun.com/?apiKey=1#/api-key",
        glm: "https://bigmodel.cn/usercenter/proj-mgmt/apikeys",
        moonshot: "https://platform.moonshot.cn/console/api-keys",
        openai: "https://platform.openai.com/api-keys"
    })[preset] || ""

    Component.onCompleted: {
        initAi()
        preloadEstimate = bridge.ttsPreloadEstimate()
    }

    Connections {
        target: bridge
        function onTtsVoiceChanged() {
            voiceId = bridge.ttsVoice()
            preloadEstimate = bridge.ttsPreloadEstimate()
        }
        function onTtsPreloadProgress(done, total) {
            if (total === 0 || done >= total) {
                preloadDone = -1
                preloadEstimate = bridge.ttsPreloadEstimate()
                if (total > 0)
                    showToast("发音已全部缓存,可离线使用")
            } else {
                preloadDone = done
                preloadTotal = total
            }
        }
        function onUpdateCheckResult(available, latest, url) {
            if (latest === "") {
                showToast("检查失败,请检查网络后重试")
                return
            }
            if (available) {
                updatePopup.latestVersion = latest
                updatePopup.releaseUrl = url
                updatePopup.open()
            } else {
                showToast("已是最新版本 v" + latest)
            }
        }
    }

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
                        text: "AI 服务"
                        font.pixelSize: 14
                        font.bold: true
                        color: T.textDark
                    }
                    Text {
                        text: "手机版使用云端 AI。选一家服务商,粘贴 API Key,点下方保存即可。"
                        font.pixelSize: 11
                        color: T.textMuted
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }

                    // 服务商列表
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: [
                                { value: "deepseek", name: "DeepSeek",
                                  desc: "性价比高,中文好,推荐",
                                  keyUrl: "https://platform.deepseek.com/api_keys" },
                                { value: "glm", name: "智谱 GLM",
                                  desc: "GLM-4-Flash 完全免费",
                                  keyUrl: "https://bigmodel.cn/usercenter/proj-mgmt/apikeys" },
                                { value: "dashscope", name: "通义千问",
                                  desc: "阿里云,免费额度多",
                                  keyUrl: "https://bailian.console.aliyun.com/?apiKey=1#/api-key" },
                                { value: "moonshot", name: "Kimi",
                                  desc: "长上下文,适合文章",
                                  keyUrl: "https://platform.moonshot.cn/console/api-keys" },
                                { value: "openai", name: "OpenAI",
                                  desc: "GPT 系列,需海外网络",
                                  keyUrl: "https://platform.openai.com/api-keys" },
                                { value: "ollama", name: "局域网 Ollama",
                                  desc: "连同一 Wi-Fi 的电脑", keyUrl: "" }
                            ]
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                required property var modelData
                                height: 52
                                radius: 12
                                color: preset === modelData.value
                                       ? T.greenSoft : T.track
                                border.width: preset === modelData.value ? 1.5 : 1
                                border.color: preset === modelData.value
                                              ? T.green : T.line
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 10
                                    spacing: 8
                                    Rectangle {
                                        width: 16; height: 16; radius: 8
                                        color: "transparent"
                                        border.width: 2
                                        border.color: preset === modelData.value
                                                      ? T.green : T.textMuted
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 8; height: 8; radius: 4
                                            color: preset === modelData.value
                                                   ? T.green : "transparent"
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        Text {
                                            text: modelData.name
                                            font.pixelSize: 14
                                            font.bold: true
                                            color: T.textDark
                                        }
                                        Text {
                                            text: modelData.desc
                                            font.pixelSize: 10
                                            color: T.textMuted
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }
                                    Rectangle {
                                        visible: modelData.keyUrl !== ""
                                                && preset !== modelData.value
                                        width: keyTxt.implicitWidth + 18
                                        height: 28
                                        radius: 14
                                        color: "transparent"
                                        border.width: 1
                                        border.color: T.blue
                                        Text {
                                            id: keyTxt
                                            anchors.centerIn: parent
                                            text: "获取 Key"
                                            font.pixelSize: 11
                                            font.bold: true
                                            color: T.blue
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: bridge.openUrl(modelData.keyUrl)
                                        }
                                    }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        if (preset !== modelData.value)
                                            applyPreset(modelData.value)
                                        else if (modelData.keyUrl !== "")
                                            bridge.openUrl(modelData.keyUrl)
                                    }
                                }
                            }
                        }
                    }

                    // 云端 API Key 输入(智能按钮:空=粘贴,有内容=清除)
                    ColumnLayout {
                        visible: isCloudPreset
                        Layout.fillWidth: true
                        spacing: 6
                        Text {
                            text: "API Key"
                            font.pixelSize: 11
                            color: T.textMuted
                        }
                        // 整行就是一个大按钮:空时点一下直接粘贴,无需键盘/长按
                        Rectangle {
                            id: keyRow
                            Layout.fillWidth: true
                            height: 48
                            radius: 12
                            color: key.length > 0 ? T.greenSoft : T.blue
                            border.width: 1
                            border.color: key.length > 0 ? T.greenBorder : T.blue
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 12
                                spacing: 8
                                Text {
                                    text: key.length > 0 ? "✓ 已填入:" : "📋"
                                    font.pixelSize: key.length > 0 ? 13 : 18
                                    font.bold: true
                                    color: key.length > 0 ? T.greenDark : "#ffffff"
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: key.length > 0 ? maskKey(key)
                                          : "点这里,一键粘贴剪贴板里的 Key"
                                    font.pixelSize: key.length > 0 ? 13 : 14
                                    font.bold: !key.length
                                    color: key.length > 0 ? T.textDark : "#ffffff"
                                    elide: Text.ElideRight
                                }
                                Rectangle {
                                    visible: key.length > 0
                                    width: replaceTxt.implicitWidth + 16
                                    height: 30
                                    radius: 15
                                    color: T.card
                                    border.width: 1
                                    border.color: T.greenBorder
                                    Text {
                                        id: replaceTxt
                                        anchors.centerIn: parent
                                        text: "更换"
                                        font.pixelSize: 11
                                        font.bold: true
                                        color: T.greenDark
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: doPaste()
                                    }
                                }
                                Rectangle {
                                    visible: key.length > 0
                                    width: clearTxt.implicitWidth + 16
                                    height: 30
                                    radius: 15
                                    color: "transparent"
                                    border.width: 1
                                    border.color: T.textMuted
                                    Text {
                                        id: clearTxt
                                        anchors.centerIn: parent
                                        text: "清除"
                                        font.pixelSize: 11
                                        color: T.textMuted
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: { key = "" }
                                    }
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                // 有 Key 时整行不再响应点击(按钮各自处理),
                                // 空 Key 时点整行直接粘贴。
                                enabled: key.length === 0
                                onClicked: doPaste()
                            }
                        }
                        Text {
                            text: "先到上面选一家服务商,点「获取 Key」复制,再回来点这里。"
                            font.pixelSize: 10
                            color: T.textMuted
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                            visible: key.length === 0
                        }
                    }

                    // 局域网 Ollama 地址提示
                    Rectangle {
                        visible: !isCloudPreset && preset === "ollama"
                        Layout.fillWidth: true
                        radius: 12
                        color: T.amberSoft
                        border.color: T.amber
                        implicitHeight: ollamaHint.implicitHeight + 20
                        Text {
                            id: ollamaHint
                            anchors.fill: parent
                            anchors.margins: 10
                            text: "手机不跑模型,需在同一 Wi-Fi 的电脑上运行 ollama,"
                                  + "并在下方「高级设置」填电脑局域网 IP。"
                            font.pixelSize: 11
                            color: T.textDark
                            wrapMode: Text.Wrap
                        }
                    }

                    // 高级设置(地址/模型名),折叠
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Rectangle {
                            Layout.fillWidth: true
                            height: 38
                            color: "transparent"
                            Text {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: "高级设置:地址 / 模型名"
                                font.pixelSize: 12
                                color: T.textMuted
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
                                placeholder: "https://api.deepseek.com"
                                onEdited: url = value
                            }
                            Field {
                                label: "模型名"
                                text: model
                                placeholder: "deepseek-chat"
                                onEdited: model = value
                            }
                            Text {
                                text: "一般不用改,选好服务商后会自动填好。"
                                font.pixelSize: 10
                                color: T.textMuted
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // 连接结果(仅失败时显示,成功用按钮文案反馈)
                    Rectangle {
                        visible: testMessage.length > 0 && !testOk
                        Layout.fillWidth: true
                        radius: 10
                        color: T.redSoft
                        border.width: 1
                        border.color: T.red
                        implicitHeight: resultText.implicitHeight + 18
                        Text {
                            id: resultText
                            anchors.fill: parent
                            anchors.margins: 9
                            text: testMessage
                            font.pixelSize: 11
                            color: T.red
                            wrapMode: Text.Wrap
                        }
                    }

                    // 唯一的主按钮:保存并测试
                    Rectangle {
                        id: saveBtn
                        Layout.fillWidth: true
                        height: 46
                        radius: 23
                        color: testing ? T.textMuted : T.green
                        Text {
                            anchors.centerIn: parent
                            text: testing ? "连接测试中…"
                                  : (testOk && testMessage.length > 0
                                     ? "✓ 已连接 · 点此重新测试"
                                     : (isCloudPreset ? "保存并测试连接" : "保存"))
                            font.pixelSize: 15
                            font.bold: true
                            color: "#ffffff"
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (testing) return
                                save()
                                if (isCloudPreset) {
                                    testing = true
                                    testMessage = ""
                                    testOk = false
                                    bridge.testConnection()
                                } else {
                                    showToast("已保存")
                                }
                            }
                        }
                    }
                }
            }

            // 外观
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                radius: 18
                color: T.card
                border.color: T.line
                implicitHeight: appearanceCol.implicitHeight + 28

                ColumnLayout {
                    id: appearanceCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Text {
                        text: "外观"
                        font.pixelSize: 14
                        font.bold: true
                        color: T.textDark
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: [
                                { v: 0, label: "跟随系统" },
                                { v: 1, label: "浅色" },
                                { v: 2, label: "深色" }
                            ]
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                height: 40
                                radius: 12
                                color: bridge.themeMode === modelData.v
                                       ? T.blueSoft : T.track
                                border.color: bridge.themeMode === modelData.v
                                              ? T.blue : T.line
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    font.pixelSize: 13
                                    font.bold: bridge.themeMode === modelData.v
                                    color: bridge.themeMode === modelData.v
                                           ? T.blue : T.textBody
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: bridge.themeMode = modelData.v
                                }
                            }
                        }
                    }
                }
            }

            // 发音音色
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                radius: 18
                color: T.card
                border.color: T.line
                implicitHeight: voiceCol.implicitHeight + 28

                ColumnLayout {
                    id: voiceCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "发音音色"
                            font.pixelSize: 14
                            font.bold: true
                            color: T.textDark
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: voiceId === "system" ? "离线" : "联网缓存"
                            font.pixelSize: 11
                            color: voiceId === "system" ? T.textMuted : T.green
                        }
                    }
                    Text {
                        text: "选系统声=零延迟离线;选神经网络音=更自然,首次需联网缓存,之后秒播"
                        font.pixelSize: 11
                        color: T.textMuted
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }

                    Repeater {
                        model: bridge.ttsVoices()
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            height: 40
                            radius: 12
                            color: voiceId === modelData.id ? T.blueSoft : T.track
                            border.color: voiceId === modelData.id ? T.blue : T.line
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                text: "●"
                                font.pixelSize: 12
                                color: voiceId === modelData.id ? T.blue : T.textMuted
                            }
                            Column {
                                anchors.left: parent.left
                                anchors.leftMargin: 34
                                anchors.verticalCenter: parent.verticalCenter
                                Text {
                                    text: modelData.label
                                    font.pixelSize: 13
                                    color: T.textDark
                                }
                            }
                            Text {
                                anchors.right: parent.right
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.id === "system"
                                      ? (bridge.systemTtsEngine() || "本机引擎")
                                      : modelData.desc
                                font.pixelSize: 10
                                color: T.textMuted
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    bridge.setTtsVoice(modelData.id)
                                    // 试听
                                    if (modelData.id === "system")
                                        bridge.speak("Hello, welcome.")
                                    else
                                        bridge.speak("Hello, this is a sample sentence.")
                                }
                            }
                        }
                    }

                    // 预下载当前词表发音(仅神经网络音有意义)
                    Rectangle {
                        Layout.fillWidth: true
                        height: 44
                        radius: 12
                        visible: voiceId !== "system"
                        color: preloadDone >= 0 ? T.track : T.greenSoft
                        border.color: T.greenBorder
                        Text {
                            anchors.centerIn: parent
                            text: preloadDone >= 0
                                  ? "正在下载发音 " + preloadDone + "/" + preloadTotal + "…"
                                  : "⬇️  预下载当前词表发音(下载后可离线)"
                            font.pixelSize: 12
                            color: T.greenDark
                        }
                        MouseArea {
                            anchors.fill: parent
                            enabled: preloadDone < 0
                            onClicked: bridge.preloadCurrentListTts()
                        }
                    }

                    // 预估占用空间(空闲时显示)
                    Text {
                        Layout.fillWidth: true
                        visible: voiceId !== "system" && preloadDone < 0
                                 && preloadEstimate !== ""
                        text: preloadEstimate === "已全部缓存"
                              ? "✓ 当前词表发音已全部缓存,可离线使用"
                              : "预计占用空间 " + preloadEstimate
                                + " · 含单词与例句发音"
                        font.pixelSize: 11
                        color: T.textMuted
                        horizontalAlignment: Text.AlignHCenter
                        topPadding: 2
                        bottomPadding: 4
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
                ChipBtn {
                    text: "🔁 重新查看新手引导"
                    Layout.fillWidth: true
                    onClicked: bridge.requestGuide()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 10
                ChipBtn {
                    text: "⬆️ 检查更新"
                    Layout.fillWidth: true
                    onClicked: {
                        showToast("正在检查更新…")
                        bridge.checkUpdate()
                    }
                }
                Text {
                    text: "当前 v" + bridge.appVersion()
                    font.pixelSize: 11
                    color: T.textMuted
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
        id: updatePopup
        property string latestVersion: ""
        property string releaseUrl: ""
        anchors.centerIn: parent
        width: parent.width * 0.86
        modal: true
        focus: true
        background: Rectangle { radius: 18; color: T.card }
        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: "发现新版本 🎉"
                font.pixelSize: 16
                font.bold: true
                color: T.textDark
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: "最新版本 v" + updatePopup.latestVersion
                      + ",当前 v" + bridge.appVersion()
                      + "。打开下载页获取新 APK,直接安装覆盖即可,数据保留。"
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
                    text: "下次再说"
                    Layout.fillWidth: true
                    onClicked: updatePopup.close()
                }
                ChipBtn {
                    text: "打开下载页"
                    Layout.fillWidth: true
                    onClicked: {
                        bridge.openUrl(updatePopup.releaseUrl)
                        updatePopup.close()
                    }
                }
            }
        }
    }

    Popup {
        id: donatePopup
        anchors.centerIn: parent
        width: parent.width * 0.9
        height: parent.height * 0.9
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
            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: qrCol.implicitHeight
                clip: true
                Column {
                    id: qrCol
                    width: parent.width
                    spacing: 14
                    Image {
                        width: Math.min(parent.width * 0.66, 250)
                        height: width * sourceSize.height / sourceSize.width
                        anchors.horizontalCenter: parent.horizontalCenter
                        fillMode: Image.PreserveAspectFit
                        source: "qrc:/assets/donate/qr1.jpg"
                    }
                    Image {
                        width: Math.min(parent.width * 0.66, 250)
                        height: width * sourceSize.height / sourceSize.width
                        anchors.horizontalCenter: parent.horizontalCenter
                        fillMode: Image.PreserveAspectFit
                        source: "qrc:/assets/donate/qr2.jpg"
                    }
                }
            }
            Text {
                text: "扫一扫二维码即可支持。感谢每一位使用者。"
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
                        body: "选一个词表,学习页就是卡片流;点击或拖动翻牌,左滑不认识,右滑认识,一次手势完成。"
                    }
                    GuideItem {
                        title: "学习与复习"
                        body: "学习=没学过的词;不认识的词自动进复习队列,随时点「开始复习」再学。没有每日上限,进度自动保存。"
                    }
                    GuideItem {
                        title: "词表"
                        body: "每个词表独立记录未学/待复习/已掌握;可以 AI 生成或补充领域词表,阅读和翻译中遇到的生词也会自动收集。"
                    }
                    GuideItem {
                        title: "阅读"
                        body: "红色=还没进词表的生词,绿色=当前词表,蓝色=其他词表,黑色=已掌握;点单词发音,可以加入阅读词表。"
                    }
                    GuideItem {
                        title: "翻译"
                        body: "输入英文翻译成中文,翻译中遇到的生词会自动收进「翻译生词」词表。"
                    }
                    GuideItem {
                        title: "生成文章"
                        body: "按当前词表自动生成一篇文章,用于在真实语境里复习;文章里的生词可以直接加入阅读词表。"
                    }
                    GuideItem {
                        title: "AI 设置"
                        body: "手机上的翻译、生成文章、对话都需要联网模型;填 DeepSeek/通义/GLM/Kimi/OpenAI 的 API Key 就能用,也可以连接电脑上的本地服务。"
                    }
                    GuideItem {
                        title: "数据"
                        body: "所有数据保存在手机本机;设置页可以重新导入内置词表,或把全部进度重置回未学状态。"
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
        function onConnectionTested(ok, message) {
            testing = false
            testOk = ok
            testMessage = message
            if (ok)
                showToast("✓ 连接成功")
        }
    }

    function initAi() {
        selectPreset(preset)
    }

    function selectPreset(v) {
        preset = v
        isCloudPreset = (v === "deepseek" || v === "dashscope"
                         || v === "glm" || v === "moonshot"
                         || v === "openai")
    }

    function applyPreset(v) {
        preset = v
        isCloudPreset = (v === "deepseek" || v === "dashscope"
                         || v === "glm" || v === "moonshot"
                         || v === "openai")
        bridge.setAiPreset(v)
        // 重新读取 bridge 已写入的地址/模型/服务类型
        url = bridge.aiUrl()
        model = bridge.aiModel()
        provider = bridge.aiProvider
        testMessage = ""
        testOk = false
        showToast("已选择" + (isCloudPreset ? currentProviderName : "Ollama"))
    }

    function showToast(msg) {
        toastText.text = msg
        toast.visible = true
        toastAnim.start()
        toastTimer.start()
    }

    function doPaste() {
        var t = bridge.clipboardText().trim()
        if (t.length > 0) {
            key = t
            showToast("已粘贴 Key")
        } else {
            showToast("剪贴板是空的,先去服务商页面复制 Key")
        }
    }

    function maskKey(k) {
        if (k.length <= 8)
            return k
        return k.slice(0, 5) + "••••••" + k.slice(-4)
    }

    function save() {
        bridge.setAiUrl(url)
        bridge.setAiModel(model)
        bridge.setAiProvider(provider)
        bridge.setAiApiKey(key)
        testMessage = ""
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
            color: T.track
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
        width: parent.width
        spacing: 3
        Text {
            text: title
            width: parent.width
            wrapMode: Text.Wrap
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
