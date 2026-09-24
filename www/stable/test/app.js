(() => {
    'use strict';

    const $ = id => document.getElementById(id);
    const api = window.StableChannelTest;
    const state = {catalog: null, account: null, loading: false, running: false, restoring: false, stop: false,
        rows: [], queue: []};
    const resultCache = new Map();
    const recordKey = (name, provider) => `stable-channel-test:result:v1:${JSON.stringify([name, provider])}`;
    const formatNames = {completions: 'Chat Completions', responses: 'Responses', claude: 'Claude Messages', gemini: 'Gemini generateContent'};
    const numberFormat = new Intl.NumberFormat('zh-CN', {maximumFractionDigits: 2});
    const multiplierFormat = new Intl.NumberFormat('zh-CN', {maximumFractionDigits: 6});
    const node = (tag, className, text) => {
        const element = document.createElement(tag);
        if (className) element.className = className;
        if (text !== undefined) element.textContent = text;
        return element;
    };
    const formatted = value => Number.isFinite(value) ? numberFormat.format(value) : '—';
    const model = () => $('modelSelect').value;

    function readResult(name, provider) {
        const key = recordKey(name, provider);
        try {
            const raw = localStorage.getItem(key);
            if (!raw) return resultCache.get(key) || null;
            const record = JSON.parse(raw);
            if (record?.model !== name || record?.provider !== provider ||
                !['done', 'error'].includes(record.result?.status) ||
                typeof record.result?.text !== 'string' ||
                !Number.isFinite(record.savedAt)) return resultCache.get(key) || null;
            // An unsuccessful disk write must not replace a newer result kept in this tab.
            const cached = resultCache.get(key);
            if (cached && cached.savedAt > record.savedAt) return cached;
            resultCache.set(key, record);
            return record;
        } catch (_) {
            $('storageStatus').textContent = '部分浏览器记录无法读取；仍可测试，新结果会继续尝试保存。';
            return resultCache.get(key) || null;
        }
    }

    function saveResult(row, name, question, answer) {
        const record = {model: name, provider: row.provider.id, question, answer, savedAt: Date.now(),
            multiply: row.provider.multiply, result: {...row.result, text: row.result.text || ''}};
        const key = recordKey(name, row.provider.id);
        row.saved = record;
        resultCache.set(key, record);
        try {
            // One key per model/provider avoids overwriting unrelated results from other tabs.
            localStorage.setItem(key, JSON.stringify(record));
        } catch (_) {
            $('storageStatus').textContent = '浏览器记录保存失败（存储不可用或空间不足），新结果暂时仅保留在当前页面。';
        }
    }

    function notice(message, error = false) {
        $('notice').hidden = !message;
        $('notice').textContent = message;
        $('notice').classList.toggle('error', error);
    }

    function updateControls() {
        const busy = state.loading || state.running;
        const ready = Boolean(state.catalog && state.account && model());
        $('refreshButton').disabled = busy;
        $('modelSelect').disabled = busy || !state.catalog;
        $('questionInput').disabled = busy || !ready;
        $('answerInput').disabled = busy || !ready;
        const available = state.rows.filter(row => row.provider.available);
        const selected = available.filter(row => row.checkbox.checked);
        $('selectAll').disabled = busy || !available.length;
        $('selectAll').checked = available.length > 0 && selected.length === available.length;
        $('selectAll').indeterminate = selected.length > 0 && selected.length < available.length;
        state.rows.forEach(row => { row.checkbox.disabled = busy || !row.provider.available; });
        $('startButton').disabled = busy || !ready || !selected.length;
        $('startButton').textContent = state.restoring ? '正在切回 auto…' : state.running ? '测试中…' : '开始测试';
        $('stopButton').hidden = !state.running || state.restoring;
        $('stopButton').disabled = state.stop;
        $('stopButton').textContent = state.stop ? '等待当前请求结束…' : '完成当前后停止';
        $('selectionSummary').textContent = `共 ${state.rows.length} 个非 auto 渠道 · 已选 ${selected.length} 个` +
            (state.rows.length > available.length ? ` · ${state.rows.length - available.length} 个不可用` : '');
    }

    function updateModelMeta() {
        $('modelFormat').textContent = state.catalog?.model?.[model()]
            ? `请求格式：${formatNames[api.formatFor(state.catalog.model[model()].suggest_format)]}` : '—';
        $('currentProvider').textContent = `当前渠道：${state.account?.selected_provider?.[model()] || '—'}`;
    }

    function resetResult(row, status = 'idle') {
        row.result = {status};
        row.details.open = false;
        row.lastRender = 0;
        renderResult(row);
    }

    function renderResult(row) {
        const result = row.result;
        const statusText = {idle: row.provider.available ? '待测试' : state.account ? '无权限或未配置' : '需登录',
            queued: '等待中', switching: '切换渠道…', asking: '请求中…', done: '已完成', error: '失败', skipped: '已跳过'};
        const active = ['switching', 'asking'].includes(result.status);
        row.element.classList.toggle('is-active', active);
        row.status.textContent = statusText[result.status];
        row.status.className = `status-label ${active ? 'busy' : result.status}`;
        // Keep the last settled result visible until this channel's next test has finished.
        const display = ['done', 'error'].includes(result.status) ? result : row.saved?.result || result;
        row.match.textContent = display.status === 'done' ? display.matched ? '匹配' : '未匹配' : '—';
        row.match.className = `match-result ${display.status === 'done' ? display.matched ? 'yes' : 'no' : ''}`;
        row.match.title = row.saved ? `问题：${row.saved.question || ''}\n预期 Answer：${row.saved.answer || ''}` : '';
        row.savedAt.textContent = row.saved
            ? `${display !== result ? '上次 · ' : ''}${new Date(row.saved.savedAt).toLocaleString('zh-CN', {hour12: false})}` : '';
        row.error.hidden = !display.error;
        row.error.textContent = display.error || '';
        row.details.hidden = display.status !== 'done' && !display.text;
        row.placeholder.hidden = !row.details.hidden;
        row.reply.textContent = display.text || '没有文本回复';
        row.answerSummary.textContent = display.status === 'done' ? `查看回复（${display.text.length} 字符）` : '查看已收到的回复';
        row.latency.textContent = formatted(display.latency);
        row.tps.textContent = formatted(display.tps);
        row.tokens.textContent = formatted(display.outputTokens);
        row.tokens.title = '接口返回的输出 Token 数';
        row.tps.title = display.elapsed == null ? '' : `请求总耗时：${formatted(display.elapsed)} s`;
        const multiply = row.saved ? row.saved.multiply : row.provider.multiply;
        row.multiply.textContent = Number.isFinite(multiply) ? multiplierFormat.format(multiply) : '—';
        row.multiply.title = row.saved ? '本次记录的渠道倍率' : '当前配置的渠道倍率';
    }

    function renderProviders() {
        $('channelRows').replaceChildren();
        state.rows = [];
        state.queue = [];
        $('runProgress').hidden = true;
        updateModelMeta();
        for (const provider of api.providersForModel(state.catalog, model(), state.account)) {
            const element = node('tr');
            const checkCell = node('td', 'checkbox-cell');
            const checkbox = node('input');
            checkbox.type = 'checkbox';
            checkbox.checked = provider.available;
            checkbox.setAttribute('aria-label', `测试 ${provider.id}`);
            checkbox.addEventListener('change', updateControls);
            checkCell.append(checkbox);
            const statusCell = node('td', 'status-cell');
            const status = node('span', 'status-label');
            const savedAt = node('span', 'result-time');
            const error = node('span', 'row-error');
            statusCell.append(status, savedAt, error);
            const match = node('td', 'match-result');
            const answer = node('td', 'answer-cell');
            const details = node('details');
            const answerSummary = node('summary', '', '查看回复');
            const reply = node('pre', 'reply-text');
            details.append(answerSummary, reply);
            const placeholder = node('span', '', '—');
            answer.append(details, placeholder);
            const latency = node('td', 'numeric');
            const tps = node('td', 'numeric');
            const tokens = node('td', 'numeric');
            const multiply = node('td', 'numeric');
            element.append(checkCell, node('td', 'provider-name', provider.id), statusCell, match, answer, latency, tps, tokens, multiply);
            const row = {provider, element, checkbox, status, savedAt, error, match, details, answerSummary, reply, placeholder, latency, tps, tokens, multiply,
                saved: readResult(model(), provider.id), lastRender: 0};
            row.result = row.saved ? {...row.saved.result} : {status: 'idle'};
            state.rows.push(row);
            renderResult(row);
            $('channelRows').append(element);
        }
        $('emptyState').hidden = state.rows.length > 0;
        $('emptyState').textContent = state.catalog ? '该模型没有非 auto 渠道。' : '模型数据未加载。';
        updateControls();
    }

    async function load() {
        if (state.running || state.loading) return;
        state.loading = true;
        const previous = model();
        notice('正在加载模型与账户…');
        $('loginLink').hidden = true;
        updateControls();
        const [catalog, account] = await Promise.allSettled([
            api.requestJson('/api/gpt5_model_list', {}), api.requestJson('/api/gpt5_apikey', {})
        ]);
        state.catalog = null;
        state.account = null;
        const errors = [];
        if (catalog.status === 'fulfilled' && catalog.value.model && typeof catalog.value.model === 'object' && !Array.isArray(catalog.value.model)) {
            state.catalog = catalog.value;
        } else errors.push(`模型加载失败：${catalog.reason?.message || '模型配置格式异常。'}`);
        if (account.status === 'fulfilled' && typeof account.value.api_key === 'string' && account.value.api_key.startsWith('sk-')) {
            state.account = account.value;
        } else {
            errors.push(`账户加载失败：${account.reason?.message || '服务器未返回有效 API Key。'}`);
            $('loginLink').hidden = !account.reason?.auth;
        }
        const models = Object.entries(state.catalog?.model || {})
            .filter(([, config]) => config && config.suggest_format !== 'image')
            .map(([name]) => name).sort((a, b) => a.localeCompare(b));
        $('modelSelect').replaceChildren();
        if (!models.length) $('modelSelect').append(new Option('暂无文本模型', ''));
        else models.forEach(name => $('modelSelect').append(new Option(name, name)));
        if (models.includes(previous)) $('modelSelect').value = previous;
        state.loading = false;
        renderProviders();
        notice(errors.join(' '), errors.length > 0);
    }

    function updateProgress(finished = false, stopped = false) {
        const rows = state.queue;
        const done = rows.filter(row => row.result.status === 'done');
        const failed = rows.filter(row => row.result.status === 'error');
        const skipped = rows.filter(row => row.result.status === 'skipped');
        const active = rows.find(row => ['switching', 'asking'].includes(row.result.status));
        const count = done.length + failed.length;
        $('progressBar').max = rows.length || 1;
        $('progressBar').value = count + skipped.length;
        $('progressText').textContent = finished
            ? `${stopped ? '测试已停止' : '测试完成'} · 已测试 ${count}/${rows.length}`
            : state.stop ? '等待当前请求结束后停止…'
                : active ? `${count + 1}/${rows.length} · ${active.provider.id} · ${active.result.status === 'switching' ? '切换渠道' : '等待回复'}`
                    : `已测试 ${count}/${rows.length}`;
        $('resultSummary').textContent = `匹配 ${done.filter(row => row.result.matched).length} · 未匹配 ${done.filter(row => !row.result.matched).length} · 失败 ${failed.length}` +
            (skipped.length ? ` · 跳过 ${skipped.length}` : '');
    }

    async function start(event) {
        event.preventDefault();
        if (state.running || state.loading || !state.account) return;
        if (!$('testForm').reportValidity()) return;
        const question = $('questionInput').value;
        const answer = $('answerInput').value;
        if (!question.trim() || !answer.length) { notice('请输入问题和预期 Answer。', true); return; }
        const selected = state.rows.filter(row => row.provider.available && row.checkbox.checked);
        if (!selected.length) { notice('请至少选择一个可用渠道。', true); return; }
        const selectedModel = model();
        const format = api.formatFor(state.catalog.model[selectedModel].suggest_format);
        state.running = true;
        state.stop = false;
        updateControls();
        notice('');
        const execute = async () => {
            state.queue = selected;
            selected.forEach(row => {
                row.saved = readResult(selectedModel, row.provider.id);
                resetResult(row, 'queued');
            });
            $('runProgress').hidden = false;
            updateProgress();
            let summary;
            let runError;
            let restoreError;
            try {
                summary = await api.runSequential({
                    providers: selected.map(row => row.provider), model: selectedModel, format, question, answer,
                    shouldStop: () => state.stop,
                    switchProvider: async (name, provider) => {
                        const account = await api.requestJson('/api/gpt5_apikey', {model: name, provider});
                        state.account = account;
                        updateModelMeta();
                        return account;
                    },
                    onResult: (provider, result) => {
                        const row = selected.find(item => item.provider.id === provider.id);
                        row.result = {...row.result, ...result};
                        if (['done', 'error'].includes(result.status)) saveResult(row, selectedModel, question, answer);
                        // Keep large streaming replies responsive without rebuilding rows or closing details.
                        const now = performance.now();
                        if (result.status !== 'asking' || now - row.lastRender >= 100 || result.elapsed != null) {
                            renderResult(row);
                            row.lastRender = now;
                        }
                        if (result.status !== 'asking' || result.text === undefined) updateProgress();
                    }
                });
            } catch (error) {
                runError = error;
                selected.forEach(row => {
                    if (!['queued', 'switching', 'asking'].includes(row.result.status)) return;
                    row.result = {...row.result, status: 'error', error: error.message || '测试中断'};
                    saveResult(row, selectedModel, question, answer);
                    renderResult(row);
                });
            } finally {
                // Keep this page's controls disabled until the account is restored.
                state.restoring = true;
                updateControls();
                $('progressText').textContent = '本轮请求已结束，正在切回 auto 渠道…';
                try {
                    const account = await api.requestJson('/api/gpt5_apikey', {model: selectedModel, provider: 'auto'});
                    state.account = account;
                    updateModelMeta();
                    if (account.selected_provider?.[selectedModel] !== 'auto') throw new Error('服务端返回的当前渠道不是 auto。');
                } catch (error) {
                    restoreError = error;
                } finally {
                    state.restoring = false;
                }
            }
            updateProgress(true, Boolean(runError || summary?.stopped));
            $('progressText').textContent += restoreError ? ' · 切回 auto 失败' : ' · 已切回 auto';
            const messages = [];
            if (runError) messages.push(runError.message || '测试失败，请刷新后重试。');
            else if (summary?.stopped && !state.stop) messages.push('测试已停止，请检查失败渠道的信息后重试。');
            if (restoreError) messages.push(`切回 auto 渠道失败：${restoreError.message || '请求失败'} 请到模型广场确认并手动切换；本轮结果已保留。`);
            notice(messages.join(' '), messages.length > 0);
        };
        try {
            await execute();
        } catch (error) {
            notice(error.message || '测试失败，请刷新后重试。', true);
        } finally {
            state.running = false;
            updateControls();
        }
    }

    $('testForm').addEventListener('submit', start);
    $('refreshButton').addEventListener('click', load);
    $('modelSelect').addEventListener('change', () => { if (!state.running) renderProviders(); });
    $('selectAll').addEventListener('change', () => {
        if (state.running) return;
        state.rows.forEach(row => { if (row.provider.available) row.checkbox.checked = $('selectAll').checked; });
        updateControls();
    });
    $('stopButton').addEventListener('click', () => {
        state.stop = true;
        updateControls();
        updateProgress();
    });
    window.addEventListener('beforeunload', event => {
        if (!state.running) return;
        event.preventDefault();
        event.returnValue = '';
    });
    load();
})();
