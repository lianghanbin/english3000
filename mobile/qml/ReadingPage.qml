import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    property var articles: []
    property string html: ""

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            ComboBox {
                id: articleCombo
                Layout.fillWidth: true
                model: articles
                textRole: "title"
                onActivated: loadArticle(articles[currentIndex].id)
            }
            Button {
                text: "翻译全文"
                enabled: articleCombo.currentIndex >= 0
                onClicked: bridge.translate(
                    bridge.articleContent(
                        articles[articleCombo.currentIndex].id),
                    "qwen2.5:1.5b")
            }
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: textItem.height
            clip: true
            Text {
                id: textItem
                width: parent.width
                text: html
                textFormat: Text.RichText
                wrapMode: Text.Wrap
                onLinkActivated: function(link) {
                    menuPopup.word = link.replace("word://", "")
                    menuPopup.open()
                }
            }
        }

        TextArea {
            id: translationView
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            readOnly: true
            placeholderText: "译文显示在这里"
        }

        Label {
            text: "红色=未入词表 · 蓝=其他词表 · 绿=当前词表 · 黑=已掌握"
            color: "#888888"
            font.pixelSize: 12
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    Popup {
        id: menuPopup
        property string word: ""
        anchors.centerIn: parent
        width: parent.width * 0.8
        modal: true
        focus: true
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            Label {
                text: menuPopup.word
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
            Button {
                text: "发音"
                Layout.fillWidth: true
                onClicked: {
                    bridge.speak(menuPopup.word)
                    menuPopup.close()
                }
            }
            Button {
                text: "翻译这个单词"
                Layout.fillWidth: true
                onClicked: {
                    bridge.translate(menuPopup.word, "qwen2.5:1.5b")
                    menuPopup.close()
                }
            }
            Button {
                text: "加入阅读词表"
                Layout.fillWidth: true
                onClicked: {
                    bridge.addReadingWord(menuPopup.word)
                    menuPopup.close()
                }
            }
        }
    }

    Connections {
        target: bridge
        function onTranslationReady(t) {
            translationView.text = t
        }
    }

    function loadArticle(id) {
        html = bridge.articleHtml(id)
    }

    function load() {
        articles = bridge.articles()
        articleCombo.currentIndex = articles.length > 0 ? 0 : -1
        if (articles.length > 0)
            loadArticle(articles[0].id)
    }

    Component.onCompleted: load()
}
