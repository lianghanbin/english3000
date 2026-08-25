import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 首次使用引导覆盖层。
// 用法:在 main.qml 顶层放一个 GuideOverlay,调用 start([steps])。
// 每个 step: { page: 0-5, title, body, target: "nav"|"center"(可选),
//             icon: 可选 emoji }
// 切到对应页面后,底部用半透明遮罩 + 卡片说明;target==="nav" 时
// 提示栏贴近底部导航,其余在屏幕中央偏下。
Item {
    id: root
    anchors.fill: parent
    visible: false
    z: 500

    property var steps: []
    property int index: 0
    property bool running: false
    // 由外部注入:切页函数 / 当前页
    property var gotoPage: null
    signal finished()
    signal dismissed()

    function start(s) {
        // 上次退出动画可能把 opacity 留在 0,必须重置
        root.opacity = 1
        steps = s
        index = 0
        running = true
        visible = true
        showStep()
    }

    function showStep() {
        var st = steps[index]
        if (!st) { finish(); return }
        if (gotoPage && st.page !== undefined)
            gotoPage(st.page)
        stepTitle.text = st.title || ""
        stepBody.text = st.body || ""
        stepIcon.text = st.icon || ""
        stepIcon.visible = !!(st.icon)
        dots.rebuild()
        // 进入动画
        card.scale = 0.9
        card.opacity = 1
        popAnim.start()
    }

    function next() {
        if (index + 1 >= steps.length) {
            finish()
        } else {
            index++
            showStep()
        }
    }

    function back() {
        if (index > 0) {
            index--
            showStep()
        }
    }

    function skip() {
        finish()
    }

    function finish() {
        if (!running)
            return
        running = false
        // 立即标记为已看并通知外部,不依赖退出动画是否跑完
        // (动画可能被切页/生命周期打断,导致每次启动都重复弹)
        root.dismissed()
        root.finished()
        exitAnim.start()
    }

    // 半透明遮罩
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: root.visible ? 0.62 : 0
        Behavior on opacity { NumberAnimation { duration: 250 } }
    }

    // 底部导航高亮光圈(target==nav 时)
    Rectangle {
        id: navGlow
        visible: root.running && steps[root.index]
                 && steps[root.index].target === "nav"
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6
        width: Math.min(parent.width - 24, 520)
        height: 64
        radius: 32
        x: (parent.width - width) / 2
        color: "transparent"
        border.width: 2
        border.color: "#ffffff"
        opacity: 0
        SequentialAnimation on opacity {
            running: navGlow.visible
            loops: Animation.Infinite
            NumberAnimation { to: 0.9; duration: 700 }
            NumberAnimation { to: 0.3; duration: 700 }
        }
    }

    // 说明卡片
    Rectangle {
        id: card
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - 40, 460)
        height: cardCol.implicitHeight + 36
        radius: 22
        color: "#ffffff"
        opacity: 0
        // 位置:nav 提示时在导航光圈上方,否则居中
        y: {
            if (!root.running || !steps[root.index])
                return parent.height * 0.4
            if (steps[root.index].target === "nav")
                return parent.height - 64 - height - 24
            return (parent.height - height) * 0.42
        }

        ColumnLayout {
            id: cardCol
            anchors.fill: parent
            anchors.margins: 20
            spacing: 10

            Text {
                id: stepIcon
                Layout.alignment: Qt.AlignHCenter
                font.pixelSize: 40
                visible: false
            }
            Text {
                id: stepTitle
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 19
                font.bold: true
                color: "#1f2937"
                wrapMode: Text.Wrap
            }
            Text {
                id: stepBody
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 14
                color: "#4b5563"
                wrapMode: Text.Wrap
                lineHeight: 1.35
            }

            // 进度点
            Row {
                id: dots
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 4
                spacing: 6
                property int count: 0
                function rebuild() {
                    count = root.steps.length
                    dotModel.model = count
                }
                Repeater {
                    id: dotModel
                    model: 0
                    Rectangle {
                        width: root.index === index ? 18 : 7
                        height: 7
                        radius: 3.5
                        color: root.index === index ? "#16a34a" : "#d1d5db"
                        Behavior on width { NumberAnimation { duration: 200 } }
                    }
                }
            }

            // 按钮区
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 6
                spacing: 10

                // 跳过(左侧)
                Text {
                    text: root.index === 0 ? "跳过引导" : "上一步"
                    font.pixelSize: 14
                    color: "#9ca3af"
                    Layout.preferredWidth: 80
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.index === 0 ? root.skip() : root.back()
                    }
                }

                Item { Layout.fillWidth: true }

                // 下一步/完成(右侧主按钮)
                Rectangle {
                    height: 42
                    radius: 21
                    color: "#16a34a"
                    Layout.preferredWidth: nextTxt.implicitWidth + 40
                    Text {
                        id: nextTxt
                        anchors.centerIn: parent
                        text: root.index + 1 >= root.steps.length
                              ? "开始使用 ✓" : "下一步"
                        font.pixelSize: 15
                        font.bold: true
                        color: "#ffffff"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.next()
                    }
                }
            }
        }
    }

    NumberAnimation {
        id: popAnim
        target: card
        property: "scale"
        from: 0.9; to: 1.0
        duration: 220
        easing.type: Easing.OutBack
    }
    ParallelAnimation {
        id: exitAnim
        NumberAnimation { target: card; property: "opacity"; to: 0; duration: 200 }
        NumberAnimation {
            target: root
            property: "opacity"
            to: 0
            duration: 250
            onFinished: {
                root.visible = false
            }
        }
    }
}
