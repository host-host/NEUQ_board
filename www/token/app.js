(() => {
    'use strict';

    const $ = id => document.getElementById(id);
    const pages = {
        overview: ['概览', 'OVERVIEW', '用量、余额与最新动态，一目了然。'],
        credentials: ['API Key', 'API KEY', '管理你的密钥，连接应用。'],
        models: ['模型广场', 'MODELS', '浏览模型价格与服务表现，选择合适的 Provider。'],
        history: ['历史记录', 'REQUEST HISTORY', '回顾每一次 API 调用，了解用量与响应表现。']
    };
    const state = {
        page: '', account: null, models: null, accountStatus: 'loading', modelStatus: 'loading',
        accountError: '', modelError: '', modelType: 'text', pending: 0, saving: false,
        accountRequest: 0, modelRequest: 0, stabilityRequest: 0, userRequest: 0, loggingOut: false,
        logRequest: 0, logPage: 1, logNext: false, logs: [], logLoaded: false, logLoading: false
    };
    const numberFormat = new Intl.NumberFormat('zh-CN');
    const CNY_PER_MILLION_CREDITS = 0.3;
    const CNY_PER_USD = 7;
    const moneyFormat = new Intl.NumberFormat('zh-CN', {minimumFractionDigits: 2, maximumFractionDigits: 7});
    const balanceFormat = new Intl.NumberFormat('zh-CN', {minimumFractionDigits: 2, maximumFractionDigits: 2});
    const STABILITY_BUCKETS = 3 * 24 * 4;
    const STABILITY_BUCKET_MS = 15 * 60 * 1000;
    let toastTimer;
    let searchTimer;
    let tokenTooltipTimer;
    let tokenTooltipTarget;
    let modelLogTooltipTimer;
    let modelLogTooltipTarget;
    let pricePopoverTimer;
    let pricePopoverTarget;
    let pricePopoverPinned = false;
    const priceFormats = new Set(['per', 'token', 'normal', 'deepseek', 'lengthdouble', 'claude']);
    const officialPriceFormat = new Intl.NumberFormat('zh-CN', {maximumFractionDigits: 10});
    const summaryPriceFormat = new Intl.NumberFormat('zh-CN', {maximumSignificantDigits: 3});
    const calculationPriceFormat = new Intl.NumberFormat('zh-CN', {maximumSignificantDigits: 6});

    function numeric(value) {
        const number = Number(value);
        return Number.isFinite(number) && number >= 0 ? number : 0;
    }

    function tokens(value) { return numberFormat.format(Math.floor(numeric(value))); }
    function creditsToYuan(value) { return numeric(value) / 1000000 * CNY_PER_MILLION_CREDITS; }
    function money(value, summary = false) {
        const amount = numeric(value);
        if (amount > 0 && amount < 0.0000001) return '<¥0.0000001';
        const format = summary && amount >= 0.01 ? balanceFormat : moneyFormat;
        return `¥${format.format(amount)}`;
    }
    function logUnitPrice(log) {
        if (!Number.isFinite(log.multiply)) return NaN;
        // 历史记录和新记录均保存扣费额度；换算为元/次或元/百万 token。
        return log.multiply * CNY_PER_MILLION_CREDITS / (isImage(log) ? 1000000 : 1);
    }
    function logPrice(log) {
        const itemized = Number(log.isimage) === 2;
        if (itemized ? !Number.isFinite(log.inputm) || !Number.isFinite(log.outputm) || !Number.isFinite(log.cachem) : !Number.isFinite(log.multiply)) return '—';
        return providerPrice({format: itemized ? 'normal' : isImage(log) ? 'per' : 'token',
            input: itemized ? log.inputm : logUnitPrice(log), output: log.outputm, cache: log.cachem}, true);
    }
    function isImage(log) { return log?.isimage === true || Number(log?.isimage) === 1; }
    function logCharge(log) {
        return Number.isFinite(log.useage) ? money(creditsToYuan(log.useage)) : '—';
    }
    function providerName(log) {
        const name = Number(log.isauto) === 1 ? `auto (${log.provider || '—'})` : log.provider || '—';
        return Number(log.isswitch) === 1 ? `${name}（已切换）` : name;
    }
    function duration(value, allowZero = false) {
        if (value == null) return '—';
        const seconds = Number(value);
        return Number.isFinite(seconds) && seconds >= 0 && (allowZero || seconds > 0) && seconds <= 1e6 ? Number(seconds.toFixed(2)).toString() : '—';
    }
    function tps(log) {
        return numeric(log.total) > 0 && numeric(log.total) <= 1e6 && numeric(log.output) > 0
            ? Number((numeric(log.output) / numeric(log.total)).toFixed(2)).toString() : '—';
    }
    function timeParts(value) {
        const date = new Date(Number(value) * 1000);
        if (!Number.isFinite(date.getTime())) return ['—', ''];
        return [date.toLocaleDateString('zh-CN', {year: 'numeric', month: '2-digit', day: '2-digit'}),
            date.toLocaleTimeString('zh-CN', {hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit'})];
    }
    function node(tag, className, text) {
        const element = document.createElement(tag);
        if (className) element.className = className;
        if (text !== undefined) element.textContent = text;
        return element;
    }
    function toast(message, error = false) {
        clearTimeout(toastTimer);
        $('toast').textContent = message;
        $('toast').className = `toast${error ? ' error' : ''}`;
        $('toast').hidden = false;
        toastTimer = setTimeout(() => { $('toast').hidden = true; }, error ? 5000 : 2800);
    }

    async function api(path, body, method = 'POST') {
        const controller = new AbortController();
        const timeout = setTimeout(() => controller.abort(), 20000);
        try {
            const options = {method, credentials: 'same-origin', cache: 'no-store', signal: controller.signal};
            if (body !== undefined) {
                options.headers = {'Content-Type': 'application/json'};
                options.body = JSON.stringify(body);
            }
            const response = await fetch(path, options);
            const text = await response.text();
            let data;
            try { data = JSON.parse(text); } catch (_) { data = null; }
            const message = data?.error?.message || (typeof data?.error === 'string' ? data.error : '');
            const authMessage = message || (typeof data === 'string' ? data : data === null ? text.slice(0, 120) : '');
            if (response.status === 401 || /log\s*in|not.logged.in/i.test(authMessage)) {
                throw Object.assign(new Error('登录已失效，请重新登录。'), {auth: true});
            }
            if (!response.ok || data?.error) throw new Error(message || `请求失败（HTTP ${response.status}）`);
            if (data === null) throw new Error('服务器响应格式异常，请稍后重试。');
            return data;
        } catch (error) {
            if (error.name === 'AbortError') throw new Error('请求超时，请重试。');
            if (error instanceof TypeError) throw new Error('网络连接失败，请重试。');
            throw error;
        } finally {
            clearTimeout(timeout);
        }
    }

    function updateRefresh() {
        $('refreshButton').disabled = state.pending > 0 || state.saving;
        $('refreshButton').classList.toggle('is-loading', state.pending > 0);
        $('refreshButton').setAttribute('aria-busy', String(state.pending > 0));
    }
    async function busy(action) {
        state.pending++;
        updateRefresh();
        try { return await action(); }
        finally { state.pending--; updateRefresh(); }
    }
    function updated() {
        $('updatedAt').textContent = `最近更新 ${new Date().toLocaleTimeString('zh-CN', {hour12: false})}`;
    }
    function showMessage(id, message, {loading = false, error = false, login = false, retry} = {}) {
        const element = $(id);
        element.replaceChildren();
        element.classList.toggle('error', error);
        if (loading) element.append(node('span', 'loading-icon'));
        element.append(node('span', '', message));
        if (login) {
            const link = node('a', 'button button-primary', '登录账户');
            link.href = '/login';
            element.append(link);
        } else if (retry) {
            const button = node('button', 'button button-white', '重试');
            button.type = 'button';
            button.addEventListener('click', retry);
            element.append(button);
        }
        element.hidden = false;
    }
    function setGuest() {
        setAccountMenu(false);
        $('accountMenu').hidden = true;
        $('userLink').hidden = false;
        $('userName').textContent = '登录';
        $('userAvatar').textContent = 'N';
        $('welcomeTitle').textContent = '欢迎来到控制台';
    }
    function expireSession() {
        state.account = null;
        state.accountStatus = 'login';
        state.userRequest++;
        state.accountRequest++;
        state.stabilityRequest++;
        state.logRequest++;
        state.logs = [];
        state.logLoaded = false;
        state.logNext = false;
        $('logRows').replaceChildren();
        $('logTable').hidden = true;
        $('exportLogs').disabled = true;
        $('recordSummary').textContent = '登录后查看 API 调用记录';
        hideTokenDetail();
        $('tokenDetailList').replaceChildren();
        setGuest();
        renderAccount();
        renderProviders();
        showMessage('historyState', '登录后查看 API 调用日志。', {login: true});
        updatePagination();
    }

    async function loadUser() {
        const id = ++state.userRequest;
        try {
            const user = await api('/api/user', undefined, 'GET');
            if (id !== state.userRequest || state.loggingOut) return;
            if (!user?.name || state.accountStatus === 'login') return setGuest();
            $('userName').textContent = user.name;
            $('userAvatar').textContent = [...String(user.name)][0].toUpperCase();
            $('userMenuToggle').setAttribute('aria-label', `${user.name}，账户菜单`);
            $('userLink').hidden = true;
            $('accountMenu').hidden = false;
            $('welcomeTitle').textContent = `欢迎回来，${user.name}`;
        } catch (_) {
            if (id === state.userRequest && !state.loggingOut && !state.account) setGuest();
        }
    }
    function setAccountMenu(open) {
        const visible = open && !$('accountMenu').hidden;
        $('accountDropdown').hidden = !visible;
        $('userMenuToggle').setAttribute('aria-expanded', String(visible));
    }
    async function logout() {
        if (state.saving || state.loggingOut) return;
        state.loggingOut = true;
        state.userRequest++;
        setSaving(true);
        $('logoutButton').querySelector('span').textContent = '正在注销…';
        const controller = new AbortController();
        const timeout = setTimeout(() => controller.abort(), 20000);
        try {
            const response = await fetch('/api/logout', {method: 'POST', credentials: 'same-origin', signal: controller.signal});
            if (!response.ok) throw new Error(`注销失败（HTTP ${response.status}）`);
            expireSession();
            location.reload();
        } catch (error) {
            toast(error.name === 'AbortError' ? '注销请求超时，请重试。' : error.message || '注销失败，请重试。', true);
        } finally {
            clearTimeout(timeout);
            state.loggingOut = false;
            setSaving(false);
            $('logoutButton').querySelector('span').textContent = '注销登录';
        }
    }
    async function loadAccount() {
        const id = ++state.accountRequest;
        state.accountStatus = 'loading';
        renderAccount();
        renderProviders();
        await busy(async () => {
            try {
                const data = await api('/api/gpt5_apikey', {});
                if (id !== state.accountRequest) return;
                if (typeof data.api_key !== 'string' || !data.api_key.startsWith('sk-')) throw new Error('服务器没有返回有效的 API Key。');
                state.account = data;
                state.accountStatus = 'ready';
                updated();
            } catch (error) {
                if (id !== state.accountRequest) return;
                if (error.auth) return expireSession();
                state.account = null;
                state.accountStatus = 'error';
                state.accountError = error.message;
            }
            renderAccount();
            renderProviders();
        });
    }
    function renderAccount() {
        const ready = state.accountStatus === 'ready';
        ['toggleApiKey', 'copyApiKey', 'rotateApiKey'].forEach(id => { $(id).disabled = !ready || state.saving; });
        $('apiKeyValue').value = ready ? state.account.api_key : '';
        $('apiKeyValue').type = 'password';
        $('toggleApiKey').setAttribute('aria-pressed', 'false');
        $('toggleApiKey').setAttribute('aria-label', '显示 API Key');
        $('toggleApiKey').title = '显示 API Key';
        for (const id of ['overviewState', 'credentialsState']) {
            $(id).hidden = ready;
            if (state.accountStatus === 'loading') showMessage(id, '正在加载账户信息…', {loading: true});
            if (state.accountStatus === 'login') showMessage(id, id === 'credentialsState' ? '登录后管理 API Key。' : '登录后查看用量与余额。', {login: true});
            if (state.accountStatus === 'error') showMessage(id, state.accountError, {error: true, retry: loadAccount});
        }
        const used = ready ? numeric(state.account.token_used) : 0;
        const limit = ready ? numeric(state.account.token_limit) : 0;
        const remaining = Math.max(0, limit - used);
        [['tokenUsed', used], ['tokenLimit', limit], ['tokenRemaining', remaining]].forEach(([id, value]) => {
            $(id).textContent = ready ? money(creditsToYuan(value), true) : '—';
            $(id).title = ready ? money(creditsToYuan(value)) : '';
        });
        $('quotaUsed').textContent = ready ? money(creditsToYuan(used)) : '—';
        $('quotaRemaining').textContent = ready ? money(creditsToYuan(remaining)) : '—';
        const percent = limit > 0 ? used / limit * 100 : used > 0 ? 100 : 0;
        $('usagePercent').textContent = ready ? `${Number(percent.toFixed(1))}%` : '—';
        $('quotaFill').style.width = `${Math.min(100, percent)}%`;
        $('quotaProgress').classList.toggle('exhausted', ready && remaining === 0);
        if (ready) $('quotaProgress').setAttribute('aria-valuenow', String(Math.min(100, percent)));
        else $('quotaProgress').removeAttribute('aria-valuenow');
        $('quotaNote').textContent = ready && remaining === 0 ? '当前余额为 ¥0.00，请留意账户余额。' : '金额单位为人民币';
    }
    async function loadModels() {
        const id = ++state.modelRequest;
        state.modelStatus = 'loading';
        showMessage('noticeState', '正在加载公告…', {loading: true});
        $('noticeList').replaceChildren();
        $('noticeCount').textContent = '—';
        $('allNoticesButton').hidden = true;
        renderProviders();
        await busy(async () => {
            try {
                const data = await api('/api/gpt5_model_list');
                if (id !== state.modelRequest) return;
                if (!data || typeof data !== 'object' || Array.isArray(data)) throw new Error('模型数据格式异常。');
                state.models = data;
                state.modelStatus = 'ready';
                renderNotices(data.notice);
            } catch (error) {
                if (id !== state.modelRequest) return;
                state.models = null;
                state.modelStatus = 'error';
                state.modelError = error.message;
                showMessage('noticeState', `公告加载失败：${error.message}`, {error: true, login: error.auth, retry: loadModels});
            }
            renderProviders();
        });
    }
    function renderNotices(raw) {
        const notices = (Array.isArray(raw) ? raw : []).filter(item => typeof item?.content === 'string' && item.content.trim());
        $('noticeList').replaceChildren();
        $('allNoticeList').replaceChildren();
        notices.forEach((notice, index) => {
            const item = node('li');
            item.append(node('span', 'notice-dot'), node('p', 'notice-content', notice.content));
            if (typeof notice.time === 'string' && notice.time.trim()) item.append(node('span', 'notice-time', notice.time));
            if (index < 3) $('noticeList').append(item.cloneNode(true));
            $('allNoticeList').append(item);
        });
        $('noticeCount').textContent = String(notices.length);
        $('allNoticesButton').hidden = notices.length <= 3;
        $('noticeState').hidden = notices.length > 0;
        if (!notices.length) showMessage('noticeState', '暂无公告，有新的消息会在这里告诉你。');
    }

    function providerConfig(config, id) { return id === 'auto' ? config?.auto : state.models?.provider?.[id]; }
    function availableProviders(config) {
        if (!Array.isArray(config?.provider)) return [];
        const accessible = id => state.models?.provider?.[id] && (state.account?.admin === true || state.models.provider[id].public === true);
        return config.provider.filter(id => id === 'auto'
            ? Array.isArray(config.auto?.provider) && config.auto.provider.some(accessible) : accessible(id));
    }
    function isPriceObject(value) {
        return value !== null && typeof value === 'object' && !Array.isArray(value);
    }
    function mergePrice(base, overrides) {
        const price = isPriceObject(base) ? {...base} : {};
        if (!isPriceObject(overrides)) return price;
        for (const [key, value] of Object.entries(overrides)) {
            price[key] = isPriceObject(price[key]) && isPriceObject(value) ? mergePrice(price[key], value) : value;
        }
        return price;
    }
    function modelProviderPrice(config, id) {
        return mergePrice(config.price, providerConfig(config, id)?.price);
    }
    function priceMultiplier(price, yuan = false) {
        const multiplier = Number.isFinite(price.multiply) && price.multiply !== 0 ? price.multiply : 1;
        return multiplier * (yuan && price.dollar === true ? CNY_PER_USD : 1);
    }
    function priceColumns(price) {
        if (price.format === 'deepseek') {
            return [{label: '非高峰', price, factor: 1}, {label: '高峰', price, factor: 2}];
        }
        if (price.format === 'lengthdouble') {
            const threshold = tokens(price.length);
            return [{label: `≤ ${threshold} tokens`, price, factor: 1},
                {label: `> ${threshold} tokens`, price: price.newprice, factor: 1}];
        }
        return [{price, factor: 1}];
    }
    function providerPrice(price, compact = false) {
        if (!priceFormats.has(price?.format)) return '价格未配置';
        const multiplier = priceMultiplier(price, true);
        const format = amount => compact ? `¥${summaryPriceFormat.format(amount)}` : money(amount);
        if (price.format === 'per' || price.format === 'token') {
            const amount = numeric(price.input) * multiplier;
            return price.format === 'per'
                ? `${format(creditsToYuan(Math.ceil(amount / CNY_PER_MILLION_CREDITS * 1000000)))}/次`
                : `${format(amount)}/M tokens`;
        }
        const columns = priceColumns(price);
        const summary = field => {
            const amounts = columns.map(column => numeric(column.price?.[field]) * column.factor * multiplier);
            return format(Math.min(...amounts));
        };
        return `${summary('input')}/${summary('output')}/${summary('cache')} per 1M`;
    }
    function renderPriceTable(price, content, site = false) {
        content.replaceChildren();
        if (!priceFormats.has(price?.format)) {
            content.append(node('p', 'price-note', '价格未配置'));
            return;
        }
        const currency = !site && price.dollar === true ? '$' : '¥';
        const unit = price.format === 'per' ? `${currency}/次` : `${currency}/M tokens`;
        const multiplier = priceMultiplier(price, site);
        const columns = priceColumns(price);
        let fields = [['input', '输入'], ['output', '输出'], ['cache', '缓存读取'], ['makecache', '缓存写入']];
        let note = '';
        if (price.format === 'per' || price.format === 'token') {
            fields = [['input', price.format === 'per' ? '每次调用' : '总 Token']];
        } else if (price.format === 'deepseek') {
            note = '北京时间周一至周五 9:00–12:00、14:00–18:00 为高峰时段，价格翻倍；节假日和调休均按星期计算。';
        } else if (price.format === 'claude' && !site) {
            fields = [['input', '输入'], ['output', '输出'], ['cache', '缓存读取'],
                ['makecache', '缓存写入（5 分钟）'], ['makecache(1h)', '缓存写入（1 小时）']];
        }
        if (price.format !== 'per' && price.format !== 'token') fields.push(['websearch', '联网搜索']);
        const table = node('table', 'official-price-table');
        table.classList.toggle('tiered-price-table', columns.length > 1);
        table.setAttribute('aria-label', `${site ? '本站价格' : '官方定价'}，${currency === '$' ? '美元' : '人民币'}`);
        if (columns.length > 1) {
            const head = node('thead');
            const row = node('tr');
            const label = node('th', '', price.format === 'lengthdouble' ? '总输入' : '计费项');
            label.scope = 'col';
            row.append(label);
            columns.forEach(column => {
                const cell = node('th', '', column.label);
                cell.scope = 'col';
                row.append(cell);
            });
            head.append(row);
            table.append(head);
        }
        const body = node('tbody');
        fields.forEach(([field, label]) => {
            if ((field.startsWith('makecache') || field === 'websearch') && !columns.some(column => Object.hasOwn(column.price || {}, field))) return;
            const fieldUnit = field === 'websearch' ? `${currency}/次` : unit;
            const row = node('tr');
            const heading = node('th', '', label);
            heading.scope = 'row';
            row.append(heading);
            const amounts = columns.map(column => {
                const value = column.price?.[field];
                return site ? numeric(value) * column.factor * multiplier
                    : typeof value === 'number' ? value * column.factor * multiplier : NaN;
            });
            const sharedPrice = amounts.length > 1 && Number.isFinite(amounts[0]) && amounts[0] >= 0 &&
                amounts.every(amount => amount === amounts[0]);
            (sharedPrice ? amounts.slice(0, 1) : amounts).forEach(amount => {
                const cell = node('td');
                if (sharedPrice) cell.colSpan = columns.length;
                if (Number.isFinite(amount) && amount >= 0) {
                    const price = node('span', 'price-inline');
                    price.append(node('span', '', officialPriceFormat.format(amount)), node('span', 'price-value-unit', fieldUnit));
                    cell.append(price);
                } else cell.textContent = '—';
                row.append(cell);
            });
            body.append(row);
        });
        table.append(body);
        content.append(table);
        if (note) content.append(node('p', 'price-note', note));
    }
    function updateSitePrice() {
        if (!pricePopoverTarget) return;
        const config = state.models?.model?.[pricePopoverTarget.dataset.model];
        const select = pricePopoverTarget.closest('.provider-row')?.querySelector('.provider-select');
        if (!config || !select) return hidePricePopover();
        const price = modelProviderPrice(config, select.value);
        const itemized = priceFormats.has(price.format) && price.format !== 'per' && price.format !== 'token';
        $('sitePriceProvider').textContent = `Provider · ${select.value}`;
        $('sitePriceValue').textContent = providerPrice(price);
        $('sitePriceValue').hidden = itemized;
        $('sitePriceContent').hidden = !itemized;
        $('sitePriceContent').replaceChildren();
        if (itemized) renderPriceTable(price, $('sitePriceContent'), true);
        $('sitePriceBasis').textContent = price.format === 'per' ? '按次计费，金额单位为人民币。'
            : price.format === 'token' ? '按输入与输出的总 Token 数计费。' : '';
        $('sitePriceBasis').hidden = !$('sitePriceBasis').textContent;
    }
    function hidePricePopover() {
        clearTimeout(pricePopoverTimer);
        $('pricePopover').hidden = true;
        pricePopoverTarget?.setAttribute('aria-expanded', 'false');
        pricePopoverTarget = null;
        pricePopoverPinned = false;
    }
    function closePricePopover() {
        // Restore keyboard focus before hiding, so focus does not reopen the panel.
        if ($('pricePopover').contains(document.activeElement)) pricePopoverTarget?.focus({preventScroll: true});
        hidePricePopover();
    }
    function scheduleHidePricePopover() {
        clearTimeout(pricePopoverTimer);
        pricePopoverTimer = setTimeout(() => {
            if (pricePopoverPinned || pricePopoverTarget?.matches(':hover, :focus-visible') ||
                $('pricePopover').matches(':hover, :focus-within')) return;
            hidePricePopover();
        }, 180);
    }
    function showPricePopover(target) {
        const config = state.models?.model?.[target.dataset.model];
        if (!priceFormats.has(config?.price?.format)) return;
        clearTimeout(pricePopoverTimer);
        if (pricePopoverTarget !== target) hidePricePopover();
        hideTokenDetail();
        hideModelLog();
        pricePopoverTarget = target;
        target.setAttribute('aria-expanded', 'true');
        $('pricePopoverTitle').textContent = target.dataset.model;
        updateSitePrice();
        renderPriceTable(config.price, $('officialPriceContent'));
        const popover = $('pricePopover');
        popover.hidden = false;
        const anchor = target.getBoundingClientRect();
        const bounds = popover.getBoundingClientRect();
        const margin = 12;
        const gap = 8;
        const below = anchor.bottom + gap;
        const above = anchor.top - bounds.height - gap;
        const top = below + bounds.height <= innerHeight - margin ? below : above >= margin ? above : below;
        popover.style.left = `${Math.max(margin, Math.min(anchor.left, innerWidth - bounds.width - margin))}px`;
        popover.style.top = `${Math.max(margin, Math.min(top, innerHeight - bounds.height - margin))}px`;
    }
    function createPriceButton(name) {
        const button = node('button', 'model-price-button');
        button.type = 'button';
        button.dataset.model = name;
        button.setAttribute('aria-label', `查看 ${name} 的本站价格与官方定价`);
        button.setAttribute('aria-haspopup', 'dialog');
        button.setAttribute('aria-controls', 'pricePopover');
        button.setAttribute('aria-expanded', 'false');
        const icon = node('span', 'model-price-info', 'i');
        icon.setAttribute('aria-hidden', 'true');
        button.append(icon);
        button.addEventListener('pointerenter', event => { if (event.pointerType === 'mouse') showPricePopover(button); });
        button.addEventListener('pointerleave', scheduleHidePricePopover);
        button.addEventListener('focus', () => { if (button.matches(':focus-visible')) showPricePopover(button); });
        button.addEventListener('blur', scheduleHidePricePopover);
        button.addEventListener('click', () => {
            if (pricePopoverTarget === button && pricePopoverPinned) return hidePricePopover();
            showPricePopover(button);
            pricePopoverPinned = true;
        });
        button.addEventListener('keydown', event => {
            if (event.key === 'ArrowDown' || (event.key === 'Tab' && !event.shiftKey && pricePopoverTarget === button)) {
                event.preventDefault();
                showPricePopover(button);
                pricePopoverPinned = true;
                $('closePricePopover').focus({preventScroll: true});
            }
        });
        return button;
    }
    function hideModelLog() {
        clearTimeout(modelLogTooltipTimer);
        $('modelLogTooltip').hidden = true;
        modelLogTooltipTarget?.removeAttribute('aria-describedby');
        modelLogTooltipTarget = null;
    }
    function scheduleHideModelLog() {
        clearTimeout(modelLogTooltipTimer);
        modelLogTooltipTimer = setTimeout(() => {
            if (!modelLogTooltipTarget?.matches(':hover, :focus-visible') && !$('modelLogTooltip').matches(':hover')) hideModelLog();
        }, 180);
    }
    function showModelLog(target, log) {
        hideModelLog();
        hidePricePopover();
        modelLogTooltipTarget = target;
        target.setAttribute('aria-describedby', 'modelLogTooltip');
        const tooltip = $('modelLogTooltip');
        tooltip.textContent = log;
        tooltip.hidden = false;
        const anchor = target.getBoundingClientRect();
        const bounds = tooltip.getBoundingClientRect();
        const margin = 12;
        const gap = 8;
        const top = anchor.top - bounds.height - gap;
        tooltip.style.left = `${Math.max(margin, Math.min(anchor.right - bounds.width, innerWidth - bounds.width - margin))}px`;
        tooltip.style.top = `${Math.max(margin, Math.min(top >= margin ? top : anchor.bottom + gap, innerHeight - bounds.height - margin))}px`;
    }
    function createModelLogButton(name, log) {
        const button = node('button', 'model-log-button');
        button.type = 'button';
        button.setAttribute('aria-label', `查看 ${name} 的模型说明`);
        const icon = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        icon.classList.add('model-log-icon');
        icon.setAttribute('aria-hidden', 'true');
        const shape = document.createElementNS('http://www.w3.org/2000/svg', 'use');
        shape.setAttribute('href', '#i-alert-triangle');
        icon.append(shape);
        button.append(icon);
        button.addEventListener('pointerenter', event => { if (event.pointerType === 'mouse') showModelLog(button, log); });
        button.addEventListener('pointerleave', scheduleHideModelLog);
        button.addEventListener('focus', () => { if (button.matches(':focus-visible')) showModelLog(button, log); });
        button.addEventListener('blur', scheduleHideModelLog);
        button.addEventListener('click', () => showModelLog(button, log));
        return button;
    }
    function renderProviders() {
        hidePricePopover();
        hideModelLog();
        state.stabilityRequest++;
        const rows = $('providerRows');
        rows.replaceChildren();
        $('providerTable').hidden = true;
        $('modelCount').textContent = '—';
        $('textModelCount').textContent = '0';
        $('imageModelCount').textContent = '0';
        if (state.accountStatus === 'login') return showMessage('providerState', '登录后选择模型的 Provider。', {login: true});
        if (state.accountStatus === 'error') return showMessage('providerState', '账户信息加载失败，暂时无法选择 Provider。', {error: true, retry: loadAccount});
        if (state.modelStatus === 'error') return showMessage('providerState', state.modelError, {error: true, retry: loadModels});
        if (state.accountStatus !== 'ready' || state.modelStatus !== 'ready') return showMessage('providerState', '正在加载模型…', {loading: true});
        const entries = Object.entries(state.models.model || {}).map(([name, config]) => ({name, config, providers: availableProviders(config)})).filter(item => item.providers.length);
        const textCount = entries.filter(item => item.config.suggest_format !== 'image').length;
        $('textModelCount').textContent = String(textCount);
        $('imageModelCount').textContent = String(entries.length - textCount);
        $('modelCount').textContent = String(entries.length);
        const query = $('modelSearch').value.trim().toLocaleLowerCase();
        const visible = entries.filter(item => (item.config.suggest_format === 'image') === (state.modelType === 'image') && item.name.toLocaleLowerCase().includes(query));
        if (!visible.length) return showMessage('providerState', query ? '没有找到匹配的模型，试试其他关键词。' : '暂无可用的模型。');
        $('providerState').hidden = true;
        $('providerTable').hidden = false;
        visible.forEach(({name, config, providers}) => {
            const row = node('div', 'provider-row');
            const info = node('div', 'model-info');
            info.append(node('span', 'model-name', name));
            if (priceFormats.has(config.price?.format)) info.append(createPriceButton(name));
            if (typeof config.log === 'string' && config.log.trim()) info.append(createModelLogButton(name, config.log));
            const select = node('select', 'provider-select');
            select.dataset.model = name;
            select.setAttribute('aria-label', `${name} Provider`);
            providers.forEach(id => {
                const option = node('option', '', `${id} · ${providerPrice(modelProviderPrice(config, id), true)}`);
                option.value = id;
                select.append(option);
            });
            select.value = providers.includes(state.account.selected_provider?.[name]) ? state.account.selected_provider[name] : providers[0];
            select.disabled = state.saving;
            select.addEventListener('change', () => saveProvider(name, select));
            const stability = node('div', 'provider-stability');
            const bars = node('div', 'stability-bars');
            for (let i = 0; i < 12; i++) {
                const bar = node('span', 'stability-bar');
                bar.title = '暂无统计数据';
                bars.append(bar);
            }
            const axis = node('div', 'stability-axis');
            axis.append(node('span', '', '24h 前'), node('span', '', '现在'));
            stability.append(bars, axis);
            const performance = node('dl', 'provider-performance');
            [['延迟 (s)', 'provider-latency'], ['TPS', 'provider-tps']].forEach(([title, className]) => {
                const item = node('div');
                item.append(node('dt', '', title), node('dd', className, '—'));
                performance.append(item);
            });
            row.append(info, select, stability, performance);
            rows.append(row);
        });
        if (state.page === 'models') loadStability();
    }
    function setSaving(value) {
        state.saving = value;
        updateRefresh();
        $('logoutButton').disabled = value;
        $('rotateApiKey').disabled = value || state.accountStatus !== 'ready';
        $('confirmRotate').disabled = value;
        document.querySelectorAll('.provider-select').forEach(select => { select.disabled = value; });
    }
    async function saveProvider(model, select) {
        if (state.saving) return;
        const selected = select.value;
        const previous = state.account.selected_provider?.[model] || select.options[0]?.value;
        updateSitePrice();
        setSaving(true);
        $('providerStatus').classList.remove('error-text');
        $('providerStatus').textContent = `正在保存 ${model}…`;
        try {
            await api('/api/gpt5_apikey', {model, provider: selected});
            if (state.accountStatus !== 'ready') return;
            // Only merge this change; other response fields may predate a key rotation or account refresh.
            state.account.selected_provider = {...state.account.selected_provider, [model]: selected};
            $('providerStatus').textContent = `${model} 已切换至 ${selected}`;
            toast('Provider 已保存');
            renderProviders();
        } catch (error) {
            if (error.auth) expireSession();
            select.value = previous;
            updateSitePrice();
            $('providerStatus').classList.add('error-text');
            $('providerStatus').textContent = `保存失败：${error.message}`;
            toast(`保存失败：${error.message}`, true);
        } finally {
            setSaving(false);
        }
    }
    async function loadStability() {
        const id = ++state.stabilityRequest;
        const rows = [...$('providerRows').children].map(row => {
            const select = row.querySelector('select');
            return {row, key: `${select.dataset.model}_${select.value}`};
        });
        if (!rows.length) return;
        rows.forEach(({row}) => {
            row.querySelectorAll('.stability-bar').forEach(bar => {
                bar.className = 'stability-bar pending';
                bar.style.height = '100%';
                bar.title = '正在加载统计数据';
            });
            row.querySelector('.provider-latency').textContent = '—';
            row.querySelector('.provider-tps').textContent = '—';
        });
        try {
            const data = await api('/api/gpt5_askstable', rows.map(item => item.key));
            if (id !== state.stabilityRequest) return;
            rows.forEach(({row, key}) => renderStability(row, data[key]));
        } catch (error) {
            if (id !== state.stabilityRequest) return;
            if (error.auth) return expireSession();
            rows.forEach(({row}) => row.querySelectorAll('.stability-bar').forEach(bar => { bar.title = '统计数据加载失败'; }));
            $('providerStatus').classList.add('error-text');
            $('providerStatus').textContent = '稳定性数据加载失败，可点击刷新数据重试。';
        }
    }
    function renderStability(row, raw) {
        const values = Array.isArray(raw) ? raw : Array.isArray(raw?.buckets) ? raw.buckets : [];
        const now = Math.floor(Date.now() / STABILITY_BUCKET_MS);
        row.querySelectorAll('.stability-bar').forEach((bar, group) => {
            let stable = 0, total = 0;
            for (let offset = 0; offset < 8; offset++) {
                const bucket = now - 95 + group * 8 + offset;
                const index = ((bucket % STABILITY_BUCKETS) + STABILITY_BUCKETS) % STABILITY_BUCKETS;
                stable += numeric(values[index * 2]);
                total += numeric(values[index * 2 + 1]);
            }
            const rate = total ? Math.min(100, stable / total * 100) : 100;
            bar.style.height = total && rate === 0 ? '2px' : `${rate}%`;
            bar.className = `stability-bar${total ? rate < 20 ? ' bad' : rate < 50 ? ' warn' : ' good' : ''}`;
            bar.title = `成功率 ${rate === 100 ? '100' : rate.toFixed(1)}%`;
        });
        const average = (total, count) => numeric(count) ? Number((numeric(total) / numeric(count)).toFixed(2)).toString() : '—';
        row.querySelector('.provider-latency').textContent = average(raw?.latency, raw?.latency_n);
        row.querySelector('.provider-tps').textContent = average(raw?.alltime_tokens, raw?.alltime);
    }

    async function copyValue(id, label) {
        const input = $(id);
        if (!input.value) return;
        try {
            if (!navigator.clipboard?.writeText) throw new Error('clipboard unavailable');
            await navigator.clipboard.writeText(input.value);
        } catch (_) {
            const type = input.type;
            const active = document.activeElement;
            try {
                input.type = 'text';
                input.select();
                if (!document.execCommand('copy')) throw new Error('copy failed');
            } catch (_) {
                toast('复制失败，请手动选择并复制。', true);
                return;
            } finally {
                input.type = type;
                input.setSelectionRange(0, 0);
                active?.focus();
            }
        }
        toast(`${label} 已复制`);
    }
    async function rotateKey() {
        if (state.saving || !state.account) return;
        setSaving(true);
        $('rotateError').hidden = true;
        $('confirmRotate').textContent = '正在重置…';
        try {
            const data = await api('/api/gpt5_apikey', {rotate: true});
            if (typeof data.api_key !== 'string' || !data.api_key.startsWith('sk-')) throw new Error('服务器没有返回有效的新密钥。');
            if (state.accountStatus !== 'ready') return;
            state.account.api_key = data.api_key;
            renderAccount();
            $('rotateDialog').close();
            toast('API Key 已重置，旧密钥已失效。');
        } catch (error) {
            if (error.auth) {
                expireSession();
                $('rotateDialog').close();
            } else {
                $('rotateError').textContent = `${error.message} 如请求已送达服务器，请刷新确认当前密钥。`;
                $('rotateError').hidden = false;
            }
        } finally {
            setSaving(false);
            $('confirmRotate').textContent = '确认重置';
            ['copyApiKey', 'toggleApiKey'].forEach(id => { $(id).disabled = state.accountStatus !== 'ready'; });
        }
    }

    function updatePagination() {
        const disabled = state.logLoading || state.accountStatus === 'login';
        $('firstPage').disabled = disabled || state.logPage <= 1;
        $('previousPage').disabled = disabled || state.logPage <= 1;
        $('nextPage').disabled = disabled || !state.logNext;
        $('pageSize').disabled = state.logLoading;
        $('pageIndicator').textContent = `第 ${state.logPage} 页`;
    }
    async function loadLogs(page = 1) {
        hideTokenDetail();
        const id = ++state.logRequest;
        const pageSize = Number($('pageSize').value);
        const start = (page - 1) * pageSize + 1;
        state.logLoading = true;
        state.logs = [];
        $('exportLogs').disabled = true;
        $('logTable').hidden = true;
        $('recordSummary').textContent = '正在加载 API 调用记录';
        updatePagination();
        showMessage('historyState', '正在加载调用日志…', {loading: true});
        await busy(async () => {
            try {
                // Existing endpoint uses inclusive, one-based ranges. Fetch one extra row for pagination.
                const data = await api('/api/gpt5_log_list', {start, end: start + pageSize});
                if (id !== state.logRequest) return;
                if (!Array.isArray(data)) throw new Error('调用日志数据格式异常。');
                state.logPage = page;
                state.logNext = data.length > pageSize;
                state.logs = data.slice(0, pageSize);
                state.logLoaded = true;
                $('logRows').replaceChildren();
                if (!state.logs.length) {
                    $('recordSummary').textContent = page > 1 ? '本页暂无调用记录' : '还没有 API 调用记录';
                    showMessage('historyState', page > 1 ? '本页没有更多记录，可以返回上一页。' : '暂无调用记录。完成一次 API 请求后，可在这里查看。');
                } else {
                    renderLogs();
                    $('recordSummary').textContent = `当前显示第 ${tokens(start)}–${tokens(start + state.logs.length - 1)} 条记录`;
                    $('historyState').hidden = true;
                    $('logTable').hidden = false;
                    $('logTable').scrollLeft = 0;
                    $('exportLogs').disabled = false;
                }
                updated();
            } catch (error) {
                if (id !== state.logRequest) return;
                state.logLoaded = false;
                if (error.auth) return expireSession();
                $('recordSummary').textContent = '调用记录加载失败';
                showMessage('historyState', error.message, {error: true, retry: () => loadLogs(page)});
            } finally {
                if (id === state.logRequest || state.accountStatus === 'login') {
                    state.logLoading = false;
                    updatePagination();
                }
            }
        });
    }
    function renderLogs() {
        const body = $('logRows');
        body.replaceChildren();
        state.logs.forEach(log => {
            const image = isImage(log);
            const row = node('tr');
            const time = node('td');
            const [day, hour] = timeParts(log.time);
            time.append(node('span', 'log-date', day), ' ', node('span', 'log-time', hour));
            const model = node('td');
            const name = node('span', 'log-model', log.model || '—');
            name.title = log.model || '';
            model.append(name, node('span', 'log-provider', ` / ${providerName(log)}`));
            const usage = node('td');
            const button = node('button', image ? 'token-button image-count' : 'token-button',
                image ? numeric(log.used_tokens) ? `${tokens(log.used_tokens)} 次` : '失败' : tokens(log.used_tokens));
            button.type = 'button';
            button.setAttribute('aria-label', `查看 ${log.model || '请求'} 的${image ? '次数' : 'Token'}与费用明细`);
            button.addEventListener('mouseenter', () => showTokenDetail(button, log));
            button.addEventListener('mouseleave', scheduleHideTokenDetail);
            button.addEventListener('focus', () => showTokenDetail(button, log));
            button.addEventListener('blur', scheduleHideTokenDetail);
            usage.append(button);
            row.append(time, model, usage, node('td', '', duration(image ? log.total : log.first, !image)), node('td', '', tps(log)),
                node('td', '', logPrice(log)), node('td', 'charged', logCharge(log)));
            body.append(row);
        });
    }
    function hideTokenDetail() {
        clearTimeout(tokenTooltipTimer);
        $('tokenTooltip').hidden = true;
        tokenTooltipTarget?.removeAttribute('aria-describedby');
        tokenTooltipTarget = null;
    }
    function scheduleHideTokenDetail() {
        clearTimeout(tokenTooltipTimer);
        tokenTooltipTimer = setTimeout(() => {
            if (!tokenTooltipTarget?.matches(':hover, :focus') && !$('tokenTooltip').matches(':hover')) hideTokenDetail();
        }, 120);
    }
    function logCostDetails(log) {
        const formulas = {};
        let total = 0;
        let complete = true;
        const add = (field, expression, count, rate, divisor) => {
            if (!Number.isFinite(rate)) {
                formulas[field] = '单价未记录';
                complete = false;
                return;
            }
            const amount = count * rate / divisor;
            total += amount;
            formulas[field] = `${expression} x ¥${calculationPriceFormat.format(rate)}/${divisor === 1000000 ? 'M tokens' : '次'} = ${money(amount)}`;
        };
        if (Number(log.isimage) === 2) {
            const input = numeric(log.input), cache = numeric(log.cache), makecache = numeric(log.makecache);
            const expression = `(${tokens(input)} - ${tokens(cache)} - ${tokens(makecache)})`;
            add('input', input < cache + makecache ? `max(${expression}, 0)` : expression,
                Math.max(0, input - cache - makecache), log.inputm, 1000000);
            add('output', tokens(log.output), numeric(log.output), log.outputm, 1000000);
            add('cache', tokens(cache), cache, log.cachem, 1000000);
            add('makecache', tokens(makecache), makecache, log.makecachem, 1000000);
            if (numeric(log.websearch) > 0) add('websearch', tokens(log.websearch), numeric(log.websearch), log.websearchm, 1);
        } else {
            add('used_tokens', tokens(log.used_tokens), numeric(log.used_tokens), logUnitPrice(log), isImage(log) ? 1 : 1000000);
        }
        return {formulas, total: complete ? total : null};
    }
    function showTokenDetail(target, log) {
        hideTokenDetail();
        tokenTooltipTarget = target;
        target.setAttribute('aria-describedby', 'tokenTooltip');
        $('tokenDetailList').replaceChildren();
        const itemized = Number(log.isimage) === 2;
        $('tokenDetailList').classList.toggle('itemized', itemized);
        const details = isImage(log) ? [['次数', 'used_tokens']] : [['输入', 'input'], ['输出', 'output'],
            ['缓存读取', 'cache'], ['缓存创建', 'makecache'], ['实际 Token', 'used_tokens']];
        if (numeric(log.websearch) > 0) {
            details.splice(itemized ? details.length - 1 : details.length, 0, ['联网搜索（次）', 'websearch']);
        }
        const calculation = logCostDetails(log);
        const totalAmount = calculation.total === null ? '单价未记录' : money(calculation.total);
        details.filter(([, field]) => field !== 'makecache' || numeric(log.makecache) > 0).forEach(([label, field]) => {
            const item = node('div');
            const formula = node('dd', 'token-cost-formula', calculation.formulas[field] || '');
            if (itemized && field === 'used_tokens') {
                formula.classList.add('token-cost-summary');
                formula.append(node('span', '', '计算费用'), node('strong', '', totalAmount));
            }
            item.append(node('dt', '', label), node('dd', '', tokens(log[field])), formula);
            $('tokenDetailList').append(item);
        });
        if (!itemized) {
            const total = node('div', 'token-cost-total');
            total.append(node('dt', '', '计算费用'), node('dd', '', totalAmount));
            $('tokenDetailList').append(total);
        }
        const tooltip = $('tokenTooltip');
        tooltip.hidden = false;
        const anchor = target.getBoundingClientRect();
        const bounds = tooltip.getBoundingClientRect();
        const margin = 12;
        const gap = 8;
        const top = anchor.top - bounds.height - gap;
        tooltip.style.left = `${Math.max(margin, Math.min(anchor.right - bounds.width, innerWidth - bounds.width - margin))}px`;
        tooltip.style.top = `${Math.max(margin, Math.min(top >= margin ? top : anchor.bottom + gap, innerHeight - bounds.height - margin))}px`;
    }
    function exportLogs() {
        if (!state.logs.length) return;
        const rows = [['时间', '模型', 'Provider', '类型', '实际 Token / 次数', '输入', '输出', '缓存读取', '缓存创建', '联网搜索（次）', '首字 / 耗时(s)', 'TPS', '单价（人民币）', '实际扣费（人民币）']];
        state.logs.forEach(log => rows.push([timeParts(log.time).join(' '), log.model || '', providerName(log), isImage(log) ? '图像' : '文本',
            numeric(log.used_tokens), numeric(log.input), numeric(log.output), numeric(log.cache), numeric(log.makecache), numeric(log.websearch),
            duration(isImage(log) ? log.total : log.first, !isImage(log)), tps(log), logPrice(log), logCharge(log)]));
        const csv = rows.map(row => row.map(value => {
            let text = String(value);
            // Prevent spreadsheet formulas in server-supplied model/provider names.
            if (/^[=+\-@\t\r\n]/.test(text)) text = `'${text}`;
            return `"${text.replaceAll('"', '""')}"`;
        }).join(',')).join('\r\n');
        const url = URL.createObjectURL(new Blob(['\uFEFF', csv], {type: 'text/csv;charset=utf-8;'}));
        const link = node('a');
        link.href = url;
        link.download = `api-logs-page-${state.logPage}.csv`;
        document.body.append(link);
        link.click();
        link.remove();
        setTimeout(() => URL.revokeObjectURL(url), 1000);
        toast('已导出本页调用日志');
    }

    const mobile = window.matchMedia('(max-width: 960px)');
    function setMenu(open) {
        const visible = open && mobile.matches;
        if (visible) setAccountMenu(false);
        $('sidebar').classList.toggle('open', visible);
        $('sidebar').inert = mobile.matches && !visible;
        $('sidebarBackdrop').hidden = !visible;
        $('menuToggle').setAttribute('aria-expanded', String(visible));
        $('menuToggle').setAttribute('aria-label', visible ? '关闭导航' : '展开导航');
        document.body.style.overflow = visible ? 'hidden' : '';
        if (visible) $('sidebar').querySelector('a').focus();
    }
    function route() {
        hideTokenDetail();
        hidePricePopover();
        hideModelLog();
        setAccountMenu(false);
        const requested = location.hash.slice(1);
        const page = Object.hasOwn(pages, requested) ? requested : 'overview';
        const changed = state.page !== page;
        state.page = page;
        const [title, eyebrow, description] = pages[page];
        document.title = `${title} · NEUQ Console`;
        $('breadcrumbTitle').textContent = title;
        $('pageTitle').textContent = title;
        $('pageEyebrow').textContent = eyebrow;
        $('pageDescription').textContent = description;
        Object.keys(pages).forEach(key => { $(`${key}Page`).hidden = key !== page; });
        document.querySelectorAll('[data-page]').forEach(link => {
            const active = link.dataset.page === page;
            link.classList.toggle('active', active);
            if (active) link.setAttribute('aria-current', 'page');
            else link.removeAttribute('aria-current');
        });
        if ($('sidebar').contains(document.activeElement) && mobile.matches) $('menuToggle').focus();
        setMenu(false);
        if (page !== 'credentials') {
            $('apiKeyValue').type = 'password';
            $('toggleApiKey').setAttribute('aria-pressed', 'false');
            $('toggleApiKey').setAttribute('aria-label', '显示 API Key');
            $('toggleApiKey').title = '显示 API Key';
        }
        if (page === 'history' && !state.logLoaded && !state.logLoading) loadLogs(1);
        if (page === 'models' && changed) renderProviders();
        if (changed) window.scrollTo({top: 0, behavior: 'instant'});
    }
    async function refresh() {
        if (state.pending || state.saving) return;
        if (state.page === 'history') return loadLogs(state.logPage);
        $('providerStatus').classList.remove('error-text');
        $('providerStatus').textContent = '切换 Provider 后，下次调用生效。';
        await Promise.allSettled([loadAccount(), loadModels(), loadUser()]);
    }

    $('refreshButton').addEventListener('click', refresh);
    $('userMenuToggle').addEventListener('click', () => setAccountMenu($('accountDropdown').hidden));
    $('userMenuToggle').addEventListener('keydown', event => {
        if (event.key === 'ArrowDown') {
            event.preventDefault();
            setAccountMenu(true);
            $('accountDropdown').querySelector('a').focus();
        }
    });
    $('logoutButton').addEventListener('click', logout);
    document.addEventListener('click', event => {
        if (!$('accountMenu').contains(event.target)) setAccountMenu(false);
    });
    $('accountMenu').addEventListener('focusout', event => {
        if (!$('accountMenu').contains(event.relatedTarget)) setAccountMenu(false);
    });
    $('menuToggle').addEventListener('click', () => setMenu(!$('sidebar').classList.contains('open')));
    $('sidebarBackdrop').addEventListener('click', () => { setMenu(false); $('menuToggle').focus(); });
    document.querySelectorAll('[data-page]').forEach(link => link.addEventListener('click', () => {
        if (mobile.matches) $('menuToggle').focus();
        setMenu(false);
    }));
    mobile.addEventListener('change', () => setMenu(false));
    document.addEventListener('keydown', event => {
        if (event.key === 'Escape') hideTokenDetail();
        if (event.key === 'Escape') closePricePopover();
        if (event.key === 'Escape') hideModelLog();
        if (event.key === 'Escape' && !$('accountDropdown').hidden) {
            setAccountMenu(false);
            $('userMenuToggle').focus();
        }
        if (event.key === 'Escape' && $('sidebar').classList.contains('open')) {
            setMenu(false);
            $('menuToggle').focus();
        }
        if (event.key === 'Tab' && $('sidebar').classList.contains('open')) {
            const links = [...$('sidebar').querySelectorAll('a')];
            if (event.shiftKey && (document.activeElement === links[0] || document.activeElement === $('menuToggle'))) {
                event.preventDefault(); links.at(-1).focus();
            } else if (!event.shiftKey && document.activeElement === links.at(-1)) {
                event.preventDefault(); links[0].focus();
            }
        }
    });
    $('allNoticesButton').addEventListener('click', () => $('noticeDialog').showModal());
    document.querySelectorAll('[data-close-dialog]').forEach(button => button.addEventListener('click', () => button.closest('dialog').close()));
    document.querySelectorAll('dialog').forEach(dialog => dialog.addEventListener('click', event => {
        const rect = dialog.getBoundingClientRect();
        if (event.target === dialog && (event.clientX < rect.left || event.clientX > rect.right || event.clientY < rect.top || event.clientY > rect.bottom)) dialog.close();
    }));
    $('toggleApiKey').addEventListener('click', () => {
        const showing = $('apiKeyValue').type === 'password';
        $('apiKeyValue').type = showing ? 'text' : 'password';
        $('toggleApiKey').setAttribute('aria-pressed', String(showing));
        $('toggleApiKey').setAttribute('aria-label', showing ? '隐藏 API Key' : '显示 API Key');
        $('toggleApiKey').title = showing ? '隐藏 API Key' : '显示 API Key';
    });
    $('copyApiKey').addEventListener('click', () => copyValue('apiKeyValue', 'API Key'));
    $('copyBaseUrl').addEventListener('click', () => copyValue('baseUrlValue', 'Base URL'));
    $('rotateApiKey').addEventListener('click', () => { $('rotateError').hidden = true; $('rotateDialog').showModal(); });
    $('confirmRotate').addEventListener('click', rotateKey);
    $('modelSearch').addEventListener('input', () => { clearTimeout(searchTimer); searchTimer = setTimeout(renderProviders, 180); });
    document.querySelectorAll('[data-model-type]').forEach(button => button.addEventListener('click', () => {
        state.modelType = button.dataset.modelType;
        document.querySelectorAll('[data-model-type]').forEach(item => {
            const active = item === button;
            item.classList.toggle('active', active);
            item.setAttribute('aria-pressed', String(active));
        });
        $('providerBilling').textContent = state.modelType === 'image' ? '单价：¥/次' : '单价：¥/M tokens';
        renderProviders();
    }));
    $('firstPage').addEventListener('click', () => loadLogs(1));
    $('previousPage').addEventListener('click', () => loadLogs(Math.max(1, state.logPage - 1)));
    $('nextPage').addEventListener('click', () => loadLogs(state.logPage + 1));
    $('pageSize').addEventListener('change', () => loadLogs(1));
    $('exportLogs').addEventListener('click', exportLogs);
    $('tokenTooltip').addEventListener('mouseenter', () => clearTimeout(tokenTooltipTimer));
    $('tokenTooltip').addEventListener('mouseleave', scheduleHideTokenDetail);
    $('modelLogTooltip').addEventListener('pointerenter', () => clearTimeout(modelLogTooltipTimer));
    $('modelLogTooltip').addEventListener('pointerleave', scheduleHideModelLog);
    $('closePricePopover').addEventListener('click', closePricePopover);
    $('pricePopover').addEventListener('pointerenter', () => clearTimeout(pricePopoverTimer));
    $('pricePopover').addEventListener('pointerleave', scheduleHidePricePopover);
    $('pricePopover').addEventListener('focusout', event => {
        if (!$('pricePopover').contains(event.relatedTarget) && event.relatedTarget !== pricePopoverTarget) hidePricePopover();
    });
    $('closePricePopover').addEventListener('keydown', event => {
        if (event.key !== 'Tab') return;
        event.preventDefault();
        const target = pricePopoverTarget;
        const next = target?.closest('.provider-row')?.querySelector('.provider-select');
        closePricePopover();
        if (!event.shiftKey) next?.focus({preventScroll: true});
    });
    document.addEventListener('pointerdown', event => {
        if (!event.target.closest('.token-button, #tokenTooltip')) hideTokenDetail();
        if (!event.target.closest('.model-price-button, #pricePopover')) hidePricePopover();
        if (!event.target.closest('.model-log-button, #modelLogTooltip')) hideModelLog();
    });
    window.addEventListener('scroll', event => { if (!$('modelLogTooltip').contains(event.target)) hideModelLog(); }, true);
    window.addEventListener('resize', hideModelLog);
    window.addEventListener('scroll', event => { if (!$('pricePopover').contains(event.target)) hidePricePopover(); }, true);
    window.addEventListener('resize', hidePricePopover);
    window.addEventListener('scroll', hideTokenDetail, true);
    window.addEventListener('resize', hideTokenDetail);
    window.addEventListener('hashchange', route);
    window.addEventListener('pageshow', event => { if (event.persisted) location.reload(); });
    route();
    Promise.allSettled([loadAccount(), loadModels(), loadUser()]);
})();
