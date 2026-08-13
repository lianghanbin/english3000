import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    property var cards: []
    property int cardIndex: -1
    property bool revealed: false
    property string currentExample: ""

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Label {
                text: bridge.newCount + " 新词 · " + bridge.dueCount
                      + " 复习 · 已掌握 " + bridge.masteredCount
            }
            Item { Layout.fillWidth: true }
            Label {
                text: cardIndex >= 0 ? (cardIndex + 1) + "/" + cards.length : ""
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 12
            color: "#1e1e1e"

            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 32
                spacing: 10

                Label {
                    text: cardIndex >= 0 ? cards[cardIndex].word : "点下方按钮开始新词"
                    font.pixelSize: 34
                    font.bold: true
                    color: "white"
                    Layout.alignment: Qt.AlignHCenter
                }
                Label {
                    text: cardIndex >= 0 ? cards[cardIndex].pos : ""
                    color: "#888888"
                    Layout.alignment: Qt.AlignHCenter
                }
                Label {
                    text: cardIndex >= 0 ? cards[cardIndex].phonetic : ""
                    color: "#888888"
                    Layout.alignment: Qt.AlignHCenter
                }
                Label {
                    visible: revealed && cardIndex >= 0
                    text: cardIndex >= 0 ? cards[cardIndex].meaning : ""
                    color: "#cccccc"
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    visible: revealed && currentExample !== ""
                    text: currentExample
                    color: "#999999"
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        RowLayout {
            Button {
                text: "显示释义"
                enabled: cardIndex >= 0 && !revealed
                onClicked: revealed = true
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "不认识"
                enabled: revealed
                onClicked: answer(false)
            }
            Button {
                text: "认识"
                enabled: revealed
                onClicked: answer(true)
            }
        }

        Button {
            text: cards.length === 0 ? "开始新词" : "再来一组"
            Layout.fillWidth: true
            onClicked: load()
        }
    }

    Connections {
        target: bridge
        function onExampleReady(wordId, sentence) {
            if (cardIndex >= 0 && cards[cardIndex].id === wordId) {
                currentExample = sentence
                cards[cardIndex].example = sentence
            }
        }
    }

    function load() {
        cards = bridge.newCards(10)
        cardIndex = cards.length > 0 ? 0 : -1
        revealed = false
        currentExample = ""
        if (cardIndex >= 0) {
            currentExample = cards[cardIndex].example
            if (currentExample === "")
                bridge.requestExample(cards[cardIndex].id,
                                      cards[cardIndex].word)
        }
    }

    function answer(known) {
        bridge.answer(cards[cardIndex].id, known)
        if (cardIndex + 1 < cards.length) {
            cardIndex++
            revealed = false
            currentExample = cards[cardIndex].example
            if (currentExample === "")
                bridge.requestExample(cards[cardIndex].id,
                                      cards[cardIndex].word)
        } else {
            load()
        }
    }

    Component.onCompleted: load()
}
