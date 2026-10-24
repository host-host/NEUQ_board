(() => {
    'use strict';

    const $ = id => document.getElementById(id);
    const CNY_PER_MILLION_CREDITS = 0.3;
    // 后端卡密面额是 int，超过该值会溢出。
    const MAX_CARD_VALUE = 2000000000;
    const quotaFormat = new Intl.NumberFormat('zh-CN');
    const moneyFormat = new Intl.NumberFormat('zh-CN', {minimumFractionDigits: 2, maximumFractionDigits: 2});
    const state = {user: null, ready: false, submitting: false, cards: [], value: 0};
    let toastTimer;

    function toast(message, error = false) {
        clearTimeout(toastTimer);
        $('toast').textContent = message;
        $('toast').className = `toast${error ? ' error' : ''}`;
        $('toast').hidden = false;
        toastTimer = setTimeout(() => { $('toast').hidden = true; }, error ? 5000 : 2600);
    }

    function yuan(credits) {
        return `¥${moneyFormat.format(Number(credits) / 1000000 * CNY_PER_MILLION_CREDITS)}`;
    }

    function selectedUnit() {
        return $('cardForm').elements.unit.value;
    }

    function cardValueFromInput() {
        const raw = Number($('cardValue').value);
        if (!Number.isFinite(raw) || raw <= 0) return NaN;
        return selectedUnit() === 'direct' ? Math.trunc(raw) : Math.trunc(raw / CNY_PER_MILLION_CREDITS * 1000000);
    }

    function updatePreview() {
        const direct = selectedUnit() === 'direct';
        $('valueLabel').textContent = direct ? '单张额度' : '单张金额（元）';
        $('cardValue').placeholder = direct ? '10000000' : '3';
        $('previewFormula').textContent = direct ? '输入值将直接作为卡密面额' : '每 0.3 元换算为 1,000,000 额度';
        const value = cardValueFromInput();
        $('previewValue').textContent = Number.isFinite(value) ? `${quotaFormat.format(value)} 额度 · ${yuan(value)}` : '-';
    }

    function setStatus(message, error = false) {
        const element = $('formStatus');
        element.hidden = !message;
        element.className = `form-status${error ? ' error' : ''}`;
        element.textContent = message || '';
    }

    function setSubmitting(submitting) {
        state.submitting = submitting;
        $('submitButton').disabled = submitting || !state.ready;
        $('submitButton').textContent = submitting ? '正在生成…' : '生成卡密';
        $('resetButton').disabled = submitting;
        $('cardValue').disabled = submitting;
        $('cardCount').disabled = submitting;
        $('cardForm').querySelectorAll('input[name="unit"]').forEach(input => { input.disabled = submitting; });
    }

    async function loadUser() {
        try {
            const response = await fetch('/api/user', {credentials: 'same-origin', cache: 'no-store'});
            const text = await response.text();
            let data = null;
            try { data = text ? JSON.parse(text) : null; } catch (_) { data = null; }
            if (!response.ok) throw new Error(`账户信息加载失败（HTTP ${response.status}）`);
            state.user = data?.name ? data : null;
        } catch (error) {
            state.user = null;
            state.ready = false;
            $('accountState').hidden = false;
            $('accountState').className = 'account-state error';
            $('accountState').textContent = error.message || '账户信息加载失败';
            setSubmitting(false);
            return;
        }
        const allowed = state.user?.admin === true;
        state.ready = state.user?.admin === true;
        $('accountState').hidden = false;
        if (!state.user) {
            $('accountState').className = 'account-state error';
            $('accountState').textContent = '未登录';
            setStatus('请先登录管理员账号后再生成卡密。', true);
        } else if (!allowed) {
            $('accountState').className = 'account-state error';
            $('accountState').textContent = `${state.user.name} · 无权限`;
        } else {
            $('accountState').className = 'account-state';
            $('accountState').textContent = `已登录：${state.user.name}`;
        }
        setSubmitting(false);
    }

    async function copyText(text, message) {
        try {
            if (!navigator.clipboard?.writeText) throw new Error('clipboard unavailable');
            await navigator.clipboard.writeText(text);
            toast(message);
        } catch (_) {
            const area = document.createElement('textarea');
            area.value = text;
            area.setAttribute('readonly', '');
            area.style.position = 'fixed';
            area.style.opacity = '0';
            document.body.append(area);
            area.select();
            try {
                if (document.execCommand('copy')) toast(message);
                else throw new Error('copy failed');
            } catch (error) {
                toast('复制失败，请手动选择卡密', true);
            } finally {
                area.remove();
            }
        }
    }

    function renderCards() {
        const body = $('resultBody');
        body.replaceChildren();
        state.cards.forEach((card, index) => {
            const row = document.createElement('tr');
            const order = document.createElement('td');
            order.textContent = String(index + 1);
            const code = document.createElement('td');
            const codeElement = document.createElement('span');
            codeElement.className = 'card-code';
            codeElement.textContent = card;
            code.append(codeElement);
            const value = document.createElement('td');
            value.textContent = `${quotaFormat.format(state.value)} 额度 · ${yuan(state.value)}`;
            const action = document.createElement('td');
            const button = document.createElement('button');
            button.type = 'button';
            button.className = 'copy-button';
            button.textContent = '复制';
            button.setAttribute('aria-label', `复制第 ${index + 1} 张卡密`);
            button.addEventListener('click', () => copyText(card, '已复制卡密'));
            action.append(button);
            row.append(order, code, value, action);
            body.append(row);
        });
        $('resultCount').textContent = String(state.cards.length);
        $('resultSection').hidden = state.cards.length === 0;
    }

    function downloadCards() {
        if (!state.cards.length) return;
        const header = `# 卡密 ${state.cards.length} 张 · 单张 ${state.value} 额度（${yuan(state.value)}）\n`;
        const content = header + state.cards.join('\n') + '\n';
        const url = URL.createObjectURL(new Blob([content], {type: 'text/plain;charset=utf-8'}));
        const link = document.createElement('a');
        link.href = url;
        link.download = `cards-${new Date().toISOString().slice(0, 19).replaceAll(':', '')}.txt`;
        document.body.append(link);
        link.click();
        link.remove();
        setTimeout(() => URL.revokeObjectURL(url), 1000);
    }

    async function submitCard(event) {
        event.preventDefault();
        if (state.submitting) return;
        if (!state.ready) return setStatus('请先登录管理员账号后再生成卡密。', true);
        const count = Math.trunc(Number($('cardCount').value));
        const value = cardValueFromInput();
        if (!Number.isFinite(count) || count < 1 || count > 1000) return setStatus('生成数量需为 1 到 1000 之间的整数。', true);
        if (!Number.isFinite(value) || value < 1) return setStatus('单张面额无效，请输入大于 0 的数值。', true);
        if (value > MAX_CARD_VALUE) return setStatus(`单张面额过大，当前上限为 ${quotaFormat.format(MAX_CARD_VALUE)} 额度（约 ${yuan(MAX_CARD_VALUE)}）。`, true);

        setSubmitting(true);
        setStatus('正在生成卡密…');
        try {
            const response = await fetch('/api/admin_card_create', {
                method: 'POST', credentials: 'same-origin', cache: 'no-store',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({amount: count, value})
            });
            const text = await response.text();
            let data = null;
            try { data = text ? JSON.parse(text) : null; } catch (_) { data = null; }
            if (!response.ok) {
                const message = data?.error?.message || '';
                if (response.status === 401) throw new Error('登录状态已失效，请重新登录。');
                if (response.status === 403) throw new Error(message || '当前账号没有生成卡密的权限。');
                throw new Error(message || `请求失败（HTTP ${response.status}）`);
            }
            if (!Array.isArray(data)) throw new Error('服务器返回了无法识别的数据。');
            state.cards = data.filter(card => typeof card === 'string' && card);
            state.value = value;
            renderCards();
            setStatus(`已生成 ${state.cards.length} 张卡密，请及时保存。`);
            toast('卡密生成成功');
        } catch (error) {
            setStatus(error.message || '生成卡密失败，请稍后重试。', true);
        } finally {
            setSubmitting(false);
        }
    }

    $('cardForm').addEventListener('submit', submitCard);
    $('cardForm').addEventListener('input', updatePreview);
    $('cardForm').addEventListener('change', updatePreview);
    $('resetButton').addEventListener('click', () => {
        if (state.submitting) return;
        $('cardValue').value = '';
        $('cardCount').value = '1';
        $('cardForm').elements.unit.value = 'convert';
        updatePreview();
        setStatus('');
    });
    $('copyAllButton').addEventListener('click', () => copyText(state.cards.join('\n'), '已复制全部卡密'));
    $('downloadButton').addEventListener('click', downloadCards);

    updatePreview();
    loadUser();
})();
