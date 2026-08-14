import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: win
    visible: true
    width: 420
    height: 760
    title: qsTr("English 3000")

    SwipeView {
        id: swipe
        anchors.fill: parent
        StudyPage {}
        ReadingPage {}
        WordListsPage {}
        TranslatePage {}
        SettingsPage {}
    }

    PageIndicator {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        count: swipe.count
        currentIndex: swipe.currentIndex
    }
}
