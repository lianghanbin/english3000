pragma Singleton
import QtQuick

// 统一线性导航图标(24x24 描边风格)。
// 用法:Icons.dataUri("study", "#2e7d32") 返回可直接给 Image.source 的 SVG URI。
QtObject {
    function dataUri(name, color) {
        var c = color || "#2e7d32"
        var inner = _paths[name] || _paths["study"]
        inner = inner.split("__C__").join(c)
        var s = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' "
              + "fill='none' stroke='" + c + "' stroke-width='2' "
              + "stroke-linecap='round' stroke-linejoin='round'>" + inner + "</svg>"
        return "data:image/svg+xml;utf8," + encodeURIComponent(s)
    }

    readonly property var _paths: {
        "study": "<rect x='3' y='5' width='18' height='14' rx='2.5'/>"
               + "<path d='M3 10h18'/>"
               + "<path d='M8 15h5'/>",
        "lists": "<path d='M9 6h11'/><path d='M9 12h11'/><path d='M9 18h11'/>"
               + "<circle cx='4.5' cy='6' r='1.3' fill='__C__' stroke='none'/>"
               + "<circle cx='4.5' cy='12' r='1.3' fill='__C__' stroke='none'/>"
               + "<circle cx='4.5' cy='18' r='1.3' fill='__C__' stroke='none'/>",
        "reading": "<path d='M12 6.5C10 5 5 5 3.5 5.5v13C5 18 10 18 12 19.5c2-1.5 7-1.5 8.5-1v-13C19 5 14 5 12 6.5z'/>"
                 + "<path d='M12 6.5v13'/>",
        "translate": "<circle cx='12' cy='12' r='9'/>"
                   + "<path d='M3.5 12h17'/>"
                   + "<path d='M12 3c2.6 2.4 2.6 15.6 0 18'/>"
                   + "<path d='M12 3c-2.6 2.4-2.6 15.6 0 18'/>"
                   + "<path d='M5 8h14'/><path d='M5 16h14'/>",
        "stats": "<rect x='4' y='12' width='3.2' height='7' rx='1'/>"
               + "<rect x='10.4' y='7' width='3.2' height='12' rx='1'/>"
               + "<rect x='16.8' y='4' width='3.2' height='15' rx='1'/>",
        "settings": "<path d='M4 7h10'/><path d='M18 7h2'/>"
                  + "<circle cx='15' cy='7' r='2.2'/>"
                  + "<path d='M4 12h3'/><path d='M11 12h9'/>"
                  + "<circle cx='8.5' cy='12' r='2.2'/>"
                  + "<path d='M4 17h12'/><path d='M20 17h0.1'/>"
                  + "<circle cx='17.5' cy='17' r='2.2'/>"
    }
}
