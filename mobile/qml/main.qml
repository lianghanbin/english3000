import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

ApplicationWindow {
    id: win
    visible: true
    width: 420
    height: 760
    title: qsTr("英语三千")
    color: T.bg

    // 首帧遮罩:底色与启动主题一致,首帧绘制后淡出,兜住原生窗口->QML 的闪色。
    Rectangle {
        id: firstPaintMask
        anchors.fill: parent
        z: 200
        color: T.bg
        Behavior on opacity { NumberAnimation { duration: 180 } }
    }

    Column {
        anchors.fill: parent
        spacing: 0

        SwipeView {
            id: swipe
            width: parent.width
            height: parent.height - 62
            interactive: true
            currentIndex: 0

            // 每个页面用 Loader 作为直接子项(Loader 本身是 Item,
            // SwipeView 能正确布局)。启动只激活学习页以加快首帧,
            // 其余页面在首帧后由 warmup 定时器分帧后台预热,
            // 既不拖慢启动,滑动时页面也已就绪、不会卡顿。
            property var loaders: [page0, page1, page2, page3, page4, page5]
            property int warmupIndex: 1 // 0 已激活,从第 2 个页面开始预热
            property bool warmupDone: false

            function ensureLoaded(i) {
                if (i >= 0 && i < loaders.length)
                    loaders[i].active = true
            }

            Loader {
                id: page0
                active: true
                asynchronous: false
                source: "qrc:/mobile/qml/StudyPage.qml"
                onLoaded: firstPaintMask.opacity = 0
            }
            Loader {
                id: page1
                active: false
                asynchronous: true
                source: "qrc:/mobile/qml/WordListsPage.qml"
            }
            Loader {
                id: page2
                active: false
                asynchronous: true
                source: "qrc:/mobile/qml/ReadingPage.qml"
            }
            Loader {
                id: page3
                active: false
                asynchronous: true
                source: "qrc:/mobile/qml/TranslatePage.qml"
            }
            Loader {
                id: page4
                active: false
                asynchronous: true
                source: "qrc:/mobile/qml/StatsPage.qml"
            }
            Loader {
                id: page5
                active: false
                asynchronous: true
                source: "qrc:/mobile/qml/SettingsPage.qml"
            }

            // 首帧后每隔一帧激活一个页面,把 QML 解析开销摊到多个帧里,
            // 不会集中卡一下;6 个页面约在半秒多内全部预热完成。
            Timer {
                id: warmupTimer
                interval: 80
                repeat: true
                running: false
                onTriggered: {
                    if (swipe.warmupIndex >= swipe.loaders.length) {
                        swipe.warmupDone = true
                        stop()
                        return
                    }
                    swipe.loaders[swipe.warmupIndex].active = true
                    swipe.warmupIndex++
                }
            }

            onCurrentIndexChanged: {
                // 兜底:用户在预热完成前快速滑到某页,立即激活当前及相邻页。
                ensureLoaded(currentIndex)
                ensureLoaded(currentIndex - 1)
                ensureLoaded(currentIndex + 1)
                if (currentIndex === 0) {
                    var p = page0.item
                    if (p && p.reloadIfNeeded)
                        p.reloadIfNeeded()
                }
            }

            Component.onCompleted: warmupTimer.start()
        }

        Rectangle {
            id: navBar
            width: parent.width
            height: 62
            color: T.card
            border.color: T.line
            z: 10

            // 滑动胶囊指示器:在 6 个 tab 之间平滑移动
            Rectangle {
                id: pill
                y: 8
                width: Math.min(64, navBar.width / 6 - 12)
                height: navBar.height - 16
                radius: height / 2
                color: T.greenSoft
                x: navBar.width / 6 * swipe.currentIndex +
                   (navBar.width / 6 - width) / 2
                Behavior on x {
                    SpringAnimation {
                        spring: 4
                        damping: 0.42
                        epsilon: 0.01
                    }
                }
            }

            Row {
                anchors.fill: parent
                NavItem {
                    label: "学习"
                    icon: "study"
                    active: swipe.currentIndex === 0
                    onClicked: swipe.currentIndex = 0
                }
                NavItem {
                    label: "词表"
                    icon: "lists"
                    active: swipe.currentIndex === 1
                    onClicked: swipe.currentIndex = 1
                }
                NavItem {
                    label: "阅读"
                    icon: "reading"
                    active: swipe.currentIndex === 2
                    onClicked: swipe.currentIndex = 2
                }
                NavItem {
                    label: "翻译"
                    icon: "translate"
                    active: swipe.currentIndex === 3
                    onClicked: swipe.currentIndex = 3
                }
                NavItem {
                    label: "数据"
                    icon: "stats"
                    active: swipe.currentIndex === 4
                    onClicked: swipe.currentIndex = 4
                }
                NavItem {
                    label: "设置"
                    icon: "settings"
                    active: swipe.currentIndex === 5
                    onClicked: swipe.currentIndex = 5
                }
            }
        }
    }

    // 词典初始化横幅改为覆盖层,出现/消失不挤压内容布局。
    Rectangle {
        id: dictBanner
        visible: !bridge.dictReady
        anchors.top: parent.top
        width: parent.width
        height: visible ? 34 : 0
        z: 50
        color: T.amberSoft
        border.color: T.amber
        opacity: visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        Text {
            anchors.centerIn: parent
            text: "正在初始化离线词典…首次约需 10~20 秒"
            font.pixelSize: 12
            color: T.textDark
        }
    }

    Component.onCompleted: {
        fallbackHideTimer.start()
        // 全新安装(未看过引导)时,等界面就绪后自动播放一次
        if (!bridge.guideSeen())
            firstRunTimer.start()
    }
    Timer {
        id: firstRunTimer
        interval: 1400
        repeat: false
        onTriggered: startGuide()
    }

    // 全局引导:任意页面可调用 win.startGuide() 重新播放
    function startGuide() {
        // 先把所有页面加载好,引导切页时才不会空白
        for (var i = 0; i < swipe.loaders.length; ++i)
            swipe.loaders[i].active = true
        guide.start([
            { page: 0, icon: "👋",
              title: "欢迎使用「英语三千」",
              body: "用碎片时间背完高频 3000 词。\n点「下一步」,30 秒看懂怎么用。" },
            { page: 0, icon: "🃏",
              title: "学习:翻卡片背单词",
              body: "屏幕中央是单词卡。点一下卡片翻开释义;\n左滑=不认识,右滑=认识,\n也可以点下方按钮。" },
            { page: 0, icon: "🔊",
              title: "发音与例句",
              body: "点单词能朗读;翻开后若配了 AI,会自动生成例句,\n点例句旁的「译」可看中文。" },
            { page: 1, target: "nav",
              title: "底部这一栏是全部功能",
              body: "从左到右:\n学习 · 词表 · 阅读 · 翻译 · 数据 · 设置。\n随时点这里切换。" },
            { page: 1, icon: "📚",
              title: "词表:管理要背的词",
              body: "左侧选一个词表,右侧是它的词条。\n内置核心 3000 词;长按可删除自建词表。" },
            { page: 1, icon: "✨",
              title: "用 AI 生成词表",
              body: "点右上角「+」,输入一个主题(比如「旅游英语」),\nAI 会自动生成一批相关单词。需要先在设置填好 AI Key。" },
            { page: 2, icon: "📖",
              title: "阅读:在文章里记词",
              body: "生词在文章里用颜色标出:\n红色=生词,绿色=当前词表,蓝色=其他词表,黑色=已掌握。\n点单词可加入阅读词表。" },
            { page: 3, icon: "🌐",
              title: "翻译:不会的词自动收藏",
              body: "输入英文翻译成中文,\n翻译中遇到的生词会自动收进「翻译生词」词表,背的时候一起复习。" },
            { page: 4, icon: "📊",
              title: "数据:看学习进度",
              body: "这里统计已学、待复习、已掌握的数量和连续打卡天数,\n坚持每天来,曲线会往上走。" },
            { page: 5, icon: "🔑",
              title: "设置:配置 AI",
              body: "翻译、生成词表和例句都靠云端 AI。\n选一家服务商(推荐 DeepSeek 或智谱 GLM),\n点「获取 Key」复制,再回来点蓝色条一键粘贴。\n不想用 AI 也完全可以,基础背单词照常工作。" },
            { page: 0, icon: "🚀",
              title: "准备好了!",
              body: "引导随时可以在「设置 → 重新查看引导」重播。\n现在开始背第一个词吧 💪" }
        ])
    }

    GuideOverlay {
        id: guide
        gotoPage: function(i) { swipe.currentIndex = i }
        onDismissed: bridge.setGuideSeen(true)
    }

    Connections {
        target: bridge
        function onGuideRequested() { startGuide() }
    }

    Timer {
        id: fallbackHideTimer
        interval: 1000
        repeat: false
        onTriggered: firstPaintMask.opacity = 0
    }
    Connections {
        target: firstPaintMask
        function onOpacityChanged() {
            if (firstPaintMask.opacity <= 0)
                firstPaintMask.visible = false
        }
    }

    component NavItem: Item {
        id: root
        property string label: ""
        property string icon: ""
        property bool active: false
        signal clicked()
        width: parent.width / 6
        height: parent.height

        Column {
            id: col
            anchors.centerIn: parent
            spacing: 3
            scale: root.active ? 1.06 : 1.0
            Behavior on scale {
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }
            Image {
                id: iconImg
                anchors.horizontalCenter: parent.horizontalCenter
                width: 22
                height: 22
                sourceSize.width: 44
                sourceSize.height: 44
                smooth: true
                source: Icons.dataUri(root.icon,
                        root.active ? T.greenDark : T.navInactive)
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.label
                font.pixelSize: 11
                font.bold: root.active
                color: root.active ? T.greenDark : T.navInactive
                Behavior on color { ColorAnimation { duration: 180 } }
            }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: root.clicked()
        }
    }
}
