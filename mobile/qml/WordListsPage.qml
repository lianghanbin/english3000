import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Page {
    property var lists: []
    property var rows: []
    property int currentId: -1
    property string currentName: ""
    property int currentCount: 0
    property int rowsLoaded: 0
    property bool aiBusy: false

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
                    text: "✦ AI 生成词表"
                    onClicked: {
                        aiPopup.mode = "new"
                        domainField.text = ""
                        aiPopup.open()
                    }
                }
                SideBtn {
                    text: "＋ AI 补充词表"
                    onClicked: {
                        aiPopup.mode = "supplement"
                        domainField.text = ""
                        aiPopup.open()
                    }
                }
                SideBtn {
                    text: "✎ AI 补全释义"
                    onClicked: {
                        aiBusy = true
                        bridge.aiFillMissingMeanings()
                    }
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
                        Item {
                            width: parent.width
                            height: 46
                            visible: currentId > 0
                            Rectangle {
                                anchors.centerIn: parent
                                width: parent.width - 24
                                height: 34
                                radius: 17
                                color: rowsLoaded >= currentCount
                                       ? "#eef4ee" : T.greenSoft
                                border.color: T.greenBorder
                                border.width: 1
                                Text {
                                    anchors.centerIn: parent
                                    text: rowsLoaded >= currentCount
                                          ? "已显示全部 " + currentCount + " 词"
                                          : "已显示 " + rows.length + " / "
                                            + currentCount + " · 点此加载更多"
                                    font.pixelSize: 12
                                    font.bold: true
                                    color: T.greenDark
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    enabled: rowsLoaded < currentCount
                                    onClicked: loadMore()
                                }
                            }
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
        function onCountsChanged() { refresh() }
        function onWordListReady(name, count) {
            aiBusy = false
            refresh()
            showToast("词表已生成:「" + name + "」共 " + count + " 词")
        }
        function onMeaningsFilled(count) {
            aiBusy = false
            refresh()
            showToast(count > 0 ? "已补全 " + count + " 个释义"
                                : "没有需要补全的释义")
        }
        function onAiFailed(message) {
            aiBusy = false
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
        var prevId = currentId
        var prevLoaded = rowsLoaded
        lists = bridge.wordLists()
        currentId = -1
        currentCount = 0
        for (var i = 0; i < lists.length; ++i) {
            if (lists[i].current) {
                currentId = lists[i].id
                currentName = lists[i].name
                currentCount = lists[i].wordCount
                break
            }
        }
        if (prevId === currentId && prevLoaded > 40)
            rowsLoaded = prevLoaded
        else
            rowsLoaded = 40
        rows = currentId >= 0 ? bridge.wordListRows(currentId, rowsLoaded) : []
    }

    function loadMore() {
        if (currentId <= 0 || rowsLoaded >= currentCount)
            return
        rowsLoaded = Math.min(rowsLoaded + 100, currentCount)
        rows = bridge.wordListRows(currentId, rowsLoaded)
    }

    function startAi() {
        var d = domainField.text.trim()
        if (d === "") {
            showToast("请先输入领域")
            return
        }
        aiBusy = true
        aiPopup.close()
        if (aiPopup.mode === "supplement")
            bridge.aiSupplementWordList(d, countSpin.value)
        else
            bridge.aiGenerateWordList(d, countSpin.value)
    }

    Popup {
        id: aiPopup
        property string mode: "new"
        anchors.centerIn: parent
        width: parent.width * 0.88
        modal: true
        focus: true
        background: Rectangle { radius: 18; color: T.card }
        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: aiPopup.mode === "supplement"
                      ? "AI 补充当前词表" : "AI 生成领域词表"
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
                    color: "#f7faf7"
                    border.color: T.line
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "词数"
                    font.pixelSize: 12
                    color: T.textMuted
                }
                SpinBox {
                    id: countSpin
                    from: 50
                    to: 500
                    stepSize: 50
                    value: 200
                    editable: true
                }
                Item { Layout.fillWidth: true }
            }
            Text {
                text: aiBusy ? "AI 生成中,请稍候…"
                             : "提示:本地模型生成较慢,请耐心等待"
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
                    text: modelData.meaning === ""
                          ? "（暂无释义，可点 AI 补全）" : modelData.meaning
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
            }
            onPressAndHold: {
                wordAction.word = modelData.word
                wordAction.itemId = modelData.id
                wordAction.resultText = ""
                wordAction.open()
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
