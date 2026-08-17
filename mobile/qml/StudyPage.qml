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
                text: "未学 " + bridge.newCount + " · 待复习 "
                      + bridge.dueCount + " · 已掌握 " + bridge.masteredCount
                font.pixelSize: 12
                color: T.textMuted
                Layout.alignment: Qt.AlignVCenter
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            ModeBtn {
                text: "开始学习"
                Layout.fillWidth: true
                onClicked: load("learn")
            }
            ModeBtn {
                text: "开始复习"
                Layout.fillWidth: true
                onClicked: load("review")
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
                id: cardBody
                anchors.fill: parent
                anchors.margins: 2
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
                                    anchors.fill: parent
                                    onClicked: {
                                        bridge.translate(currentExample,
                                                         bridge.aiModel())
                                        showToast("翻译例句中…")
                                    }
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
                    Item {
                        Layout.alignment: Qt.AlignHCenter
                        width: 46
                        height: 46
                        Rectangle {
                            id: pulseRing
                            anchors.fill: parent
                            radius: width / 2
                            color: "transparent"
                            border.color: T.green
                            border.width: 2
                            scale: 0.55
                            opacity: 0
                            SequentialAnimation {
                                running: speaking
                                loops: Animation.Infinite
                                ParallelAnimation {
                                    NumberAnimation {
                                        target: pulseRing
                                        property: "scale"
                                        from: 0.55
                                        to: 1.25
                                        duration: 850
                                    }
                                    NumberAnimation {
                                        target: pulseRing
                                        property: "opacity"
                                        from: 0.85
                                        to: 0
                                        duration: 850
                                    }
                                }
                            }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: "♪"
                            font.pixelSize: 22
                            color: T.green
                        }
                    }
                    Item { Layout.fillHeight: true }
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
            text: "点卡片显示释义 · 认识/不认识自动记录 · AI 随时可问"
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

    SequentialAnimation {
        id: cardSeq
        PropertyAnimation { target: cardBody; property: "opacity"; to: 0; duration: 130 }
        PropertyAnimation { target: cardBody; property: "x"; to: -300; duration: 1 }
        ScriptAction { script: fillCard() }
        PropertyAnimation {
            target: cardBody
            property: "x"
            from: 300
            to: 0
            duration: 230
            easing.type: Easing.OutCubic
        }
        PropertyAnimation { target: cardBody; property: "opacity"; to: 1; duration: 170 }
    }

    Timer {
        id: speakTimer
        interval: 2200
        onTriggered: speaking = false
    }

    Connections {
        target: bridge
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
            var s = t.length > 60 ? t.substring(0, 60) + "…" : t
            showToast(s)
        }
    }

    function showToast(msg) {
        toastText.text = msg
        toast.visible = true
        toastAnim.start()
        toastTimer.start()
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
        rankText.text = c.rank > 0 ? "高频词 #" + c.rank
                                   : "第 " + (cardIndex + 1) + " / "
                                     + cards.length + " 词"
        wordText.text = c.word
        ipaText.text = c.phonetic !== undefined ? (c.phonetic || "") : ""
        posText.text = c.pos || ""
        meaningText.text = (c.pos ? c.pos + "  " : "") + (c.meaning || "")
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

    function load(modeArg) {
        if (modeArg !== undefined)
            mode = modeArg
        cards = mode === "review" ? bridge.reviewCards(10)
                                  : bridge.newCards(10)
        cardIndex = cards.length > 0 ? 0 : -1
        if (cardIndex >= 0) {
            cardSeq.start()
        } else {
            fillCard()
        }
    }

    Component.onCompleted: load()

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
