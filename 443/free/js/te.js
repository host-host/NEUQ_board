window.onerror = function(b, a, d, e, f) {
    if (!a || "Script error." === b)
        return !0;
    b = "msg=" + encodeURIComponent(b) + "&url=" + encodeURIComponent(a) + "&line=" + encodeURIComponent(d) + "&col=" + encodeURIComponent(e) + "&error=" + encodeURIComponent(f);
    a = new Image(1,1);
    a.src = "//a.tool.lu/__te.gif?" + b;
    var c = "_img_" + Math.random();
    window[c] = a;
    a.onload = a.onerror = function() {
        window[c] = null
    }
    ;
    return !0
}
;
