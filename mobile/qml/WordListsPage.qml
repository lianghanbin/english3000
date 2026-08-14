import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: "当前词表：" + bridge.currentListName
            font.bold: true
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Label {
            text: "未学 " + bridge.newCount + " · 待复习 "
                  + bridge.dueCount + " · 已掌握 "
                  + bridge.masteredCount
            color: "#666666"
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: bridge.wordLists()
            delegate: ItemDelegate {
                width: list.width
                text: modelData.name + "（" + modelData.wordCount
                      + " 词）" + (modelData.current ? "  [当前]" : "")
                onClicked: {
                    bridge.setCurrentList(modelData.id)
                    bridge.refresh()
                    list.model = bridge.wordLists()
                }
            }
        }

        Button {
            text: "刷新"
            Layout.fillWidth: true
            onClicked: list.model = bridge.wordLists()
        }
    }
}
