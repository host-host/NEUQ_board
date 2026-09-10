const AUTH_NEXT_KEY = 'login_next';
let authReturnPath = '';
try {
    authReturnPath = sessionStorage.getItem(AUTH_NEXT_KEY) || '';
} catch (_) {}

function authCurrentPath() {
    return window.location.pathname + window.location.search + window.location.hash;
}

function authValidReturnPath(value) {
    if (!value) return '';
    try {
        const url = new URL(value, window.location.origin);
        if (url.origin !== window.location.origin || url.pathname.startsWith('//')) return '';
        // 登录、注册及 API 地址不作为登录后的返回页面。
        if (/^\/(login|reg|api|v1|v1beta)(\/|$)/i.test(url.pathname)) return '';
        return url.pathname + url.search + url.hash;
    } catch (_) {
        return '';
    }
}

function authSafeReturnPath(value) {
    return authValidReturnPath(value) || '/';
}

function authSaveReturnPath(value = authCurrentPath()) {
    const path = authValidReturnPath(value);
    if (!path) return;
    authReturnPath = path;
    try {
        sessionStorage.setItem(AUTH_NEXT_KEY, path);
    } catch (_) {}
}

function authConsumeReturnPath() {
    let value = authReturnPath;
    authReturnPath = '';
    try {
        value = sessionStorage.getItem(AUTH_NEXT_KEY) || value;
        sessionStorage.removeItem(AUTH_NEXT_KEY);
    } catch (_) {}
    return authSafeReturnPath(authValidReturnPath(value) || document.referrer);
}

if (authValidReturnPath(authCurrentPath())) {
    authSaveReturnPath();
} else {
    const referrer = authValidReturnPath(document.referrer);
    // 来源页面优先于旧记录；同一地址保留记录中的锚点。
    if (referrer && referrer.split('#')[0] !== authValidReturnPath(authReturnPath).split('#')[0]) {
        authSaveReturnPath(referrer);
    }
}

window.addEventListener('pageshow', () => authSaveReturnPath());
window.addEventListener('pagehide', () => authSaveReturnPath());
window.addEventListener('hashchange', () => authSaveReturnPath());
window.addEventListener('popstate', () => authSaveReturnPath());
document.addEventListener('click', event => {
    const link = event.target.closest?.('a[href]');
    if (!link) return;
    const url = new URL(link.href, window.location.href);
    if (url.origin === window.location.origin && /^\/(login|reg)(\/|$)/i.test(url.pathname)) {
        authSaveReturnPath();
    }
}, true);
