const AUTH_NEXT_KEY = 'login_next';

function authCurrentPath() {
    return window.location.pathname + window.location.search + window.location.hash;
}

function authSaveReturnPath() {
    sessionStorage.setItem(AUTH_NEXT_KEY, authCurrentPath());
}

function authSafeReturnPath(value) {
    if (!value) return '/';

    try {
        const url = new URL(value, window.location.origin);
        if (url.origin !== window.location.origin) return '/';
        return url.pathname + url.search + url.hash;
    } catch (_) {
        return '/';
    }
}

function authConsumeReturnPath() {
    const value = sessionStorage.getItem(AUTH_NEXT_KEY);
    sessionStorage.removeItem(AUTH_NEXT_KEY);
    return authSafeReturnPath(value);
}
