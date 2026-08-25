pragma Singleton
import QtQuick

// 全局主题。bridge.themeMode: 0=跟随系统,1=浅色,2=深色。
// 所有页面统一取这里的颜色,切换主题即时生效。
QtObject {
    id: theme

    // 当前是否深色(由 bridge 设置 + 系统配色共同决定)
    readonly property bool dark: {
        var m = bridge ? bridge.themeMode : 1
        if (m === 2) return true
        if (m === 1) return false
        return Qt.styleHints ? Qt.styleHints.colorScheme === Qt.ColorScheme.Dark
                             : false
    }

    // —— 基础表面 ——
    readonly property color bg: dark ? "#10160f" : "#f4f7f2"
    readonly property color card: dark ? "#1a2419" : "#ffffff"
    readonly property color track: dark ? "#26322a" : "#e3eae2"
    readonly property color line: dark ? "#2c3a2f" : "#e2ece2"

    // —— 主色绿 ——(深色下提亮保证对比度)
    readonly property color green: dark ? "#5cc678" : "#2e7d32"
    readonly property color greenDark: dark ? "#86e0a0" : "#1b5e20"
    readonly property color greenBright: dark ? "#7fe09a" : "#4caf50"
    readonly property color greenSoft: dark ? "#1f3a28" : "#e8f5e9"
    readonly property color greenBorder: dark ? "#2f5a3d" : "#cfe6d1"

    // —— 蓝 ——
    readonly property color blue: dark ? "#6fb0ff" : "#1565c0"
    readonly property color blueBright: dark ? "#8fc1ff" : "#2f7fd6"
    readonly property color blueSoft: dark ? "#1d2f49" : "#e3f2fd"

    // —— 红/琥珀 ——
    readonly property color red: dark ? "#ff8a8a" : "#c62828"
    readonly property color redBright: dark ? "#ff9a9a" : "#d64545"
    readonly property color redSoft: dark ? "#3d2222" : "#ffebee"
    readonly property color amber: dark ? "#ffd166" : "#f9a825"
    readonly property color amberSoft: dark ? "#3a3220" : "#fff8e1"

    // —— 文字 ——
    readonly property color textDark: dark ? "#eaf4ec" : "#183421"
    readonly property color textBody: dark ? "#d2ddd4" : "#38463c"
    readonly property color textMuted: dark ? "#8fa094" : "#7a8a7e"
    readonly property color navInactive: dark ? "#728377" : "#9aa99c"

    // —— 桌面端深色(保留)——
    readonly property color deskDark: "#142036"
    readonly property color deskBorder: "#26395e"
    readonly property color deskText: "#dce6f8"
    readonly property color deskAccent: "#9be8b8"
}
