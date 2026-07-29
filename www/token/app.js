const accountContent = document.getElementById('accountContent');
const loadingState = document.getElementById('loadingState');
const loginState = document.getElementById('loginState');
const errorState = document.getElementById('errorState');

function formatTokens(value) {
    return new Intl.NumberFormat('zh-CN').format(Math.max(0, Math.floor(Number(value) || 0)));
}

function showState(element) {
    [accountContent, loadingState, loginState, errorState].forEach(item => { item.hidden = item !== element; });
}

async function loadTokenAccount() {
    try {
        const response = await fetch('/api/gpt5_apikey', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: '{}'});
        const text = await response.text();
        let data;
        try { data = text ? JSON.parse(text) : {}; } catch (_) { throw new Error(text || '无法读取账户信息'); }
        const message = data?.error?.message || text;
        if (!response.ok || data?.error?.message) {
            if (response.status === 401 || /log in/i.test(message)) return showState(loginState);
            throw new Error(data?.error?.message || `请求失败（HTTP ${response.status}）`);
        }
        if (typeof data.api_key !== 'string' || !data.api_key.startsWith('sk-')) throw new Error('服务器没有返回有效的 API Key');
        const used = Number(data.token_used) || 0;
        const limit = Number(data.token_limit) || 0;
        document.getElementById('tokenUsed').textContent = formatTokens(used);
        document.getElementById('tokenLimit').textContent = formatTokens(limit);
        document.getElementById('tokenRemaining').textContent = formatTokens(Math.max(0, limit - used));
        document.getElementById('apiKeyValue').value = data.api_key;
        showState(accountContent);
    } catch (error) {
        errorState.textContent = error.message || '加载账户信息失败';
        showState(errorState);
    }
}

document.getElementById('copyApiKeyButton').addEventListener('click', async () => {
    const input = document.getElementById('apiKeyValue');
    try { await navigator.clipboard.writeText(input.value); }
    catch (_) { input.select(); document.execCommand('copy'); }
    const button = document.getElementById('copyApiKeyButton');
    button.title = '已复制';
    setTimeout(() => { button.title = '复制 API Key'; }, 1500);
});

document.getElementById('toggleApiKeyButton').addEventListener('click', () => {
    const input = document.getElementById('apiKeyValue');
    const button = document.getElementById('toggleApiKeyButton');
    const showing = input.type === 'text';
    input.type = showing ? 'password' : 'text';
    button.setAttribute('aria-pressed', String(!showing));
    button.setAttribute('aria-label', showing ? '显示 API Key' : '隐藏 API Key');
    button.title = showing ? '显示 API Key' : '隐藏 API Key';
});

loadTokenAccount();
