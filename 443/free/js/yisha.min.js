var nameOf = n=>n.toString().replace(/[ |\(\)=>]/g, "");
window.ys = {},
function(n, t) {
    "use strict";
    n.extend(t, {
        openDialog: function(i) {
            t.isMobile() ? (i.width = "auto",
            i.height = "auto") : i.height || (i.height = n(window).height() - 50 + "px");
            var r = n.extend({
                type: 2,
                title: "",
                width: "768px",
                content: "",
                maxmin: !0,
                shade: .4,
                btn: ["确认", "关闭"],
                callback: null,
                shadeClose: !1,
                fix: !1,
                closeBtn: 1
            }, i);
            layer.open({
                type: r.type,
                area: [r.width, r.height],
                maxmin: r.maxmin,
                shade: r.shade,
                title: r.title,
                content: r.content,
                btn: r.btn,
                shadeClose: r.shadeClose,
                fix: r.fix,
                closeBtn: r.closeBtn,
                yes: r.callback,
                cancel: function() {
                    return !0
                }
            })
        },
        openDialogContent: function(i) {
            t.isMobile() ? (i.width = "auto",
            i.height = "auto") : i.height || (i.height = n(window).height() - 50 + "px");
            var r = n.extend({
                type: 1,
                title: !1,
                width: "768px",
                content: "",
                maxmin: !1,
                shade: .4,
                btn: null,
                callback: null,
                shadeClose: !0,
                fix: !0,
                closeBtn: 0
            }, i);
            layer.open({
                type: r.type,
                area: [r.width, r.height],
                maxmin: r.maxmin,
                shade: r.shade,
                title: r.title,
                content: r.content,
                btn: r.btn,
                shadeClose: r.shadeClose,
                fix: r.fix,
                closeBtn: r.closeBtn,
                yes: r.callback,
                cancel: function() {
                    return !0
                }
            })
        },
        closeDialog: function() {
            var n = parent.layer.getFrameIndex(window.name);
            parent.layer.close(n)
        },
        msgWarning: function(n) {
            layer.msg(n, {
                icon: 0,
                time: 1e3,
                shift: 5
            })
        },
        msgSuccess: function(n) {
            t.isNullOrEmpty(n) && (n = "操作成功");
            top.layer.msg(n, {
                icon: 1,
                time: 1e3,
                shift: 5
            })
        },
        msgError: function(n) {
            t.isNullOrEmpty(n) && (n = "操作失败");
            layer.msg(n, {
                icon: 2,
                time: 3e3,
                shift: 5
            })
        },
        toastWarning: function(n) {
            t.isNullOrEmpty(n) && (n = "操作失败");
            Toastify({
                text: n,
                duration: 3e3,
                newWindow: !0,
                close: !0,
                gravity: "top",
                position: "center",
                stopOnFocus: !0,
                style: {
                    background: "#ff8500"
                },
                onClick: function() {}
            }).showToast()
        },
        toastSuccess: function(n) {
            t.isNullOrEmpty(n) && (n = getTextByLanguage("操作成功"));
            Toastify({
                text: n,
                duration: 3e3,
                newWindow: !0,
                close: !0,
                gravity: "top",
                position: "center",
                stopOnFocus: !0,
                style: {
                    background: "#5eb862"
                },
                onClick: function() {}
            }).showToast()
        },
        toastError: function(n) {
            t.isNullOrEmpty(n) && (n = getTextByLanguage("操作失败"));
            Toastify({
                text: n,
                duration: 3e3,
                newWindow: !0,
                close: !0,
                gravity: "top",
                position: "center",
                stopOnFocus: !0,
                style: {
                    background: "#666666"
                },
                onClick: function() {}
            }).showToast()
        },
        alertWarning: function(n) {
            layer.alert(n, {
                icon: 0,
                title: getTextByLanguage("系统提示"),
                btn: ["确认"],
                btnclass: ["btn btn-primary"]
            })
        },
        alertSuccess: function(n) {
            layer.alert(n, {
                icon: 1,
                title: getTextByLanguage("系统提示"),
                btn: [getTextByLanguage("确认")],
                btnclass: ["btn btn-primary"]
            })
        },
        alertError: function(n) {
            layer.alert(n, {
                icon: 2,
                title: getTextByLanguage("系统提示"),
                btn: [getTextByLanguage("确认")],
                btnclass: ["btn btn-primary"]
            })
        },
        confirm: function(n, t) {
            layer.confirm(n, {
                icon: 3,
                title: getTextByLanguage("系统提示"),
                btn: [getTextByLanguage("确认"), getTextByLanguage("取消")],
                btnclass: ["btn btn-primary", "btn btn-danger"]
            }, function(n) {
                layer.close(n);
                t(!0)
            })
        },
        showLoading: function(i) {
            t.isNullOrEmpty(i) && (i = getTextByLanguage("正在处理中..."));
            n.blockUI({
                message: '<div class="loaderbox"><div class="loading-activity"><\/div> ' + i + "<\/div>",
                css: {
                    border: "none",
                    backgroundColor: "transparent"
                }
            })
        },
        closeLoading: function() {
            setTimeout(function() {
                n.unblockUI()
            }, 50)
        },
        getIds: function(t) {
            var i = "";
            return n.each(t, function(n, t) {
                n == 0 ? i = t.Id : i += "," + t.Id
            }),
            i
        },
        checkRowEdit: function(n) {
            if (n.length == 0)
                t.toastError("您没有选择任何行！");
            else if (n.length > 1)
                t.toastError("您的选择大于1行！");
            else if (n.length == 1)
                return !0;
            return !1
        },
        checkRowDelete: function(n) {
            if (n.length == 0)
                t.toastError("您没有选择任何行！");
            else if (n.length > 0)
                return !0;
            return !1
        },
        ajax: function(i) {
            var u = "", f = location.href, r;
            if (f.indexOf("/en") > 0 && (u = "en"),
            r = n.extend({
                url: i.url,
                "async": !0,
                type: "get",
                data: i.data || {},
                dataType: i.dataType || "json",
                error: function() {
                    t.alertError(getTextByLanguage("系统出错了"))
                },
                success: function() {
                    t.toastSuccess()
                },
                beforeSend: function() {
                    t.showLoading(getTextByLanguage("正在处理中..."))
                },
                complete: function() {
                    t.closeLoading()
                }
            }, i),
            t.isNullOrEmpty(r.url)) {
                t.alertError("url 参数不能为空");
                return
            }
            r.url = t.addVerifyParam(r.url);
            n.ajax({
                url: r.url,
                "async": r.async,
                type: r.type,
                data: r.data,
                headers: {
                    language: u
                },
                dataType: r.dataType,
                error: r.error,
                success: r.success,
                beforeSend: r.beforeSend,
                complete: r.complete
            })
        },
        ajaxUploadFile: function(i) {
            var r = n.extend({
                url: i.url,
                data: i.data || {},
                error: function() {
                    t.alertError(getTextByLanguage("系统出错了"))
                },
                success: function() {
                    t.toastSuccess()
                },
                beforeSend: function() {
                    t.showLoading(getTextByLanguage("正在处理中..."))
                },
                complete: function() {
                    t.closeLoading()
                }
            }, i);
            if (t.isNullOrEmpty(r.url)) {
                t.alertError(getTextByLanguage("url 参数不能为空"));
                return
            }
            if (t.isNullOrEmpty(r.data)) {
                t.alertError(getTextByLanguage("data 参数不能为空"));
                return
            }
            r.url = t.addVerifyParam(r.url);
            n.ajax({
                url: r.url,
                data: r.data,
                type: "post",
                processData: !1,
                contentType: !1,
                error: r.error,
                success: r.success,
                beforeSend: r.beforeSend,
                complete: r.complete
            })
        },
        addVerifyParam: function(n) {
            var t = t_timestamp
              , i = t_version;
            return n += n.indexOf("?") > 0 ? "&" : "?",
            n += "gts=" + t + "&gv=" + i,
            n + ("&r_=" + Math.random().toString())
        },
        exportExcel: function(n, i) {
            t.ajax({
                url: n,
                type: "post",
                data: i,
                success: function(n) {
                    n.Tag == 1 ? window.location.href = ctx + "File/DownloadFile?filePath=" + n.Data + "&delete=1" : t.toastError(n.Message)
                },
                beforeSend: function() {
                    t.showLoading("正在导出数据，请稍后...")
                }
            })
        },
        request: function(n) {
            var i = decodeURI(window.location.search)
              , r = new RegExp("(^|&)" + n + "=([^&]*)(&|$)")
              , t = i.substr(1).match(r);
            return t != null ? unescape(t[2]) : null
        },
        getHttpFileName: function(n) {
            if (n == null || n == "")
                return n;
            var t = n.lastIndexOf("/");
            return t > 0 ? n.substring(t + 1) : n
        },
        getFileNameWithoutExtension: function(n) {
            if (n == null || n == "")
                return n;
            var t = n.indexOf(".");
            return t > 0 ? n.substring(0, t) : n
        },
        changeURLParam: function(n, t, i) {
            var e = t + "=([^&]*)", r = t + "=" + i, f, u;
            return n.match(e) ? (f = "/(" + t + "=)([^&]*)/gi",
            n.replace(eval(f), r)) : n.match("[?]") ? (u = n.split("#"),
            u.length > 1 ? u[0] + "&" + r + "#" + u[1] : n + "&" + r) : n + "?" + r
        },
        getURLWithoutQueryString: function() {
            return window.location.origin + window.location.pathname
        },
        getURLParam: function(n) {
            n = n.replace(/[\[]/, "\\[").replace(/[\]]/, "\\]");
            var i = new RegExp("[\\?&]" + n + "=([^&#]*)")
              , t = i.exec(location.search);
            return t === null ? "" : decodeURIComponent(t[1].replace(/\+/g, " "))
        },
        isNullOrEmpty: function(n) {
            return typeof n == "string" && n == "" || n == null || n == undefined ? !0 : !1
        },
        getJson: function(n) {
            return n
        },
        isJSON: function(n) {
            try {
                return JSON.parse(n) && !!n
            } catch (t) {
                return !1
            }
        },
        getGuid: function(n, t) {
            var i = "10000000-1000-4000-8000-100000000000".replace(/[018]/g, n=>(n ^ crypto.getRandomValues(new Uint8Array(1))[0] & 15 >> n / 4).toString(16));
            return n == "1" && (i = i.toUpperCase()),
            t != "1" && (i = i.replace(/-/g, "")),
            i
        },
        getValueByKey: function(t, i) {
            var r = "";
            return n.each(t, function(n, t) {
                t.Key == i && (r = t.Value)
            }),
            r
        },
        getLastValue: function(n) {
            if (!t.isNullOrEmpty(n)) {
                var i = n.toString().split(",");
                return i[i.length - 1]
            }
            return ""
        },
        formatDate: function(n, t) {
            var i, r, u;
            if (!n)
                return "";
            i = n;
            typeof n == "string" && (i = n.indexOf("/Date(") > -1 ? new Date(parseInt(n.replace("/Date(", "").replace(")/", ""), 10)) : new Date(Date.parse(n.replace(/-/g, "/").replace("T", " ").split(".")[0])));
            r = {
                "M+": i.getMonth() + 1,
                "d+": i.getDate(),
                "H+": i.getHours(),
                "m+": i.getMinutes(),
                "s+": i.getSeconds(),
                "q+": Math.floor((i.getMonth() + 3) / 3),
                S: i.getMilliseconds()
            };
            /(y+)/.test(t) && (t = t.replace(RegExp.$1, (i.getFullYear() + "").substr(4 - RegExp.$1.length)));
            for (u in r)
                new RegExp("(" + u + ")").test(t) && (t = t.replace(RegExp.$1, RegExp.$1.length == 1 ? r[u] : ("00" + r[u]).substr(("" + r[u]).length)));
            return t
        },
        getTime: function() {
            var n = new Date;
            return [n.getFullYear(), (n.getMonth() + 1).padLeft(), n.getDate().padLeft()].join("-") + " " + [n.getHours().padLeft(), n.getMinutes().padLeft(), n.getSeconds().padLeft()].join(":")
        },
        getTimeWithMilliSecond: function() {
            var n = new Date;
            return [n.getHours().padLeft(), n.getMinutes().padLeft(), n.getSeconds().padLeft(), n.getMilliseconds().padLeft("3", "0")].join(":")
        },
        getUTCMilliseconds: function() {
            return (new Date).getUTCMilliseconds().padLeft("3", "0")
        },
        formatCurrency: function(n) {
            var r, i, t;
            for (n = n.toString().replace(/\$|\,/g, ""),
            isNaN(n) && (n = "0"),
            r = n == (n = Math.abs(n)),
            n = Math.floor(n * 100 + .50000000001),
            i = n % 100,
            n = Math.floor(n / 100).toString(),
            i < 10 && (i = "0" + i),
            t = 0; t < Math.floor((n.length - (1 + t)) / 3); t++)
                n = n.substring(0, n.length - (4 * t + 3)) + "," + n.substring(n.length - (4 * t + 3));
            return (r ? "" : "-") + n + "." + i
        },
        trim: function(n, i) {
            return i ? t.trimEnd(t.trimStart(n, i), i) : n.replace(/(^\s*)|(\s*$)/g, "")
        },
        trimAllSpace: function(n) {
            return n.replace(/\s*/g, "")
        },
        trimStart: function(n, t) {
            var r, i;
            return t == null || t == "" ? n.replace(/^s*/, "") : (r = new RegExp("^" + t + "*"),
            i = n.replace(r, ""),
            i)
        },
        trimEnd: function(n, t) {
            var r, i, u;
            if (t == null || t == "") {
                for (r = /s/,
                i = n.length; r.test(n.charAt(--i)); )
                    ;
                return n.slice(0, i + 1)
            }
            if (r = new RegExp(t),
            i = n.length,
            t.length < i) {
                for (u = ""; ; ) {
                    if (u = n.slice(i - t.length, i),
                    !r.test(u))
                        break;
                    i -= t.length
                }
                return n.slice(0, i)
            }
            return n
        },
        toString: function(n) {
            return n == null ? "" : n.toString()
        },
        replaceAll: function(n, t, i) {
            return n.replace(new RegExp(t,"g"), i)
        },
        openLink: function(n, t) {
            var i = document.createElement("a");
            i.target = t ? t : "_blank";
            i.href = n;
            i.click()
        },
        recursion: function(n, i, r, u, f) {
            u || (u = "id");
            f || (f = "parentId");
            for (var e in n)
                if (n[e][u] == i)
                    return r.push(n[e]),
                    t.recursion(n, n[e][f], r, u, f)
        },
        isMobile: function() {
            return navigator.userAgent.match(/(Android|iPhone|SymbianOS|Windows Phone|iPad|iPod)/i)
        },
        jsonSort: function(n, t) {
            function i(n, t) {
                t = t || !1;
                var r = {};
                return "[object Array]" === Object.prototype.toString.call(n) ? (r = t == !0 ? n.sort() : n,
                r.forEach(function(n, u) {
                    r[u] = i(n, t)
                }),
                t == !0 && (r = r.sort(function(n, t) {
                    return (n = JSON.stringify(n)) < (t = JSON.stringify(t)) ? -1 : t < n ? 1 : 0
                }))) : "[object Object]" === Object.prototype.toString.call(n) ? Object.keys(n).sort(function(n, t) {
                    return n < t ? -1 : n > t ? 1 : 0
                }).forEach(function(u) {
                    r[u] = i(n[u], t)
                }) : r = n,
                r
            }
            function e(n) {
                return (n = n.replace(/,[ \t\r\n]+}/g, "}")).replace(/,[ \t\r\n]+\]/g, "]")
            }
            var r, u, f;
            if (n)
                try {
                    n = e(n);
                    u = JSON.parse(n);
                    f = i(u, t);
                    r = JSON.stringify(f, null, 2)
                } catch (n) {
                    throw n;
                }
            return r
        },
        copyToClipboard: function(n) {
            if (navigator.clipboard)
                return navigator.clipboard.writeText(n);
            if (window.clipboardData && window.clipboardData.setData)
                return window.clipboardData.setData("Text", n),
                Promise.resolve();
            const t = document.createElement("INPUT");
            return t.setAttribute("type", "text"),
            t.value = n,
            document.body.append(t),
            t.focus(),
            t.select(),
            document.execCommand("copy"),
            t.remove(),
            Promise.resolve()
        },
        copyToClipboardToast: function(n) {
            if (t.isNullOrEmpty(n)) {
                t.toastError(getTextByLanguage("复制的内容不能为空"));
                return
            }
            t.copyToClipboard(n).then(()=>{
                t.toastSuccess(getTextByLanguage("复制到剪贴板成功"))
            }
            ).catch(n=>{
                t.toastSuccess(getTextByLanguage("复制到剪贴板失败：") + n)
            }
            )
        },
        copyToClipboardToastText: function(n) {
            if (t.isNullOrEmpty(n)) {
                t.toastError(getTextByLanguage("复制的内容不能为空"));
                return
            }
            t.copyToClipboard(n).then(()=>{
                t.toastSuccess(n + " " + getTextByLanguage("复制到剪贴板成功"))
            }
            ).catch(n=>{
                t.toastSuccess(getTextByLanguage("复制到剪贴板失败：") + n)
            }
            )
        },
        copyToClipboardByFormElementId: function(i) {
            var r = n("#" + i).val();
            t.copyToClipboardToast(r)
        },
        copyToClipboardToastTextByFormElementId: function(i) {
            var r = n("#" + i).val();
            t.copyToClipboardToastText(r)
        },
        copyToClipboardByNonFormElementId: function(i) {
            var r = n("#" + i).html();
            t.copyToClipboardToast(r)
        },
        copyToClipboardByNonFormThis: function(i) {
            var r = n(i).html();
            t.copyToClipboardToast(r)
        },
        copyToClipboardToastTextByNonFormThis: function(i) {
            var r = n(i).html();
            t.copyToClipboardToastText(r)
        },
        escape: function(n) {
            return n.replace(/\\/g, "\\\\").replace(/\"/g, '\\"')
        },
        unescape: function(n) {
            return n.replace(/\\\\/g, "\\").replace(/\\\"/g, '"')
        },
        arrayRemove: function(n, t) {
            const i = n.indexOf(t);
            return i >= 0 && n.splice(i, 1),
            n.length
        },
        removeBracket: function(n) {
            var t = n.replace("[", "").replace("]", "");
            return t.replace("{", "").replace("}", "")
        },
        componentToHex: function(n) {
            var t = n.toString(16);
            return t.length == 1 ? "0" + t : t
        },
        colorRGB2Hex: function(n) {
            n = t.trim(n);
            var r = n.replace("(", "").replace(")", "").replace(/，/g, ",").split(",")
              , u = parseInt(r[0])
              , f = parseInt(r[1])
              , e = parseInt(r[2])
              , i = "";
            return r.length >= 4 && (i = parseFloat(r[3]) * 255,
            i = parseInt(i),
            i >= 0 && (i = t.componentToHex(i))),
            "#" + t.componentToHex(u) + t.componentToHex(f) + t.componentToHex(e) + i
        },
        colorHex2RGB: function(n) {
            var i;
            n = t.trim(n);
            var u = n.substring(1, 3)
              , f = n.substring(3, 5)
              , e = n.substring(5, 7)
              , r = "";
            return n.length > 8 && (r = n.substring(7, 9)),
            i = {},
            i.r = parseInt(u, 16),
            i.g = parseInt(f, 16),
            i.b = parseInt(e, 16),
            r != "" && (i.a = parseInt(r, 16),
            i.a = parseFloat(i.a / 255).toFixed(2)),
            i
        },
        setConfig: function(n, t) {
            try {
                localStorage.setItem(n, t)
            } catch (i) {
                console.error(i)
            }
        },
        getConfig: function(n) {
            return localStorage.getItem(n)
        },
        removeConfig: function(n) {
            return localStorage.removeItem(n)
        },
        loadConfig: function(i, r, u) {
            var f, r;
            if (u || (u = {}),
            f = t.getConfig(r),
            f) {
                for (r in u)
                    delete u[r];
                n.extend(!0, u, JSON.parse(f))
            }
            return n("#" + i).setWebControls(u),
            u
        },
        saveConfig: function(i, r, u) {
            var f = n("#" + i).getWebControls(), e;
            return u && n.extend(f, u),
            e = JSON.stringify(f),
            e.length < 1e4 && t.setConfig(r, JSON.stringify(f)),
            f
        },
        scrollBottom: function(t) {
            n("html, body").animate({
                scrollTop: t
            })
        },
        isChinese: function(n) {
            return /^[\u4E00-\u9FA5]+$/.test(n) ? !0 : !1
        },
        strToHexArr: function(n) {
            for (var e, i, o, r = t.removeBracket(n), u = r.split(/[, ]+/), f = 0; f < u.length; f++)
                t.isNullOrEmpty(u[f]) || (u[f] = u[f].replace("0x", "").replace("0X", ""));
            for (e = [],
            r = u.join(""),
            r = r.replace(/\s+/g, ""),
            i = r.length; i >= 2; )
                o = r.substring(i - 2, i),
                e.push(o),
                i = i - 2;
            return i >= 1 && e.push(r.substring(0, i)),
            e.reverse()
        },
        queryStringToJSON: function(n, t) {
            var i = n, u = i.indexOf("?"), f, r;
            return u >= 0 && (i = i.substring(u + 1)),
            f = i.split("&"),
            r = {},
            f.forEach(function(n) {
                n = n.split("=");
                r[n[0]] = t ? decodeURIComponent(n[1] || "") : n[1]
            }),
            JSON.parse(JSON.stringify(r))
        },
        jsonToQueryString: function(n, t) {
            var i = null;
            return i = typeof n == "string" ? JSON.parse(n) : n,
            Object.keys(i).map(n=>t ? encodeURIComponent(n) + "=" + encodeURIComponent(i[n]) : n + "=" + i[n]).join("&")
        },
        isOverlapping: function(n, t) {
            const i = n.getBoundingClientRect()
              , r = t.getBoundingClientRect();
            return i.left < r.right && i.right > r.left && i.top < r.bottom && i.bottom > r.top
        },
        getElementActualWidth: function(n) {
            var t = document.getElementById(n);
            if (t) {
                const n = parseInt(getComputedStyle(t).width) + parseInt(getComputedStyle(t).marginLeft) + parseInt(getComputedStyle(t).marginRight);
                return isNaN(n) ? 0 : n
            }
            return 0
        },
        getElementActualWidthByElement: function(n) {
            if (n) {
                const t = parseInt(getComputedStyle(n).width) + parseInt(getComputedStyle(n).marginLeft) + parseInt(getComputedStyle(n).marginRight);
                return isNaN(t) ? 0 : t
            }
            return 0
        },
        getElementActualHeight: function(n) {
            var t = document.getElementById(n);
            if (t) {
                const n = parseInt(getComputedStyle(t).height) + parseInt(getComputedStyle(t).marginTop) + parseInt(getComputedStyle(t).marginBottom);
                return isNaN(n) ? 0 : n
            }
            return 0
        },
        getElementMarginHeight: function(n) {
            var t = document.getElementById(n);
            return parseInt(getComputedStyle(t).marginTop) + parseInt(getComputedStyle(t).marginBottom)
        },
        getWeekStr: function(n) {
            var t = null;
            return t = isChineseLanguage ? ["日", "一", "二", "三", "四", "五", "六", "日"] : ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"],
            t[n]
        },
        getLocationOrigin: function() {
            return typeof window.location.origin == "undefined" && (window.location.origin = window.location.protocol + "//" + window.location.hostname + (window.location.port ? ":" + window.location.port : "")),
            window.location.origin
        },
        isNumeric: function(n) {
            return typeof n != "string" ? !1 : !isNaN(n) && !isNaN(parseFloat(n))
        },
        bindEnterKey: function(t, i) {
            n("#" + t).bind("keyup", function(t) {
                t.keyCode == 13 && n("#" + i).click()
            })
        },
        downloadImage: function(n, t) {
            var f, r;
            t = t || {};
            var e = t.filename ? t.filename : "下载"
              , u = t.ext ? t.ext : "png"
              , i = "image/png";
            u == "jpg" ? i = "image/jpg" : u == "gif" && (i = "image/gif");
            f = n.toDataURL(i, 1).replace(i, "image/octet-stream");
            r = document.createElement("a");
            r.href = f;
            r.download = e + "." + u;
            r.dispatchEvent(new MouseEvent("click",{
                bubbles: !0,
                cancelable: !0,
                view: window
            }))
        },
        downloadSvg: function(n, t) {
            t = t || {};
            var r = t.filename ? t.filename : "下载"
              , u = new Blob([n],{
                type: "image/svg+xml;charset=utf-8"
            })
              , f = URL.createObjectURL(u)
              , i = document.createElement("a");
            i.href = f;
            i.download = r + ".svg";
            i.dispatchEvent(new MouseEvent("click",{
                bubbles: !0,
                cancelable: !0,
                view: window
            }))
        },
        downloadAsCSV: function(n, i) {
            for (var e, o, r, u, c, s = document.querySelectorAll("table#" + n + " tr"), h = [], f = 0; f < s.length; f++) {
                for (e = [],
                o = s[f].querySelectorAll("td, th"),
                r = 0; r < o.length; r++)
                    u = o[r].innerText.replace(/(\r\n|\n|\r)/gm, "").replace(/(\s\s)/gm, " "),
                    u = u.replace(/"/g, `""`),
                    e.push(`"` + u + `"`);
                h.push(e.join(","))
            }
            c = h.join("\n");
            t.downloadBlob(c, i, "text/csv;charset=utf-8;")
        },
        downloadBlob: function(n, i, r) {
            if (t.isNullOrEmpty(n)) {
                t.toastError("下载的数据不能为空");
                return
            }
            var f = new Blob([n],{
                type: r
            })
              , e = URL.createObjectURL(f)
              , u = document.createElement("a");
            u.href = e;
            u.setAttribute("download", i);
            u.click()
        },
        localImageToBase64: function(n, t) {
            if (n.files.length == 0)
                return "";
            const i = n.files[0]
              , r = new FileReader;
            r.onload = function(n) {
                const r = n.target.result;
                t && t(i, i.name, r)
            }
            ;
            r.readAsDataURL(i)
        },
        getDefaultInt: function(n, t) {
            if (n) {
                var i = parseInt(n, 10);
                if (!isNaN(i))
                    return i
            }
            return t ? t : 0
        },
        getArrayValueByKey: function(n, t, i, r) {
            for (var u = 0; u < n.length; u++)
                if (n[u][t] == i)
                    return n[u][r];
            return ""
        }
    })
}(window.jQuery, window.ys);
Number.prototype.padLeft = function(n, t) {
    var i = String(n || 10).length - String(this).length + 1;
    return i > 0 ? new Array(i).join(t || "0") + this : this
}
;
String.prototype.insert = function(n, t) {
    return n > 0 ? this.substring(0, n) + t + this.substr(n) : t + this
}
,
function(n) {
    "use strict";
    n.fn.ysRadioBox = function(t, i) {
        var u, f, r, e;
        return typeof t == "string" ? n.fn.ysRadioBox.methods[t](this, i) : (u = n(this),
        f = u.attr("id"),
        !f) ? !1 : (r = n.extend({
            url: null,
            key: "Key",
            value: "Value",
            data: null,
            dataName: "Data",
            "default": undefined
        }, t),
        e = {
            loadData: function() {
                r.url && n.ajax({
                    url: r.url,
                    type: "get",
                    dataType: "json",
                    "async": !1,
                    cache: !1,
                    success: function(n) {
                        r.data = n;
                        r.dataName && r.data != null && (r.data = r.data[r.dataName])
                    },
                    error: function() {
                        throw exception;
                    }
                })
            },
            render: function(t) {
                if (t.data && t.data.length >= 0) {
                    var r = u.attr("ref")
                      , e = f + "_radiobox"
                      , i = "";
                    n.each(t.data, function(n) {
                        var u = t.data[n];
                        i += "<label class='radio-box'>";
                        i += "<input type='radio' name='" + e + "' value='" + u[t.key] + "' ref='" + r + "' /> " + u[t.value];
                        i += "<\/label>";
                        u.IsDefault == 1 && (t.default = u[t.key])
                    });
                    u.append(i)
                }
                t.default != undefined && u.ysRadioBox("setValue", t.default)
            }
        },
        e.loadData(),
        e.render(r),
        u)
    }
    ;
    n.fn.ysRadioBox.methods = {
        getValue: function(t) {
            var i = "";
            return n(t).find("div.checked").each(function(t, r) {
                i += n(r).find("input[type=radio]").val();
                i += ","
            }),
            i.indexOf(",") >= 0 && (i = i.substring(0, i.length - 1)),
            i
        },
        setValue: function(t, i) {
            if (!ys.isNullOrEmpty(i)) {
                typeof i != "string" && (i = i.toString());
                n(t).find("div").each(function(t, i) {
                    n(i).removeClass("checked")
                });
                var r = i.split(",");
                n.each(r, function(i, r) {
                    var u = n(t).find("input[type=radio][value=" + r + "]");
                    u.attr("checked", !0);
                    u.parent().addClass("checked")
                })
            }
        }
    };
    n.fn.ysCheckBox = function(t, i) {
        var u, f, r, e;
        return typeof t == "string" ? n.fn.ysCheckBox.methods[t](this, i) : (u = n(this),
        f = u.attr("id"),
        !f) ? !1 : (r = n.extend({
            url: null,
            key: "Key",
            value: "Value",
            data: null,
            dataName: "Data",
            "default": undefined
        }, t),
        e = {
            loadData: function() {
                r.url && (n.ajax({
                    url: r.url,
                    type: "get",
                    dataType: "json",
                    "async": !1,
                    cache: !1,
                    success: function(n) {
                        r.data = n
                    },
                    error: function() {
                        throw exception;
                    }
                }),
                r.dataName && r.data != null && (r.data = r.data[r.dataName]))
            },
            render: function(t) {
                if (t.data && t.data.length >= 0) {
                    var r = f + "_checkbox"
                      , i = "";
                    n.each(t.data, function(n) {
                        var u = t.data[n];
                        i += "<label class='check-box'>";
                        i += "<input name='" + r + "' type='checkbox' value='" + u[t.key] + "'>" + u[t.value] + "<\/input>";
                        i += "<\/label>";
                        u.IsDefault == 1 && (t.default = u[t.key])
                    });
                    u.append(i)
                }
                t.default != undefined && u.ysCheckBox("setValue", t.default)
            }
        },
        e.loadData(),
        e.render(r),
        u)
    }
    ;
    n.fn.ysCheckBox.methods = {
        getValue: function(t) {
            var i = "";
            return n(t).find("div.checked").each(function(t, r) {
                i += n(r).find("input[type=checkbox]").val();
                i += ","
            }),
            i.indexOf(",") >= 0 && (i = i.substring(0, i.length - 1)),
            i
        },
        setValue: function(t, i) {
            if (!ys.isNullOrEmpty(i)) {
                typeof i != "string" && (i = i.toString());
                var r = i.split(",");
                n.each(r, function(i, r) {
                    var u = n(t).find("input[type=checkbox][value=" + r + "]");
                    u.attr("checked", !0);
                    u.parent().addClass("checked")
                })
            }
        }
    };
    n.fn.ysComboBox = function(t, i) {
        var u, f, r, e;
        return typeof t == "string" ? n.fn.ysComboBox.methods[t](this, i) : (u = n(this),
        f = u.attr("id"),
        !f) ? !1 : (r = n.extend({
            url: null,
            key: "Key",
            value: "Value",
            maxHeight: "160px",
            width: 180,
            "class": null,
            multiple: !1,
            data: null,
            dataName: "Data",
            onChange: null,
            "default": undefined
        }, t),
        e = {
            loadData: function() {
                r.url && (n.ajax({
                    url: r.url,
                    type: "get",
                    dataType: "json",
                    "async": !1,
                    cache: !1,
                    success: function(n) {
                        r.data = n
                    },
                    error: function() {
                        throw exception;
                    }
                }),
                r.dataName && r.data != null && (r.data = r.data[r.dataName]))
            },
            render: function(t) {
                var i, o, e, s, r, h;
                t.data && t.data.length >= 0 && (i = f + "_select",
                o = "",
                t.multiple && (o = 'multiple=""'),
                e = "<select id='" + i + "' name='" + i + "' class='" + ys.toString(t.class) + " select2' " + o + ">",
                s = n("#" + i).length > 0,
                s && n("#" + i).empty(),
                r = "",
                h = !1,
                t.data.length > 0 && (h = t.data[0][t.value]instanceof Array),
                !h,
                n.each(t.data, function(i) {
                    var u = t.data[i];
                    typeof u == "string" ? r += "<option value='" + u + "'>" + u + "<\/option>" : u[t.value]instanceof Array ? (r += "<optgroup label='--" + u[t.key] + "--'>",
                    n.each(u[t.value], function(n) {
                        var i = u[t.value][n];
                        r += "<option value='" + i[t.key] + "'>" + i[t.value] + "<\/option>";
                        u.IsDefault == 1 && (t.default = u[t.key])
                    })) : (r += "<option value='" + u[t.key] + "'>" + u[t.value] + "<\/option>",
                    u.IsDefault == 1 && (t.default = u[t.key]))
                }),
                s ? n("#" + i).append(r) : (e += r,
                e += "<\/select>",
                u.append(e),
                t.onChange && n("#" + i).change(t.onChange)),
                n("#" + i).select2({
                    minimumResultsForSearch: Infinity
                }),
                t.class || n("#" + f).find(".select2-container").width(t.width),
                t.default != undefined && u.ysComboBox("setValue", t.default))
            }
        },
        e.loadData(),
        e.render(r),
        u)
    }
    ;
    n.fn.ysComboBox.methods = {
        getValue: function(t) {
            var i = n("#" + n(t).attr("id") + "_select").select2("val");
            return i == null ? "" : i.toString()
        },
        setValue: function(t, i) {
            ys.isNullOrEmpty(i) || (typeof i != "string" && (i = i.toString()),
            n("#" + n(t).attr("id") + "_select").val(i.split(",")).trigger("change"))
        }
    };
    n.fn.getWebControls = function(t) {
        var i = {};
        return t && typeof t == "object" && (i = t),
        n(this).find("[col]").each(function(t, r) {
            var f = n(r).attr("id")
              , u = n(r).attr("col");
            r.tagName == "INPUT" ? r.type == "checkbox" ? n(r).prop("checked") && (i[u] = i[u] ? i[u] + "," + n(r).val() : n(r).val()) : r.type == "radio" ? n(r).prop("checked") && (i[u] = n(r).val()) : i[u] = n(r).val() : r.tagName == "SELECT" ? i[u] = n(r).val() : r.tagName == "DIV" ? i[u] = n(r).find("#" + f + "_tree").length > 0 ? n(r).ysComboBoxTree("getValue") : n(r).find("#" + f + "_select").length > 0 ? n(r).ysComboBox("getValue") : n(r).find("input[type=checkbox]").length > 0 ? n(r).ysCheckBox("getValue") : n(r).find("input[type=radio]").length > 0 ? n(r).ysRadioBox("getValue") : n(r).html() : r.tagName == "IMG" ? i[u] = n(r).prop("src") : r.tagName == "SPAN" ? i[u] = n(r).find("#" + f + "_select").length > 0 ? n(r).ysComboBox("getValue") : n(r).html() : r.tagName == "TEXTAREA" && (i[u] = n(r).val())
        }),
        i
    }
    ;
    n.fn.setWebControls = function(t) {
        return n(this).find("[col]").each(function(i, r) {
            var f = n(r).attr("id")
              , u = n(r).attr("col");
            r.tagName == "INPUT" ? r.type == "checkbox" ? n(r).val() == t[u] ? n(r).prop("checked", "checked") : t[u] == !0 && n(r).prop("checked", "checked") : r.type == "radio" ? n(r).val() == t[u] && (n(r).iCheck ? n(r).iCheck("check") : n(r).prop("checked", !0)) : n(r).val(t[u]) : r.tagName == "SELECT" ? n(r).val(t[u]) : r.tagName == "DIV" ? n(r).find("#" + f + "_tree").length > 0 ? n(r).ysComboBoxTree("setValue", t[u]) : n(r).find("#" + f + "_select").length > 0 ? n(r).ysComboBox("setValue", t[u]) : n(r).find("input[type=checkbox]").length > 0 ? n(r).ysCheckBox("setValue", t[u]) : n(r).find("input[type=radio]").length > 0 ? n(r).ysRadioBox("setValue", t[u]) : n(r).html(t[u]) : r.tagName == "SPAN" ? n(r).html(t[u]) : r.tagName == "TEXTAREA" && n(r).val(t[u])
        }),
        t
    }
    ;
    n.fn.autoHeight = function() {
        function t(n) {
            n.style.height = "auto";
            n.scrollTop = 0;
            n.style.height = n.scrollHeight + "px"
        }
        this.each(function() {
            t(this);
            n(this).on("keyup", function() {
                t(this)
            })
        })
    }
}(window.jQuery);
