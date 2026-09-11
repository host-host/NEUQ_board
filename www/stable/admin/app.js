const tableContent = document.getElementById('tableContent');
const loadingState = document.getElementById('loadingState');
const emptyState = document.getElementById('emptyState');
const loginState = document.getElementById('loginState');
const permissionState = document.getElementById('permissionState');
const errorState = document.getElementById('errorState');
const pageSizeSelect = document.getElementById('pageSize');
const tokenTooltip = document.getElementById('tokenBreakdownTooltip');
const filterForm = document.getElementById('filterForm');
const userFilterInput = document.getElementById('userFilter');
const fromFilterInput = document.getElementById('fromFilter');
const toFilterInput = document.getElementById('toFilter');
const grantDialog = document.getElementById('grantDialog');
const grantForm = document.getElementById('grantForm');
const grantUserIdInput = document.getElementById('grantUserId');
const grantAmountInput = document.getElementById('grantAmount');
const grantStatus = document.getElementById('grantStatus');
const grantResult = document.getElementById('grantResult');
const submitGrantButton = document.getElementById('submitGrantButton');
const closeGrantDialogButton = document.getElementById('closeGrantDialogButton');
const cancelGrantButton = document.getElementById('cancelGrantButton');
let currentPage = 1;
let hasNextPage = false;
let requestId = 0;
let activeFilters = {};

function formatTokens(value) {
    return new Intl.NumberFormat('zh-CN').format(Math.max(0, Math.floor(Number(value) || 0)));
}

function formatQuota(value) {
    const quota = Number(value);
    return Number.isFinite(quota) ? new Intl.NumberFormat('zh-CN').format(Math.trunc(quota)) : '-';
}

function formatTokenDetail(value) {
    const tokens = Math.max(0, Math.floor(Number(value) || 0));
    return tokens ? formatTokens(tokens) : '-';
}

function formatTime(value) {
    const date = new Date(Number(value) * 1000);
    if (!Number.isFinite(date.getTime())) return '-';
    return new Intl.DateTimeFormat('zh-CN', {
        year:'numeric', month:'2-digit', day:'2-digit', hour:'2-digit', minute:'2-digit', second:'2-digit', hour12:false
    }).format(date);
}

function formatDuration(value) {
    const seconds = Number(value);
    if (!Number.isFinite(seconds) || seconds <= 0 || seconds > 1e6) return '-';
    return Number(seconds.toFixed(2));
}

function formatTps(log) {
    const output = Math.max(0, Number(log.output) || 0);
    const total = Number(log.total);
    if (!Number.isFinite(total) || total <= 0 || total > 1e6 || output <= 0) return '-';
    return Number((output / total).toFixed(2));
}

function isImageLog(log) {
    return log?.isimage === true || Number(log?.isimage) === 1;
}

function hideTokenBreakdown() {
    tokenTooltip.hidden = true;
}

function showTokenBreakdown(target, log) {
    document.getElementById('tooltipInput').textContent = formatTokenDetail(log.input);
    document.getElementById('tooltipOutput').textContent = formatTokenDetail(log.output);
    document.getElementById('tooltipCache').textContent = formatTokenDetail(log.cache);
    document.getElementById('tooltipMakecache').textContent = formatTokenDetail(log.makecache);
    tokenTooltip.hidden = false;
    const targetRect = target.getBoundingClientRect();
    const tooltipRect = tokenTooltip.getBoundingClientRect();
    const margin = 8;
    const gap = 8;
    let left = targetRect.right - tooltipRect.width;
    let top = targetRect.top - tooltipRect.height - gap;
    if (top < margin) top = targetRect.bottom + gap;
    left = Math.min(Math.max(margin, left), window.innerWidth - tooltipRect.width - margin);
    top = Math.min(Math.max(margin, top), window.innerHeight - tooltipRect.height - margin);
    tokenTooltip.style.left = `${Math.round(left)}px`;
    tokenTooltip.style.top = `${Math.round(top)}px`;
}

function showState(element) {
    hideTokenBreakdown();
    [tableContent, loadingState, emptyState, loginState, permissionState, errorState].forEach(item => {
        item.hidden = item !== element;
    });
}

function appendTextCell(row, value, className = '') {
    const cell = document.createElement('td');
    cell.textContent = value;
    if (className) cell.className = className;
    row.appendChild(cell);
    return cell;
}

function statusMeta(value) {
    const stable = Number(value);
    if (stable === 1) return {label:'成功', className:'success'};
    if (stable >= 1000) return {label:'上游错误', className:'upstream'};
    if (stable >= 2) return {label:'请求错误', className:'request'};
    return {label:'待检查', className:'unknown'};
}

function appendStatusCell(row, stable) {
    const meta = statusMeta(stable);
    const cell = document.createElement('td');
    const label = document.createElement('span');
    label.className = `status-label ${meta.className}`;
    label.textContent = meta.label;
    cell.appendChild(label);
    if (Number(stable) !== 1) {
        const code = document.createElement('span');
        code.className = 'status-code';
        code.textContent = `#${Number(stable) || 0}`;
        cell.appendChild(code);
    }
    row.appendChild(cell);
}

function appendUserCell(row, log) {
    const cell = document.createElement('td');
    const content = document.createElement('div');
    const identity = document.createElement('div');
    const name = document.createElement('span');
    const id = document.createElement('span');
    const grantButton = document.createElement('button');
    content.className = 'user-content';
    name.className = 'user-name';
    id.className = 'user-id';
    name.textContent = log.user || '未知用户';
    id.textContent = log.userid || '-';
    identity.append(name, id);
    grantButton.className = 'user-grant-button';
    grantButton.type = 'button';
    grantButton.textContent = '+';
    grantButton.title = `调整 ${log.user || log.userid} 的额度`;
    grantButton.setAttribute('aria-label', grantButton.title);
    grantButton.addEventListener('click', () => openGrantDialog(log.userid, log.user));
    content.append(identity, grantButton);
    cell.appendChild(content);
    row.appendChild(cell);
}

function appendUsageCell(row, log) {
    const image = isImageLog(log);
    const used = Math.max(0, Math.floor(Number(log.used_tokens) || 0));
    const cell = document.createElement('td');
    const target = document.createElement('span');
    target.className = image ? 'token-total image-total' : 'token-total';
    target.textContent = image ? (used ? `${formatTokens(used)} 次` : '失败') : formatTokens(used);
    if (!image) {
        target.tabIndex = 0;
        target.addEventListener('mouseenter', () => showTokenBreakdown(target, log));
        target.addEventListener('mouseleave', hideTokenBreakdown);
        target.addEventListener('focus', () => showTokenBreakdown(target, log));
        target.addEventListener('blur', hideTokenBreakdown);
    }
    cell.appendChild(target);
    row.appendChild(cell);
}

function appendInfoCell(row, info) {
    const cell = document.createElement('td');
    if (!info) {
        cell.textContent = '-';
    } else {
        const detail = document.createElement('details');
        detail.className = 'error-detail';
        const summary = document.createElement('summary');
        const content = document.createElement('pre');
        summary.textContent = '查看';
        content.textContent = info;
        detail.append(summary, content);
        cell.appendChild(detail);
    }
    row.appendChild(cell);
}

function renderRows(items) {
    const body = document.getElementById('logBody');
    hideTokenBreakdown();
    body.replaceChildren();
    for (const log of items) {
        const image = isImageLog(log);
        const used = Math.max(0, Math.floor(Number(log.used_tokens) || 0));
        const multiply = Math.max(0, Number(log.multiply) || 0);
        const row = document.createElement('tr');
        appendTextCell(row, formatTime(log.time));
        appendUserCell(row, log);
        appendStatusCell(row, log.stable);
        appendTextCell(row, log.model || '-', 'model-cell');
        appendTextCell(row, log.provider || '-');
        appendUsageCell(row, log);
        appendTextCell(row, formatDuration(image ? log.total : log.first));
        appendTextCell(row, formatTps(log));
        appendTextCell(row, formatTokens(Math.ceil(used * multiply)));
        appendInfoCell(row, log.info);
        body.appendChild(row);
    }
}

function localTimeSeconds(input) {
    if (!input.value) return 0;
    const milliseconds = new Date(input.value).getTime();
    return Number.isFinite(milliseconds) ? Math.floor(milliseconds / 1000) : NaN;
}

function applyFilters() {
    const from = localTimeSeconds(fromFilterInput);
    const to = localTimeSeconds(toFilterInput);
    toFilterInput.setCustomValidity('');
    if (!Number.isFinite(from) || !Number.isFinite(to)) {
        toFilterInput.setCustomValidity('请输入有效的时间');
        return toFilterInput.reportValidity();
    }
    if (from && to && from > to) {
        toFilterInput.setCustomValidity('结束时间不能早于开始时间');
        return toFilterInput.reportValidity();
    }
    activeFilters = {};
    const user = userFilterInput.value.trim();
    if (user) activeFilters.user = user;
    if (from) activeFilters.from = from;
    if (to) activeFilters.to = to;
    loadPage(1);
}

function clearFilters() {
    filterForm.reset();
    toFilterInput.setCustomValidity('');
    activeFilters = {};
    loadPage(1);
}

function selectedGrantMode() {
    return grantForm.elements.grantMode.value;
}

function calculatedGrantAmount() {
    const amount = Number(grantAmountInput.value);
    if (selectedGrantMode() === 'direct') return Math.trunc(amount);
    return Math.trunc(amount / 0.3 * 1000000);
}

function updateGrantPreview() {
    const direct = selectedGrantMode() === 'direct';
    document.getElementById('grantAmountLabel').textContent = direct ? '额度' : '金额';
    document.getElementById('grantFormula').textContent = direct ? '输入值将直接计入总额度' : '每 0.3 换算为 1,000,000 额度';
    grantAmountInput.placeholder = direct ? '1000000' : '0.30';
    const added = calculatedGrantAmount();
    document.getElementById('grantPreviewValue').textContent = grantAmountInput.value ? formatQuota(added) : '-';
}

function resetGrantFeedback() {
    grantStatus.hidden = true;
    grantStatus.className = 'grant-status';
    grantStatus.textContent = '';
    grantResult.hidden = true;
}

function openGrantDialog(userid = '', username = '') {
    grantForm.reset();
    resetGrantFeedback();
    grantUserIdInput.value = typeof userid === 'string' && userid.length === 8 ? userid : '';
    document.getElementById('grantDialogSubtitle').textContent = username ? `${username} · ${userid}` : '调整账户的 API 可用额度';
    updateGrantPreview();
    grantDialog.showModal();
    (grantUserIdInput.value ? grantAmountInput : grantUserIdInput).focus();
}

function closeGrantDialog() {
    if (!submitGrantButton.disabled) grantDialog.close();
}

function setGrantSubmitting(submitting) {
    grantForm.querySelectorAll('input').forEach(input => { input.disabled = submitting; });
    closeGrantDialogButton.disabled = submitting;
    cancelGrantButton.disabled = submitting;
    submitGrantButton.disabled = submitting;
    submitGrantButton.textContent = submitting ? '正在调整...' : '确认调整';
}

async function submitGrant() {
    if (!grantForm.reportValidity()) return;
    const userid = grantUserIdInput.value;
    const mode = selectedGrantMode();
    const amount = Number(grantAmountInput.value);
    resetGrantFeedback();
    setGrantSubmitting(true);
    grantStatus.hidden = false;
    grantStatus.textContent = '正在调整用户额度...';
    try {
        const response = await fetch('/api/gpt5_admin_add_token', {
            method:'POST',
            headers:{'Content-Type':'application/json'},
            body:JSON.stringify({
                userid,
                mode,
                amount
            })
        });
        const text = await response.text();
        let data;
        try { data = text ? JSON.parse(text) : {}; }
        catch (_) { throw new Error(text || '无法读取服务器响应'); }
        const message = data?.error?.message;
        if (!response.ok || message) {
            if (response.status === 401) throw new Error('登录状态已失效');
            if (response.status === 403) throw new Error('当前账号没有调整额度的权限');
            if (response.status === 404) throw new Error('未找到该用户');
            throw new Error(message || `请求失败（HTTP ${response.status}）`);
        }
        grantStatus.hidden = true;
        grantResult.hidden = false;
        document.getElementById('grantResultUser').textContent = data.user ? `${data.user} · ${data.userid}` : data.userid;
        document.getElementById('grantResultAdded').textContent = formatQuota(data.added);
        document.getElementById('grantResultLimit').textContent = formatQuota(data.token_limit);
    } catch (error) {
        grantStatus.className = 'grant-status error';
        grantStatus.textContent = error.message || '调整额度失败';
    } finally {
        setGrantSubmitting(false);
    }
}

function updatePagination(start, count) {
    const hasPrevious = currentPage > 1;
    document.getElementById('firstPage').disabled = !hasPrevious;
    document.getElementById('previousPage').disabled = !hasPrevious;
    document.getElementById('nextPage').disabled = !hasNextPage;
    document.getElementById('pageIndicator').textContent = `第 ${currentPage} 页`;
    const filtered = Object.keys(activeFilters).length ? ' · 筛选结果' : '';
    document.getElementById('recordSummary').textContent = `第 ${formatTokens(start)} 至 ${formatTokens(start + count - 1)} 条 · 按时间倒序${filtered}`;
}

async function loadPage(page) {
    const id = ++requestId;
    const pageSize = Number(pageSizeSelect.value);
    const start = (page - 1) * pageSize + 1;
    const end = start + pageSize;
    showState(loadingState);
    try {
        const response = await fetch('/api/gpt5_admin_log_list', {
            method:'POST',
            headers:{'Content-Type':'application/json'},
            body:JSON.stringify({start, end, ...activeFilters})
        });
        const text = await response.text();
        let data;
        try { data = text ? JSON.parse(text) : {}; }
        catch (_) { throw new Error(text || '无法读取服务器响应'); }
        const message = data?.error?.message || '';
        if (!response.ok || message) {
            if (response.status === 401 || /log in/i.test(message)) return showState(loginState);
            if (response.status === 403 || /permission/i.test(message)) return showState(permissionState);
            throw new Error(message || `请求失败（HTTP ${response.status}）`);
        }
        if (id !== requestId) return;
        const items = Array.isArray(data) ? data : [];
        currentPage = page;
        if (!items.length) {
            emptyState.textContent = Object.keys(activeFilters).length ? '没有符合筛选条件的日志记录' : '暂无日志记录';
            return showState(emptyState);
        }
        hasNextPage = items.length > pageSize;
        const visibleItems = items.slice(0, pageSize);
        renderRows(visibleItems);
        updatePagination(start, visibleItems.length);
        showState(tableContent);
    } catch (error) {
        if (id !== requestId) return;
        errorState.textContent = error.message || '加载日志失败';
        showState(errorState);
    }
}

document.getElementById('firstPage').addEventListener('click', () => loadPage(1));
document.getElementById('previousPage').addEventListener('click', () => loadPage(currentPage - 1));
document.getElementById('nextPage').addEventListener('click', () => loadPage(currentPage + 1));
document.getElementById('refreshButton').addEventListener('click', () => loadPage(currentPage));
document.getElementById('openGrantDialogButton').addEventListener('click', () => openGrantDialog());
closeGrantDialogButton.addEventListener('click', closeGrantDialog);
cancelGrantButton.addEventListener('click', closeGrantDialog);
grantDialog.addEventListener('cancel', event => {
    if (submitGrantButton.disabled) event.preventDefault();
});
grantDialog.addEventListener('click', event => {
    if (event.target === grantDialog) closeGrantDialog();
});
grantForm.addEventListener('submit', event => {
    event.preventDefault();
    submitGrant();
});
grantForm.addEventListener('change', event => {
    if (event.target.name === 'grantMode') {
        grantAmountInput.value = '';
        grantAmountInput.setCustomValidity('');
        resetGrantFeedback();
        updateGrantPreview();
    }
});
grantAmountInput.addEventListener('input', () => {
    grantAmountInput.setCustomValidity('');
    resetGrantFeedback();
    updateGrantPreview();
});
grantUserIdInput.addEventListener('input', () => {
    document.getElementById('grantDialogSubtitle').textContent = '调整账户的 API 可用额度';
    resetGrantFeedback();
});
filterForm.addEventListener('submit', event => {
    event.preventDefault();
    applyFilters();
});
document.getElementById('clearFilterButton').addEventListener('click', clearFilters);
[fromFilterInput, toFilterInput].forEach(input => {
    input.addEventListener('input', () => toFilterInput.setCustomValidity(''));
});
pageSizeSelect.addEventListener('change', () => loadPage(1));
window.addEventListener('scroll', hideTokenBreakdown, true);
window.addEventListener('resize', hideTokenBreakdown);

loadPage(1);
