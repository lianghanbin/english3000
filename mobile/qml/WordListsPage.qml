import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Page {
    property var lists: []
    ListModel { id: rows }
    property int currentId: -1
    property string currentName: ""
    property bool aiBusy: false
    property int pageOffset: 0
    property int pageSize: 60
    property bool hasMore: false

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
                                if (modelData.current)
                                    return
                                bridge.setCurrentList(modelData.id)
                                showToast("已切换到:「" + modelData.name + "」")
                            }
                            onPressAndHold: {
                                delList.id = modelData.id
                                delList.name = modelData.name
                                delList.open()
                            }
                        }
                    }
                }

                SideBtn {
                    text: "AI 生成词表"
                    onClicked: {
                        domainField.text = ""
                        aiPopup.open()
                    }
                }
                SideBtn {
                    text: "AI 补充词表"
                    onClicked: supplementDirect()
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
                    text: "未学 " + bridge.newCount + " / 已掌握 "
                          + bridge.masteredCount
                    font.pixelSize: 11
                    color: T.textMuted
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 30
                radius: 8
                color: T.track
                Row {
                    anchors.fill: parent
                    HeadCell { text: "单词"; w: 0.28 }
                    HeadCell { text: "释义"; w: 0.5 }
                    HeadCell { text: "状态"; w: 0.22 }
                }
            }

            ListView {
                id: rowsView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: rows
                delegate: RowItem {}
                cacheBuffer: 400
                boundsBehavior: Flickable.StopAtBounds
                onAtYEndChanged: {
                    if (atYEnd && hasMore)
                        loadMore()
                }
            }

            // 流式追加后下一帧直接跟随到底(不做滚动动画,
            // 否则词快速到来时动画反复 restart 会抖动)
            Timer {
                id: scrollTimer
                interval: 16
                onTriggered: rowsView.positionViewAtEnd()
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
            onTriggered: {
                toastAnim.stop()
                toast.opacity = 0
                toast.visible = false
            }
        }
    }

    Connections {
        target: bridge
        function onCurrentListChanged() {
            // 切换当前词表:刷新左侧选中态 + 右侧词条
            refreshRows()
        }
        function onListChanged() {
            // 流式生成中列表由 onWordAppended 实时维护,不要 refresh 清空
            if (aiBusy) {
                lists = bridge.wordLists()
                syncCurrent()
                return
            }
            refresh()
        }
        function onWordAppended(listId, word, pos, meaning) {
            // 流式:每来一个词就 append 一行(ListModel 只新增,
            // 不重建已有行,所以不会整屏抖动)
            if (listId !== currentId) {
                lists = bridge.wordLists()
                syncCurrent()
                rows.clear()
                pageOffset = 0
                hasMore = false
            }
            rows.append({
                id: -1, word: word, pos: pos,
                meaning: meaning, status: "new", isNew: true
            })
            // 等 delegate 创建后再滚到底,保证新词始终可见
            scrollTimer.restart()
        }
        function onWordListReady(name, count) {
            aiBusy = false
            lists = bridge.wordLists()
            syncCurrent()
            showToast("词表已生成:「" + name + "」共 " + count + " 词")
        }
        function onAiFailed(message) {
            aiBusy = false
            // 用户主动取消不弹错误
            if (message && message.indexOf("已取消") < 0)
                showToast("AI 失败:" + message)
        }
        function onTranslationReady(t) {
            wordAction.resultText = t
        }
        function onTranslationFailed(m) {
            wordAction.resultText = "翻译失败:" + m
        }
    }

    Popup {
        id: wordAction
        property string word: ""
        property int itemId: -1
        property string resultText: ""
        anchors.centerIn: parent
        width: parent.width * 0.86
        modal: true
        focus: true
        background: Rectangle { radius: 18; color: T.card }
        contentItem: ColumnLayout {
            spacing: 10
            Text {
                text: wordAction.word
                font.pixelSize: 19
                font.bold: true
                color: T.textDark
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                Layout.fillWidth: true
                text: {
                    var i = bridge.wordInfo(wordAction.word)
                    var s = ""
                    if (i.phonetic)
                        s += i.phonetic + "  "
                    if (i.pos)
                        s += i.pos + "  "
                    s += i.meaning === "" ? "（暂无释义）" : i.meaning
                    return s
                }
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 12
                color: T.textMuted
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                AiBtn {
                    text: "发音"
                    Layout.fillWidth: true
                    onClicked: {
                        bridge.speak(wordAction.word)
                        wordAction.close()
                    }
                }
                AiBtn {
                    text: "翻译"
                    Layout.fillWidth: true
                    onClicked: {
                        wordAction.resultText = "翻译中…"
                        bridge.translate(wordAction.word,
                                         bridge.aiModel())
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                AiBtn {
                    text: "重置为未学"
                    Layout.fillWidth: true
                    onClicked: {
                        if (wordAction.itemId > 0) {
                            bridge.resetListItem(wordAction.itemId)
                            refresh()
                        }
                        showToast("已重置为未学")
                        wordAction.close()
                    }
                }
            }
            AiBtn {
                text: "加入阅读词表"
                Layout.fillWidth: true
                onClicked: {
                    bridge.addToReadingList(wordAction.word)
                    showToast("已加入阅读词表")
                    wordAction.close()
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                radius: 10
                color: T.greenSoft
                visible: wordAction.resultText !== ""
                Text {
                    anchors.fill: parent
                    anchors.margins: 8
                    text: wordAction.resultText
                    wrapMode: Text.Wrap
                    font.pixelSize: 12
                    color: T.textBody
                }
            }
            AiBtn {
                text: "关闭"
                Layout.fillWidth: true
                onClicked: wordAction.close()
            }
        }
    }

    function showToast(msg) {
        toastText.text = msg
        toast.visible = true
        toastAnim.start()
        toastTimer.start()
    }

    function refresh() {
        lists = bridge.wordLists()
        syncCurrent()
        pageOffset = 0
        rows.clear()
        loadMore()
    }

    // 切换当前词表或进度变化时调用:
    // 重新取词表列表(整体替换 lists 以触发左侧选中高亮重绘),
    // 并刷新右侧当前词表的词条。词表列表本身数据量很小,开销可忽略。
    function refreshRows() {
        lists = bridge.wordLists()
        syncCurrent()
        pageOffset = 0
        rows.clear()
        loadMore()
    }

    function syncCurrent() {
        currentId = -1
        currentName = ""
        for (var i = 0; i < lists.length; ++i) {
            if (lists[i].current) {
                currentId = lists[i].id
                currentName = lists[i].name
                break
            }
        }
    }

    function loadMore() {
        if (currentId < 0)
            return
        var more = bridge.wordListPageRows(currentId, pageOffset, pageSize)
        for (var i = 0; i < more.length; i++) {
            rows.append({
                id: more[i].id,
                word: more[i].word,
                pos: more[i].pos,
                meaning: more[i].meaning,
                status: more[i].status,
                isNew: false
            })
        }
        pageOffset += more.length
        hasMore = more.length === pageSize
    }

    function startAi() {
        var d = domainField.text.trim()
        if (d === "") {
            showToast("请先输入领域")
            return
        }
        aiBusy = true
        aiPopup.close()
        bridge.aiGenerateWordList(d, 200)
    }

    // 补充词表:直接用当前词表主题补充,不弹窗
    function supplementDirect() {
        if (currentId < 0) {
            showToast("请先选择一个词表")
            return
        }
        // 把已有词全部加载进来,这样补充的新词追加到末尾时
        // 能进入可见区域、触发淡入动画并自动滚到底
        while (hasMore)
            loadMore()
        aiBusy = true
        // 等批量加载的行布局完成后立刻滑到底,让用户在新词位置等待
        scrollTimer.restart()
        bridge.aiSupplementWordList("", 100)
    }

    function stopAi() {
        aiBusy = false
        bridge.cancelAi()
    }

    Popup {
        id: aiPopup
        anchors.centerIn: parent
        width: parent.width * 0.88
        modal: true
        focus: true
        background: Rectangle { radius: 18; color: T.card }
        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: "AI 生成领域词表"
                font.pixelSize: 16
                font.bold: true
                color: T.textDark
                Layout.alignment: Qt.AlignHCenter
            }
            TextField {
                id: domainField
                Layout.fillWidth: true
                placeholderText: "领域,如:编程、医学、日常口语"
                font.pixelSize: 14
                color: T.textDark
                background: Rectangle {
                    radius: 10
                    color: T.track
                    border.color: T.line
                }
            }
            Text {
                text: "AI 将生成约 200 个相关单词。开始后可随时点"
                      + "「停止」中断。"
                font.pixelSize: 11
                color: T.textMuted
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                AiBtn {
                    text: "取消"
                    Layout.fillWidth: true
                    enabled: !aiBusy
                    onClicked: aiPopup.close()
                }
                AiBtn {
                    text: "开始生成"
                    Layout.fillWidth: true
                    enabled: !aiBusy
                    onClicked: startAi()
                }
            }
        }
    }

    Popup {
        id: delList
        property int id: -1
        property string name: ""
        anchors.centerIn: parent
        width: parent.width * 0.86
        modal: true
        focus: true
        background: Rectangle { radius: 18; color: T.card }
        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: "删除词表?"
                font.pixelSize: 16
                font.bold: true
                color: T.textDark
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: "确定删除「" + delList.name
                      + "」?词表内学习记录将一并删除。"
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
                font.pixelSize: 12
                color: T.textBody
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                AiBtn {
                    text: "取消"
                    Layout.fillWidth: true
                    onClicked: delList.close()
                }
                AiBtn {
                    text: "删除"
                    Layout.fillWidth: true
                    onClicked: {
                        bridge.deleteWordList(delList.id)
                        delList.close()
                        refresh()
                        showToast("已删除词表")
                    }
                }
            }
        }
    }

    Component.onCompleted: refresh()

    component AiBtn: Rectangle {
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
        width: rowsView.width
        height: 42
        color: T.card
        border.color: T.line
        border.width: 0
        // 底部细分隔线,不做斑马纹
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: T.line
            opacity: 0.6
        }
        // 新词只做一次轻微淡入,不缩放、不闪色、不跳动
        readonly property bool popping:
            (typeof isNew !== "undefined") && isNew
        opacity: popping ? 0 : 1
        Component.onCompleted: {
            if (popping)
                fadeIn.start()
        }
        NumberAnimation {
            id: fadeIn
            target: row
            property: "opacity"
            from: 0; to: 1
            duration: 280
            easing.type: Easing.OutCubic
        }

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
                        text: word
                        font.pixelSize: 13
                        font.bold: true
                        color: T.greenDark
                    }
                }
            }
            Item {
                width: parent.width * 0.5
                height: parent.height
                Text {
                    id: meaningText
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    width: parent.width - 8
                    elide: Text.ElideRight
                    text: meaning === ""
                          ? "（暂无释义，点按查词典）" : meaning
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
                    color: chipBg(status)
                    Text {
                        id: chipText
                        anchors.centerIn: parent
                        text: chipLabel(status)
                        font.pixelSize: 10
                        font.bold: true
                        color: chipFg(status)
                    }
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                bridge.speak(word)
                clickPop.start()
            }
            onPressAndHold: {
                wordAction.word = word
                wordAction.itemId = id
                wordAction.resultText = ""
                wordAction.open()
            }
        }
        SequentialAnimation {
            id: clickPop
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
