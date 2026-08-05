const accountContent = document.getElementById('accountContent');
const loadingState = document.getElementById('loadingState');
const loginState = document.getElementById('loginState');
const errorState = document.getElementById('errorState');
let accountData = null;
let modelConfig = null;
let stabilityRequestId = 0;
const STABILITY_BUCKETS = 3 * 24 * 4;
const STABILITY_BUCKET_MS = 15 * 60 * 1000;

function formatTokens(value) {
    return new Intl.NumberFormat('zh-CN').format(Math.max(0, Math.floor(Number(value) || 0)));
}

function showState(element) {
    [accountContent, loadingState, loginState, errorState].forEach(item => { item.hidden = item !== element; });
}

async function loadTokenAccount() {
    try {
        const [response, modelResponse] = await Promise.all([
            fetch('/api/gpt5_apikey', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: '{}'}),
            fetch('/api/gpt5_model_list', {method: 'POST'})
        ]);
        const text = await response.text();
        let data;
        try { data = text ? JSON.parse(text) : {}; } catch (_) { throw new Error(text || '无法读取账户信息'); }
        const message = data?.error?.message || text;
        if (!response.ok || data?.error?.message) {
            if (response.status === 401 || /log in/i.test(message)) return showState(loginState);
            throw new Error(data?.error?.message || `请求失败（HTTP ${response.status}）`);
        }
        if (!modelResponse.ok) throw new Error(`模型列表加载失败（HTTP ${modelResponse.status}）`);
        modelConfig = await modelResponse.json();
        accountData = data;
        if (typeof data.api_key !== 'string' || !data.api_key.startsWith('sk-')) throw new Error('服务器没有返回有效的 API Key');
        const used = Number(data.token_used) || 0;
        const limit = Number(data.token_limit) || 0;
        document.getElementById('tokenUsed').textContent = formatTokens(used);
        document.getElementById('tokenLimit').textContent = formatTokens(limit);
        document.getElementById('tokenRemaining').textContent = formatTokens(Math.max(0, limit - used));
        document.getElementById('apiKeyValue').value = data.api_key;
        renderProviderChoices();
        showState(accountContent);
        loadProviderStability();
    } catch (error) {
        errorState.textContent = error.message || '加载账户信息失败';
        showState(errorState);
    }
}

function formatMultiply(value) {
    return Number(value.toFixed(6)).toString();
}

function createProviderStability() {
    const element = document.createElement('div');
    element.className = 'provider-stability';
    element.innerHTML = '<div class="provider-stability-bars"></div><div class="provider-stability-axis"><span>24h前</span><span>现在</span></div>';
    const bars = element.firstElementChild;
    for (let i = 0; i < 12; i++) {
        const bar = document.createElement('span');
        bar.className = 'provider-stability-bar';
        bar.title = '成功率 100%';
        bars.appendChild(bar);
    }
    return element;
}

function renderProviderChoices() {
    const container = document.getElementById('providerChoices');
    container.innerHTML = '';
    for (const [model, config] of Object.entries(modelConfig?.model || {})) {
        const providerIds = config?.provider;
        if (!Array.isArray(providerIds)) continue;
        const providers = providerIds.filter(id => {
            const provider = modelConfig.provider?.[id];
            return provider && (accountData.admin === true || provider.public === true);
        });
        if (providers.length === 0) continue;
        const row = document.createElement('label');
        row.className = 'provider-choice';
        const name = document.createElement('span');
        name.className = 'provider-model';
        name.textContent = model;
        const select = document.createElement('select');
        select.setAttribute('aria-label', `${model} Provider`);
        select.dataset.model = model;
        const modelMultiply = Number(config.price?.[0]) || 0;
        providers.forEach(id => {
            const option = document.createElement('option');
            const providerMultiply = Number(modelConfig.provider[id].multiply) || 0;
            const chargedMultiply = modelMultiply * 0.5 * providerMultiply / 0.3;
            option.value = id;
            option.textContent = `${id} · ${formatMultiply(chargedMultiply)}x`;
            select.appendChild(option);
        });
        select.value = providers.includes(accountData.selected_provider?.[model])
            ? accountData.selected_provider[model] : providers[0];
        select.addEventListener('change', () => saveProvider(model, select));
        row.append(name, select, createProviderStability());
        container.appendChild(row);
    }
    if (!container.children.length) container.innerHTML = '<div class="provider-choice"><span class="provider-model">暂无可选 Provider</span></div>';
}

async function saveProvider(model, select) {
    const status = document.getElementById('providerStatus');
    status.className = 'provider-status';
    status.textContent = '正在保存...';
    select.disabled = true;
    try {
        const response = await fetch('/api/gpt5_apikey', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({model, provider: select.value})
        });
        const data = await response.json();
        if (!response.ok || data?.error?.message) throw new Error(data?.error?.message || `保存失败（HTTP ${response.status}）`);
        accountData = data;
        status.textContent = '已保存';
        loadProviderStability();
    } catch (error) {
        status.className = 'provider-status error';
        status.textContent = error.message || '保存失败';
        renderProviderChoices();
        loadProviderStability();
    } finally {
        select.disabled = false;
    }
}

function renderStability(row, raw) {
    const values = Array.isArray(raw) ? raw : [];
    const now = Math.floor(Date.now() / STABILITY_BUCKET_MS);
    const bars = row.querySelectorAll('.provider-stability-bar');
    for (let group = 0; group < 12; group++) {
        let stable = 0, total = 0;
        for (let offset = 0; offset < 8; offset++) {
            const bucket = now - 95 + group * 8 + offset;
            const index = ((bucket % STABILITY_BUCKETS) + STABILITY_BUCKETS) % STABILITY_BUCKETS;
            stable += Number(values[index * 2]) || 0;
            total += Number(values[index * 2 + 1]) || 0;
        }
        const rate = total ? stable / total * 100 : 100;
        const bar = bars[group];
        bar.style.height = total && rate === 0 ? '2px' : `${rate}%`;
        bar.className = `provider-stability-bar ${total ? rate < 20 ? 'bad' : rate < 50 ? 'warn' : 'good' : ''}`.trim();
        bar.title = `成功率 ${rate === 100 ? '100' : rate.toFixed(1)}%`;
    }
}

async function loadProviderStability() {
    const requestId = ++stabilityRequestId;
    const rows = [...document.querySelectorAll('#providerChoices .provider-choice')]
        .map(row => {
            const select = row.querySelector('select[data-model]');
            return select ? {row, select, key: `${select.dataset.model}_${select.value}`} : null;
        })
        .filter(Boolean);
    if (!rows.length) return;
    const keys = rows.map(({key}) => key);
    try {
        const response = await fetch('/api/gpt5_askstable', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(keys)
        });
        const data = await response.json();
        if (!response.ok || data?.error || requestId !== stabilityRequestId) return;
        rows.forEach(({row, select, key}) => {
            const currentKey = `${select.dataset.model}_${select.value}`;
            if (currentKey === key) renderStability(row, data[key]);
        });
    } catch (_) {}
}

document.getElementById('copyApiKeyButton').addEventListener('click', async () => {
    const input = document.getElementById('apiKeyValue');
    try { await navigator.clipboard.writeText(input.value); }
    catch (_) { input.select(); document.execCommand('copy'); }
    const button = document.getElementById('copyApiKeyButton');
    button.title = '已复制';
    setTimeout(() => { button.title = '复制 API Key'; }, 1500);
});

document.getElementById('rotateApiKeyButton').addEventListener('click', async () => {
    if (!confirm('重置后，之前的 API Key 将立即失效。确定要继续吗？')) return;
    const button = document.getElementById('rotateApiKeyButton');
    const status = document.getElementById('apiKeyStatus');
    button.disabled = true;
    status.className = 'api-key-status';
    status.textContent = '正在重置...';
    try {
        const response = await fetch('/api/gpt5_apikey', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({rotate: true})
        });
        const text = await response.text();
        let data;
        try { data = text ? JSON.parse(text) : {}; }
        catch (_) { throw new Error(text || '无法读取服务器响应'); }
        if (!response.ok || data?.error?.message) {
            throw new Error(data?.error?.message || `重置失败（HTTP ${response.status}）`);
        }
        if (typeof data.api_key !== 'string' || !data.api_key.startsWith('sk-')) {
            throw new Error('服务器没有返回有效的 API Key');
        }
        accountData = data;
        document.getElementById('apiKeyValue').value = data.api_key;
        status.textContent = 'API Key 已重置，旧 Key 已失效';
    } catch (error) {
        status.className = 'api-key-status error';
        status.textContent = error.message || '重置失败';
    } finally {
        button.disabled = false;
    }
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
