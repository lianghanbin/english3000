import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Page {
    property var cards: []
    property int cardIndex: -1
    property bool revealed: false
    property string currentExample: ""
    property string mode: "learn"
    property bool speaking: false
    property bool pendingKnown: false
    property bool needsReload: false
    property int total: bridge.newCount + bridge.dueCount + bridge.masteredCount

    background: Rectangle { color: T.bg }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Row {
                Layout.alignment: Qt.AlignVCenter
                spacing: 8
                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    color: T.green
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: bridge.currentListName === ""
                          ? "核心 3000" : bridge.currentListName
                    font.pixelSize: 22
                    font.bold: true
                    color: T.textDark
                }
            }
            Item { Layout.fillWidth: true }
                Text {
                    text: "未学 " + bridge.newCount + " / 待复习 "
                          + bridge.dueCount + " / 已掌握 " + bridge.masteredCount
                font.pixelSize: 12
                color: T.textMuted
                Layout.alignment: Qt.AlignVCenter
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            ModeBtn {
                text: "开始复习"
                Layout.fillWidth: true
                onClicked: load("review")
            }
            ModeBtn {
                text: "开始学习"
                Layout.fillWidth: true
                onClicked: load("learn")
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 20
            radius: 10
            color: T.track
            clip: true
            Rectangle {
                id: progressBar
                width: total === 0 ? 0 : parent.width
                                      * (bridge.masteredCount / total)
                height: parent.height
                radius: 10
                gradient: Gradient {
                    GradientStop { position: 0.0; color: T.green }
                    GradientStop { position: 1.0; color: T.greenBright }
                }
                Behavior on width {
                    NumberAnimation { duration: 500; easing.type: Easing.OutCubic }
                }
            }
            Text {
                anchors.centerIn: parent
                text: "已掌握 " + (total === 0 ? 0
                    : Math.round(bridge.masteredCount / total * 100)) + "%"
                font.pixelSize: 11
                font.bold: true
                color: T.textDark
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Rectangle {
                id: cardShadow
                x: cardBody.x + 4
                y: cardBody.y + 8
                width: cardBody.width
                height: cardBody.height
                radius: 28
                color: "#24000000"
                opacity: cardBody.opacity
                rotation: cardBody.rotation
                scale: cardBody.scale
            }

            Rectangle {
                id: cardBody
                x: 2
                y: 2
                width: parent.width - 4
                height: parent.height - 4
                radius: 26
                color: T.card
                border.color: "#eaf0ea"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    anchors.topMargin: 14
                    anchors.bottomMargin: 14
                    spacing: 5

                    Text {
                        id: rankText
                        Layout.alignment: Qt.AlignHCenter
                        font.pixelSize: 12
                        color: T.textMuted
                    }
                    Item { Layout.fillHeight: true }
                    Text {
                        id: wordText
                        Layout.alignment: Qt.AlignHCenter
                        font.pixelSize: 42
                        font.bold: true
                        color: T.textDark
                    }
                    Text {
                        id: ipaText
                        Layout.alignment: Qt.AlignHCenter
                        font.pixelSize: 17
                        color: T.textMuted
                    }
                    Text {
                        id: posText
                        Layout.alignment: Qt.AlignHCenter
                        font.pixelSize: 13
                        color: T.textMuted
                    }
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: revealed ? 96 : 0
                        height: 4
                        radius: 2
                        color: T.green
                        Behavior on width {
                            NumberAnimation { duration: 350; easing.type: Easing.OutCubic }
                        }
                    }
                    Text {
                        id: meaningText
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignHCenter
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        font.pixelSize: 21
                        font.bold: true
                        color: T.textBody
                        opacity: revealed ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 350 } }
                    }
                    Rectangle {
                        id: exampleBox
                        Layout.fillWidth: true
                        Layout.preferredHeight: exampleText.implicitHeight + 16
                        radius: 12
                        color: T.greenSoft
                        visible: revealed && currentExample !== ""
                        opacity: revealed ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 450 } }

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 8
                            Text {
                                id: exampleText
                                text: currentExample
                                width: parent.width - 82
                                anchors.verticalCenter: parent.verticalCenter
                                wrapMode: Text.Wrap
                                font.pixelSize: 14
                                color: T.textBody
                            }
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 30
                                height: 24
                                radius: 12
                                color: T.green
                                Text {
                                    anchors.centerIn: parent
                                    text: "译"
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: "#ffffff"
                                }
                                MouseArea {
                                    id: transBtn
                                    anchors.fill: parent
                                    onClicked: doExampleTranslate()
                                }
                            }
                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 3
                                Repeater {
                                    model: 5
                                    Rectangle {
                                        width: 4
                                        radius: 2
                                        color: T.green
                                        anchors.verticalCenter: parent.verticalCenter
                                        height: 12
                                        SequentialAnimation on height {
                                            running: speaking && exampleBox.visible
                                            loops: Animation.Infinite
                                            NumberAnimation { to: 26; duration: 300 }
                                            NumberAnimation { to: 12; duration: 300 }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                    Item { Layout.fillHeight: true }
                }
            }

            MouseArea {
                id: cardMouse
                anchors.fill: parent
                drag.target: cardBody
                drag.axis: Drag.XAxis
                drag.threshold: 6
                drag.minimumX: -150
                drag.maximumX: 150
                onClicked: {
                    if (exampleBox.visible) {
                        var p = transBtn.mapFromItem(cardMouse, mouse.x,
                                                     mouse.y)
                        if (transBtn.contains(Qt.point(p.x, p.y))) {
                            doExampleTranslate()
                            return
                        }
                    }
                    reveal()
                }
                onPositionChanged: {
                    if (!drag.active)
                        return
                    var p = cardBody.x
                    if (!revealed && Math.abs(p) > 18)
                        reveal()
                    cardBody.rotation = p / 18
                    cardBody.scale = 1 - Math.min(Math.abs(p), 150) / 1500
                    cardBody.opacity = 1 - Math.min(Math.abs(p), 300) / 1200
                }
                onReleased: {
                    if (!revealed) {
                        backX.start()
                    } else if (cardBody.x <= -42) {
                        swipeAnswer(false)
                    } else if (cardBody.x >= 42) {
                        swipeAnswer(true)
                    } else {
                        backX.start()
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            ActionBtn {
                text: "不认识"
                colorA: T.redBright
                colorB: T.red
                enabled: revealed
                onClicked: answer(false)
            }
            ActionBtn {
                text: "显示释义"
                colorA: T.blueBright
                colorB: T.blue
                enabled: cardIndex >= 0 && !revealed
                onClicked: reveal()
            }
            ActionBtn {
                text: "认识"
                colorA: T.greenBright
                colorB: T.green
                enabled: revealed
                onClicked: answer(true)
            }
        }

            Text {
                text: "点击翻牌,左甩不认识,右甩认识"
            font.pixelSize: 12
            color: T.textMuted
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
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

    Popup {
        id: exampleTransPopup
        anchors.centerIn: parent
        width: parent.width * 0.9
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { radius: 18; color: T.card }
        contentItem: ColumnLayout {
            spacing: 10
            Text {
                text: "例句翻译"
                font.pixelSize: 16
                font.bold: true
                color: T.textDark
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                id: exTransSrc
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 13
                color: T.textBody
            }
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: T.line
            }
            Text {
                id: exTransResult
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 14
                color: T.greenDark
            }
            ActionBtn {
                text: "关闭"
                colorA: T.blueBright
                colorB: T.blue
                Layout.fillWidth: true
                onClicked: exampleTransPopup.close()
            }
        }
    }

    SequentialAnimation {
        id: cardSeq
        ScriptAction { script: fillCard() }
        ParallelAnimation {
            PropertyAnimation {
                target: cardBody
                property: "x"
                from: 340
                to: 0
                duration: 260
                easing.type: Easing.OutCubic
            }
            PropertyAnimation {
                target: cardBody
                property: "opacity"
                from: 0
                to: 1
                duration: 220
                easing.type: Easing.OutCubic
            }
            PropertyAnimation {
                target: cardBody
                property: "rotation"
                from: 8
                to: 0
                duration: 260
                easing.type: Easing.OutCubic
            }
            PropertyAnimation {
                target: cardBody
                property: "scale"
                from: 0.96
                to: 1
                duration: 260
                easing.type: Easing.OutCubic
            }
        }
    }

    SequentialAnimation {
        id: flyX
        ParallelAnimation {
            PropertyAnimation {
                id: flyXAnim
                target: cardBody
                property: "x"
                duration: 210
                easing.type: Easing.InOutCubic
            }
            PropertyAnimation {
                id: flyRot
                target: cardBody
                property: "rotation"
                duration: 210
                easing.type: Easing.InOutCubic
            }
            PropertyAnimation {
                target: cardBody
                property: "scale"
                to: 0.94
                duration: 210
                easing.type: Easing.InOutCubic
            }
            PropertyAnimation {
                target: cardBody
                property: "opacity"
                to: 0
                duration: 190
                easing.type: Easing.InCubic
            }
        }
        ScriptAction {
            script: {
                var k = pendingKnown
                pendingKnown = false
                answer(k)
            }
        }
    }

    ParallelAnimation {
        id: backX
        NumberAnimation {
            target: cardBody
            property: "x"
            to: 0
            duration: 240
            easing.type: Easing.OutBack
        }
        NumberAnimation {
            target: cardBody
            property: "rotation"
            to: 0
            duration: 240
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: cardBody
            property: "scale"
            to: 1
            duration: 240
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: cardBody
            property: "opacity"
            to: 1
            duration: 200
        }
    }

    Timer {
        id: speakTimer
        interval: 2200
        onTriggered: speaking = false
    }

    Connections {
        target: bridge
        function onListChanged() {
            needsReload = true
            if (SwipeView.isCurrentItem) {
                needsReload = false
                load(mode)
            }
        }
        function onExampleReady(wordId, sentence) {
            if (cardIndex >= 0 && cards[cardIndex].id === wordId) {
                currentExample = sentence
                cards[cardIndex].example = sentence
                if (revealed && !speaking && sentence !== "") {
                    speaking = true
                    bridge.speak(sentence)
                    speakTimer.restart()
                }
            }
        }
        function onTranslationReady(t) {
            if (exampleTransPopup.opened) {
                exTransResult.text = t
                return
            }
            var s = t.length > 60 ? t.substring(0, 60) + "…" : t
            showToast(s)
        }
        function onTranslationFailed(m) {
            if (exampleTransPopup.opened) {
                exTransResult.text = "翻译失败:" + m
                return
            }
            showToast("翻译失败:" + m)
        }
    }

    function showToast(msg) {
        toastText.text = msg
        toast.visible = true
        toastAnim.start()
        toastTimer.start()
    }

    function doExampleTranslate() {
        exTransSrc.text = currentExample
        exTransResult.text = "翻译中…"
        exampleTransPopup.open()
        bridge.translate(currentExample, bridge.aiModel())
    }

    function fillCard() {
        var c = cards[cardIndex]
        if (!c) {
            wordText.text = "没有更多卡片"
            ipaText.text = ""
            posText.text = ""
            meaningText.text = "换个词表,或先学别的词"
            rankText.text = ""
            currentExample = ""
            revealed = false
            return
        }
        rankText.text = c.rank > 0 ? "高频词 #" + c.rank : ""
        rankText.visible = rankText.text !== ""
        wordText.text = c.word
        ipaText.text = c.phonetic !== undefined ? (c.phonetic || "") : ""
        posText.text = c.pos || ""
        var m = c.meaning ? c.meaning : "（暂无释义）"
        meaningText.font.pixelSize = m.length > 60 ? 15
                                   : (m.length > 28 ? 17 : 21)
        meaningText.text = (c.pos ? c.pos + "  " : "") + m
        currentExample = c.example || ""
        if (currentExample === "")
            bridge.requestExample(c.id, c.word)
        revealed = false
        startSpeak()
    }

    function startSpeak() {
        var c = cards[cardIndex]
        if (!c) return
        speaking = true
        bridge.speak(c.word)
        speakTimer.restart()
    }

    function reveal() {
        if (cardIndex < 0 || revealed) return
        revealed = true
        if (currentExample !== "") {
            speaking = true
            bridge.speak(currentExample)
            speakTimer.restart()
        }
    }

    function answer(known) {
        if (cardIndex < 0) return
        bridge.answer(cards[cardIndex].id, known)
        showToast(known ? "已记录:认识" : "已加入复习队列")
        speaking = false
        if (cardIndex + 1 < cards.length) {
            cardIndex++
            cardSeq.start()
        } else {
            load(mode)
        }
    }

    function swipeAnswer(known) {
        if (cardIndex < 0) return
        pendingKnown = known
        flyXAnim.to = known ? 420 : -420
        flyRot.to = known ? 10 : -10
        flyX.start()
    }

    function load(modeArg) {
        if (modeArg !== undefined)
            mode = modeArg
        cards = mode === "review" ? bridge.reviewCards(10)
                                  : bridge.newCards(20)
        cardIndex = cards.length > 0 ? 0 : -1
        if (cardIndex >= 0) {
            cardSeq.start()
        } else {
            fillCard()
        }
    }

    function reloadIfNeeded() {
        if (needsReload) {
            needsReload = false
            load(mode)
        }
    }

    Component.onCompleted: load()

    onVisibleChanged: {
        if (visible && needsReload) {
            needsReload = false
            load(mode)
        }
    }

    component ActionBtn: Rectangle {
        id: root
        property string text: ""
        property color colorA: T.green
        property color colorB: T.greenDark
        property bool enabled: true
        signal clicked()
        Layout.fillWidth: true
        height: 44
        radius: 22
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.colorA }
            GradientStop { position: 1.0; color: root.colorB }
        }
        opacity: enabled ? 1 : 0.45
        Text {
            anchors.centerIn: parent
            text: root.text
            font.pixelSize: 15
            font.bold: true
            color: "#ffffff"
        }
        MouseArea {
            anchors.fill: parent
            enabled: root.enabled
            onClicked: root.clicked()
        }
    }

    component ModeBtn: Rectangle {
        id: root
        property string text: ""
        signal clicked()
        Layout.fillWidth: true
        height: 34
        radius: 17
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
}
