import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "."

Page {
    property var articles: []
    property string html: ""
    property string lastSource: ""
    property bool translating: false
    property bool genBusy: false
    property bool showTranslate: false
    property string sourceHtml: ""
    // 朗读状态:reading=true 时高亮 readingIndex 段并逐段推进
    property bool reading: false
    property int readingIndex: -1
    property var paragraphs: []
    // 逐段翻译:translations[i] = 该段译文
    property var translations: []
    property var translateQueue: []
    property int translatingIdx: -1
    property bool translationsVisible: true

    background: Rectangle { color: T.bg }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "阅读"
                font.pixelSize: 20
                font.bold: true
                color: T.textDark
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                width: readBtn.implicitWidth + 24
                height: 30
                radius: 15
                color: T.greenSoft
                Text {
                    id: readBtn
                    anchors.centerIn: parent
                    text: reading ? "停止" : "朗读"
                    font.pixelSize: 12
                    font.bold: true
                    color: T.greenDark
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: readAloud()
                }
            }
            Rectangle {
                width: translateBtn.implicitWidth + 28
                height: 30
                radius: 15
                color: translating ? T.greenSoft : T.green
                Text {
                    id: translateBtn
                    anchors.centerIn: parent
                    text: translating ? "取消"
                        : (hasTranslations() && translationsVisible ? "收起译文"
                           : (hasTranslations() ? "显示译文" : "翻译全文"))
                    font.pixelSize: 12
                    font.bold: true
                    color: translating ? T.green : "#ffffff"
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: !translating && articleCombo.currentIndex >= 0
                    onClicked: translateAll()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            ChipBtn {
                text: genBusy ? "生成中…" : "生成文章"
                Layout.fillWidth: true
                enabled: !genBusy
                onClicked: genPopup.open()
            }
            ChipBtn {
                text: "对话练习"
                Layout.fillWidth: true
                onClicked: openChat()
            }
            ChipBtn {
                text: "导入网址"
                Layout.fillWidth: true
                onClicked: importPopup.open()
            }
            ChipBtn {
                text: "删除"
                Layout.fillWidth: true
                onClicked: deleteCurrent()
            }
        }

        ComboBox {
            id: articleCombo
            Layout.fillWidth: true
            model: articles
            textRole: "title"
            onActivated: loadArticle(articles[currentIndex].id)
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 18
            color: T.card
            border.color: T.line

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Flickable {
                    id: articleFlick
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: width
                    contentHeight: articleCol.height
                    property int playToken: 0

                    Column {
                        id: articleCol
                        width: parent.width
                        spacing: 15

                        Repeater {
                            id: paraRepeater
                            model: paragraphs
                            delegate: Item {
                                required property int index
                                required property string modelData
                                width: articleCol.width
                                implicitHeight: col.implicitHeight
                                // 当前朗读段的绿色高亮框(包住原文+译文)
                                Rectangle {
                                    id: paraHl
                                    anchors.fill: col
                                    anchors.leftMargin: -10
                                    anchors.rightMargin: -10
                                    anchors.topMargin: -6
                                    anchors.bottomMargin: -6
                                    radius: 12
                                    color: T.greenSoft
                                    border.color: T.green
                                    border.width: 1.5
                                    opacity: 0
                                    scale: 0.98
                                    states: State {
                                        name: "on"
                                        when: reading && readingIndex === index
                                        PropertyChanges { target: paraHl; opacity: 1; scale: 1 }
                                    }
                                    transitions: Transition {
                                        NumberAnimation {
                                            properties: "opacity,scale"
                                            duration: 280
                                            easing.type: Easing.OutCubic
                                        }
                                    }
                                }
                                Column {
                                    id: col
                                    width: parent.width
                                    spacing: 0
                                    Text {
                                        id: paraText
                                        width: parent.width
                                        text: modelData
                                        textFormat: Text.RichText
                                        wrapMode: Text.Wrap
                                        // 逐段播放:初始透明下移,依次淡入上浮
                                        opacity: 0
                                        y: 16
                                        Component.onCompleted: playOnce()
                                        function playOnce() {
                                            paraText.y = 16
                                            paraText.opacity = 0
                                            paraDelay.interval = index * 160
                                            paraDelay.start()
                                        }
                                        Connections {
                                            target: articleFlick
                                            function onPlayTokenChanged() {
                                                playOnce()
                                            }
                                        }
                                        Timer {
                                            id: paraDelay
                                            onTriggered: {
                                                paraAnim.start()
                                                paraAnim2.start()
                                            }
                                        }
                                        NumberAnimation {
                                            id: paraAnim
                                            target: paraText
                                            property: "opacity"
                                            to: 1
                                            duration: 420
                                            easing.type: Easing.OutCubic
                                        }
                                        NumberAnimation {
                                            id: paraAnim2
                                            target: paraText
                                            property: "y"
                                            to: 0
                                            duration: 420
                                            easing.type: Easing.OutCubic
                                        }
                                        TapHandler {
                                            acceptedButtons: Qt.LeftButton
                                            longPressThreshold: 600
                                            onTapped: function(eventPoint) {
                                                var link = paraText.linkAt(
                                                    eventPoint.position.x,
                                                    eventPoint.position.y)
                                                if (link !== "")
                                                    bridge.speak(
                                                        link.replace("word://", ""))
                                            }
                                            onLongPressed: function(eventPoint) {
                                                var link = paraText.linkAt(
                                                    eventPoint.position.x,
                                                    eventPoint.position.y)
                                                if (link !== "") {
                                                    popupWord.word =
                                                        link.replace("word://", "")
                                                    popupWord.open()
                                                }
                                            }
                                        }
                                    }
                                    // 该段译文:逐段翻译后显示在原文下方
                                    Column {
                                        width: parent.width
                                        spacing: 0
                                        visible: translationsVisible
                                        Item {
                                            width: parent.width
                                            height: transTxt.text !== ""
                                                    || translatingIdx === index
                                                    ? 8 : 0
                                        }
                                        Rectangle {
                                            id: transBox
                                            width: parent.width
                                            height: transTxt.implicitHeight + 18
                                            radius: 8
                                            color: T.track
                                            visible: transTxt.text !== ""
                                            Text {
                                                id: transTxt
                                                anchors.fill: parent
                                                anchors.margins: 9
                                                width: parent.width - 18
                                                wrapMode: Text.Wrap
                                                font.pixelSize: 14
                                                color: T.textBody
                                                text: (translations && index >= 0
                                                        && index < translations.length)
                                                      ? translations[index] : ""
                                                opacity: 0
                                                onTextChanged: if (text) {
                                                    transTxt.opacity = 0
                                                    transFade.start()
                                                }
                                                NumberAnimation on opacity {
                                                    id: transFade
                                                    from: 0; to: 1; duration: 300
                                                }
                                            }
                                        }
                                        Row {
                                            spacing: 6
                                            visible: translatingIdx === index
                                            Item { width: 1; height: 8 }
                                            Text {
                                                text: "翻译中…"
                                                font.pixelSize: 12
                                                color: T.textMuted
                                                anchors.verticalCenter:
                                                    parent.verticalCenter
                                            }
                                            Rectangle {
                                                width: 6; height: 6; radius: 3
                                                color: T.green
                                                anchors.verticalCenter:
                                                    parent.verticalCenter
                                                SequentialAnimation on opacity {
                                                    running: translatingIdx === index
                                                    loops: Animation.Infinite
                                                    NumberAnimation { to: 0.2; duration: 500 }
                                                    NumberAnimation { to: 1; duration: 500 }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Row {
                    Layout.fillWidth: true
                    spacing: 9
                    LegendDot { dotColor: T.green; label: "当前词表" }
                    LegendDot { dotColor: T.blue; label: "其他词表" }
                    LegendDot { dotColor: T.red; label: "未入词表" }
                    LegendDot { dotColor: T.textMuted; label: "已掌握" }
                }

                Rectangle {
                    visible: showTranslate
                    Layout.fillWidth: true
                    height: 1
                    color: T.line
                }

                ColumnLayout {
                    id: translatePanel
                    visible: showTranslate
                    Layout.fillWidth: true
                    Layout.preferredHeight: 300
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: translating ? "翻译中…" : "翻译结果"
                            font.pixelSize: 13
                            font.bold: true
                            color: T.greenDark
                        }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            width: 48
                            height: 26
                            radius: 13
                            color: T.greenSoft
                            Text {
                                anchors.centerIn: parent
                                text: "关闭"
                                font.pixelSize: 11
                                color: T.greenDark
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: closeTranslate()
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 3
                        Text {
                            text: "原文"
                            font.pixelSize: 11
                            font.bold: true
                            color: T.textMuted
                        }
                        Flickable {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            contentWidth: width
                            contentHeight: sourceText.height
                            Text {
                                id: sourceText
                                width: parent.width
                                text: sourceHtml
                                textFormat: Text.RichText
                                wrapMode: Text.Wrap
                                color: T.textBody
                                TapHandler {
                                    acceptedButtons: Qt.LeftButton
                                    longPressThreshold: 600
                                    onTapped: function(eventPoint) {
                                        var link = sourceText.linkAt(
                                            eventPoint.position.x,
                                            eventPoint.position.y)
                                        if (link !== "") {
                                            bridge.speak(
                                                link.replace("word://", ""))
                                        }
                                    }
                                    onLongPressed: function(eventPoint) {
                                        var link = sourceText.linkAt(
                                            eventPoint.position.x,
                                            eventPoint.position.y)
                                        if (link !== "") {
                                            popupWord.word =
                                                link.replace("word://", "")
                                            popupWord.open()
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: T.line
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 3
                        Text {
                            text: "译文"
                            font.pixelSize: 11
                            font.bold: true
                            color: T.textMuted
                        }
                        Flickable {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            contentWidth: width
                            contentHeight: translateText.height
                            Text {
                                id: translateText
                                width: parent.width
                                text: ""
                                wrapMode: Text.Wrap
                                font.pixelSize: 12
                                color: T.textBody
                            }
                        }
                    }
                }
            }
        }

    }

    Popup {
        id: chatPopup
        x: 8
        y: 8
        width: parent.width - 16
        height: parent.height - 16
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        background: Rectangle {
            radius: 20
            color: T.deskDark
            border.color: T.deskBorder
        }
        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "对话练习"
                    font.pixelSize: 17
                    font.bold: true
                    color: T.deskText
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: chatStatus
                    font.pixelSize: 10
                    color: chatBusy ? T.deskAccent : "#7b93c2"
                }
                Rectangle {
                    width: 52
                    height: 30
                    radius: 15
                    color: "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "关闭"
                        font.pixelSize: 12
                        color: T.deskText
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: chatPopup.close()
                    }
                }
            }

            ListView {
                id: chatView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 8
                model: chatModel
                delegate: Item {
                    width: chatView.width
                    height: bubble.implicitHeight + 10
                    Rectangle {
                        id: bubble
                        anchors.right: from === "me" ? parent.right : undefined
                        anchors.left: from === "me" ? undefined : parent.left
                        width: Math.min(chatView.width * 0.82,
                                        body.implicitWidth + 24)
                        implicitHeight: body.implicitHeight + 16
                        radius: 14
                        color: from === "me" ? T.green : "#26395e"
                        Text {
                            id: body
                            anchors.fill: parent
                            anchors.margins: 10
                            text: model.text
                            wrapMode: Text.Wrap
                            font.pixelSize: 13
                            color: from === "me" ? "#ffffff" : T.deskText
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Rectangle {
                    Layout.fillWidth: true
                    height: 42
                    radius: 21
                    color: "#1c2b49"
                    border.color: T.deskBorder
                    TextField {
                        id: chatInput
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 8
                        background: Rectangle { color: "transparent" }
                        placeholderText: "用英文回答…"
                        placeholderTextColor: "#5f7398"
                        font.pixelSize: 14
                        color: T.deskText
                        onAccepted: sendChat()
                    }
                }
                Rectangle {
                    width: 68
                    height: 42
                    radius: 21
                    color: chatBusy ? "#2c3b58" : T.green
                    Text {
                        anchors.centerIn: parent
                        text: "发送"
                        font.pixelSize: 14
                        font.bold: true
                        color: "#ffffff"
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: !chatBusy
                        onClicked: sendChat()
                    }
                }
            }
        }
    }

    Popup {
        id: importPopup
        anchors.centerIn: parent
        width: parent.width * 0.88
        modal: true
        focus: true
        background: Rectangle { radius: 18; color: T.card }
        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: "导入文章"
                font.pixelSize: 16
                font.bold: true
                color: T.textDark
                Layout.alignment: Qt.AlignHCenter
            }
            TextField {
                id: urlField
                Layout.fillWidth: true
                placeholderText: "https://example.com/article"
                font.pixelSize: 14
                color: T.textDark
                background: Rectangle {
                    radius: 10
                    color: T.track
                    border.color: T.line
                }
                onAccepted: startImport()
            }
            GenBtn {
                text: "选择本地文件…"
                Layout.fillWidth: true
                onClicked: fileDialog.open()
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                GenBtn {
                    text: "取消"
                    Layout.fillWidth: true
                    onClicked: importPopup.close()
                }
                GenBtn {
                    text: "导入"
                    Layout.fillWidth: true
                    onClicked: startImport()
                }
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: "选择文章文件"
        nameFilters: ["文本文件 (*.txt *.md *.html)", "所有文件 (*)"]
        onAccepted: {
            bridge.importArticleFile(
                selectedFile.toString().replace("file://", ""))
            importPopup.close()
        }
    }

    Popup {
        id: popupWord
        property string word: ""
        anchors.centerIn: parent
        width: parent.width * 0.84
        modal: true
        focus: true
        background: Rectangle {
            radius: 18
            color: T.deskDark
            border.color: T.deskBorder
        }
        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: popupWord.word
                font.pixelSize: 21
                font.bold: true
                color: T.deskText
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                Layout.fillWidth: true
                text: {
                    var i = bridge.wordInfo(popupWord.word)
                    var s = ""
                    if (i.phonetic)
                        s += i.phonetic + "  "
                    if (i.pos)
                        s += i.pos + "  "
                    s += i.meaning
                    return s
                }
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 12
                color: "#9fd9b4"
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                PopBtn {
                    text: "发音"
                    Layout.fillWidth: true
                    onClicked: {
                        bridge.speak(popupWord.word)
                        popupWord.close()
                    }
                }
                PopBtn {
                    text: "翻译"
                    Layout.fillWidth: true
                    onClicked: {
                        translateWord(popupWord.word)
                        popupWord.close()
                    }
                }
            }
            PopBtn {
                text: "加入阅读词表"
                Layout.fillWidth: true
                onClicked: {
                    bridge.addReadingWord(popupWord.word)
                    showToast("已加入阅读词表")
                    popupWord.close()
                }
            }
            PopBtn {
                text: "翻译本句"
                Layout.fillWidth: true
                onClicked: {
                    var a = articles[articleCombo.currentIndex]
                    if (!a) {
                        showToast("没有当前文章")
                        return
                    }
                    var s = bridge.sentenceForArticle(a.id,
                                                       popupWord.word)
                    if (s === "") {
                        showToast("没找到所在句子")
                        return
                    }
                    translateWord(s)
                    popupWord.close()
                }
            }
        }
    }

    Popup {
        id: genPopup
        anchors.centerIn: parent
        width: parent.width * 0.88
        modal: true
        focus: true
        background: Rectangle { radius: 18; color: T.card }
        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: "生成文章"
                font.pixelSize: 16
                font.bold: true
                color: T.textDark
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: "按当前词表「" + bridge.currentListName + "」生成"
                font.pixelSize: 12
                color: T.textMuted
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            TextField {
                id: topicField
                Layout.fillWidth: true
                placeholderText: "主题(留空=按当前词表)"
                font.pixelSize: 14
                color: T.textDark
                background: Rectangle {
                    radius: 10
                    color: T.track
                    border.color: T.line
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "难度"
                    font.pixelSize: 12
                    color: T.textMuted
                }
                ComboBox {
                    id: genLevelCombo
                    Layout.fillWidth: true
                    model: ["简单", "中等", "较难"]
                    currentIndex: 1
                }
            }
            Text {
                text: genBusy ? "生成中,约 1~3 分钟…" : ""
                font.pixelSize: 11
                color: T.green
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                GenBtn {
                    text: "取消"
                    Layout.fillWidth: true
                    enabled: !genBusy
                    onClicked: genPopup.close()
                }
                GenBtn {
                    text: "开始生成"
                    Layout.fillWidth: true
                    enabled: !genBusy
                    onClicked: startArticleAi()
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
        z: 60
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

    Connections {
        target: bridge
        function onTranslationReady(t) {
            if (translating && translatingIdx >= 0) {
                onParaTranslated(t)
                return
            }
            translating = false
        }
        function onTranslationFailed(m) {
            if (translating && translatingIdx >= 0) {
                onParaFailed(m)
                return
            }
        }
        function onArticleReady(id, title) {
            genBusy = false
            load(id)
            showToast("文章已生成:" + title)
        }
        function onAiFailed(message) {
            genBusy = false
            showToast("生成失败:" + message)
        }
        function onChatReady(text) {
            chatBusy = false
            chatStatus = "已回复"
            chatModel.append({ from: "ai", text: text })
            chatView.positionViewAtEnd()
        }
        function onChatFailed(message) {
            chatBusy = false
            chatStatus = "失败"
            chatModel.append({ from: "ai", text: message })
            chatView.positionViewAtEnd()
        }
        function onArticleImported(id, title) {
            load(id)
            showToast("已导入:" + title)
            importPopup.close()
        }
    }

    ListModel { id: chatModel }
    property bool chatBusy: false
    property string chatStatus: ""

    function closeTranslate() {
        showTranslate = false
        translating = false
    }

    function showToast(msg) {
        toastText.text = msg
        toast.visible = true
        toastAnim.start()
        toastTimer.start()
    }

    function openChat() {
        var a = articles[articleCombo.currentIndex]
        if (!a) {
            showToast("请先选择一篇文章")
            return
        }
        chatModel.clear()
        chatStatus = "准备第一个问题…"
        chatBusy = true
        bridge.chatOpen(a.title, bridge.articleContent(a.id))
        chatPopup.open()
    }

    function sendChat() {
        var t = chatInput.text.trim()
        if (t === "" || chatBusy)
            return
        chatModel.append({ from: "me", text: t })
        chatInput.text = ""
        chatBusy = true
        chatStatus = "思考中…"
        bridge.chatSend(t)
        chatView.positionViewAtEnd()
    }

    function startImport() {
        var u = urlField.text.trim()
        if (u === "") {
            showToast("请输入网址")
            return
        }
        bridge.importUrl(u)
    }

    function deleteCurrent() {
        var a = articles[articleCombo.currentIndex]
        if (!a) {
            showToast("没有可删除的文章")
            return
        }
        bridge.deleteArticle(a.id)
        load()
        showToast("已删除文章")
    }

    function translateAll() {
        if (translating) {
            // 正在逐段翻译时再点=停止(已译出的保留)
            translating = false
            translatingIdx = -1
            translateQueue = []
            return
        }
        // 已有译文且正显示 -> 收起,恢复纯英文
        if (hasTranslations() && translationsVisible) {
            translationsVisible = false
            return
        }
        // 译文被收起 -> 重新展开(不重新请求)
        if (hasTranslations() && !translationsVisible) {
            translationsVisible = true
            return
        }
        if (!paragraphs || paragraphs.length === 0) {
            showToast("请先选择一篇文章")
            return
        }
        // 每段去标签取纯文本,建翻译队列
        translations = new Array(paragraphs.length).fill("")
        translationsVisible = true
        var q = []
        for (var i = 0; i < paragraphs.length; i++)
            q.push(i)
        translateQueue = q
        translating = true
        translateNextParagraph()
    }

    function hasTranslations() {
        if (!translations) return false
        for (var i = 0; i < translations.length; i++)
            if (translations[i] && translations[i] !== "") return true
        return false
    }

    function plainText(htmlPara) {
        return htmlPara.replace(/<[^>]*>/g, "")
                .replace(/&amp;/g, "&").replace(/&lt;/g, "<")
                .replace(/&gt;/g, ">").replace(/&quot;/g, '"')
                .replace(/&#39;/g, "'")
    }

    function translateNextParagraph() {
        if (!translating || translateQueue.length === 0) {
            translating = false
            translatingIdx = -1
            return
        }
        var idx = translateQueue.shift()
        translatingIdx = idx
        bridge.translate(plainText(paragraphs[idx]), "")
    }

    function onParaTranslated(text) {
        if (translatingIdx < 0) return
        var arr = translations.slice()
        arr[translatingIdx] = text
        translations = arr
        translatingIdx = -1
        // 继续下一段(留 80ms 喘息,避免连发被限流)
        paraGapTimer.start()
    }

    function onParaFailed(m) {
        if (translatingIdx >= 0) {
            var arr = translations.slice()
            arr[translatingIdx] = "（翻译失败:" + m + "）"
            translations = arr
        }
        translatingIdx = -1
        translateQueue = []
        translating = false
    }

    // 单词弹窗的查词:走底部翻译面板(非逐段模式)
    function translateWord(w) {
        translating = false
        translatingIdx = -1
        lastSource = w
        sourceHtml = "<div style='font-size:13px; line-height:1.6;'>"
                     + bridge.highlightText(w) + "</div>"
        showTranslate = true
        translateText.text = "翻译中…"
        bridge.translate(w, "")
    }

    function startArticleAi() {
        var t = topicField.text.trim()
        genBusy = true
        genPopup.close()
        bridge.aiGenerateArticle(t, 0, genLevelCombo.currentIndex + 1)
    }

    function loadArticle(id) {
        stopReadAloud()
        translating = false
        translatingIdx = -1
        translateQueue = []
        translations = []
        translationsVisible = true
        paragraphs = bridge.articleParagraphs(id)
        // 切换文章:model 绑定会自动重建段落,这里回到顶部并触发逐段动画
        articleFlick.playToken++
        articleFlick.contentY = 0
    }

    function readAloud() {
        if (reading) { stopReadAloud(); return }
        if (!paragraphs || paragraphs.length === 0) {
            showToast("请先选择一篇文章")
            return
        }
        reading = true
        readingIndex = 0
        speakCurrentParagraph()
    }

    function stopReadAloud() {
        if (!reading) return
        reading = false
        readingIndex = -1
        readAdvanceTimer.stop()
        bridge.stopSpeak()
    }

    function speakCurrentParagraph() {
        if (!reading || readingIndex < 0
                || readingIndex >= paragraphs.length) {
            stopReadAloud()
            return
        }
        // 滚到当前段,让绿色框可见
        var item = paraRepeater.itemAt(readingIndex)
        if (item) {
            var target = item.mapToItem(articleFlick.contentItem, 0, 0).y
            articleFlick.contentY = Math.max(0, target - 40)
        }
        // 去标签拿纯文本朗读
        var plain = paragraphs[readingIndex]
                      .replace(/<[^>]*>/g, "")
                      .replace(/&amp;/g, "&").replace(/&lt;/g, "<")
                      .replace(/&gt;/g, ">").replace(/&quot;/g, '"')
                      .replace(/&#39;/g, "'")
        bridge.speak(plain)
        // 按字数估算本段朗读时长(系统TTS约每秒 14 字符)
        var ms = Math.max(1500, plain.length * 75)
        readAdvanceTimer.interval = ms
        readAdvanceTimer.restart()
    }

    function advanceParagraph() {
        readingIndex++
        if (readingIndex >= paragraphs.length) {
            stopReadAloud()
            return
        }
        speakCurrentParagraph()
    }

    function load(selectId) {
        articles = bridge.articles()
        articleCombo.currentIndex = articles.length > 0 ? 0 : -1
        if (selectId !== undefined) {
            for (var i = 0; i < articles.length; ++i) {
                if (articles[i].id === selectId) {
                    articleCombo.currentIndex = i
                    break
                }
            }
        }
        if (articles.length > 0)
            loadArticle(articles[articleCombo.currentIndex].id)
    }

    Component.onCompleted: load()

    // 朗读推进:一段读完后切到下一段
    Timer {
        id: readAdvanceTimer
        repeat: false
        onTriggered: advanceParagraph()
    }

    // 逐段翻译间隔
    Timer {
        id: paraGapTimer
        interval: 120
        repeat: false
        onTriggered: translateNextParagraph()
    }

    component LegendDot: Row {
        id: root
        property color dotColor: T.green
        property string label: ""
        spacing: 4
        Rectangle {
            width: 9
            height: 9
            radius: 5
            color: root.dotColor
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: root.label
            font.pixelSize: 9
            color: T.textMuted
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    component PopBtn: Rectangle {
        id: root
        property string text: ""
        signal clicked()
        height: 38
        radius: 19
        color: T.green
        Text {
            anchors.centerIn: parent
            text: root.text
            font.pixelSize: 13
            font.bold: true
            color: "#ffffff"
        }
        MouseArea {
            anchors.fill: parent
            onClicked: root.clicked()
        }
    }

    component GenBtn: Rectangle {
        id: root
        property string text: ""
        property bool enabled: true
        signal clicked()
        height: 38
        radius: 19
        color: root.enabled ? T.green : "#cfd8cf"
        opacity: root.enabled ? 1 : 0.7
        Text {
            anchors.centerIn: parent
            text: root.text
            font.pixelSize: 13
            font.bold: true
            color: "#ffffff"
        }
        MouseArea {
            anchors.fill: parent
            enabled: root.enabled
            onClicked: root.clicked()
        }
    }

    component ChipBtn: Rectangle {
        id: root
        property string text: ""
        property bool enabled: true
        signal clicked()
        height: 32
        radius: 16
        color: T.greenSoft
        border.color: T.greenBorder
        border.width: 1
        opacity: root.enabled ? 1 : 0.5
        Text {
            anchors.centerIn: parent
            text: root.text
            font.pixelSize: 12
            font.bold: true
            color: T.greenDark
        }
        MouseArea {
            anchors.fill: parent
            enabled: root.enabled
            onClicked: root.clicked()
        }
    }
}
