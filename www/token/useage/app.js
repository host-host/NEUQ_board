const tableContent = document.getElementById('tableContent');
const loadingState = document.getElementById('loadingState');
const emptyState = document.getElementById('emptyState');
const loginState = document.getElementById('loginState');
const errorState = document.getElementById('errorState');
const pageSizeSelect = document.getElementById('pageSize');
const tokenTooltip = document.getElementById('tokenBreakdownTooltip');
const tooltipInput = document.getElementById('tooltipInput');
const tooltipOutput = document.getElementById('tooltipOutput');
const tooltipCache = document.getElementById('tooltipCache');
const tooltipMakecache = document.getElementById('tooltipMakecache');
let currentPage = 1;
let hasNextPage = false;
let requestId = 0;

function formatTokens(value) {
    return new Intl.NumberFormat('zh-CN').format(Math.max(0, Math.floor(Number(value) || 0)));
}

function formatTokenDetail(value) {
    const tokens = Math.max(0, Math.floor(Number(value) || 0));
    return tokens ? formatTokens(tokens) : '-';
}

function formatMultiply(value) {
    const number = Math.max(0, Number(value) || 0);
    return `${Number(number.toFixed(6))}x`;
}

function formatTime(value) {
    const date = new Date(Number(value) * 1000);
    if (!Number.isFinite(date.getTime())) return '-';
    return new Intl.DateTimeFormat('zh-CN', {
        year:'numeric', month:'2-digit', day:'2-digit', hour:'2-digit', minute:'2-digit', second:'2-digit', hour12:false
    }).format(date);
}

function showState(element) {
    hideTokenBreakdown();
    [tableContent, loadingState, emptyState, loginState, errorState].forEach(item => { item.hidden = item !== element; });
}

function hideTokenBreakdown() {
    tokenTooltip.hidden = true;
}

function showTokenBreakdown(target, log) {
    tooltipInput.textContent = formatTokenDetail(log.input);
    tooltipOutput.textContent = formatTokenDetail(log.output);
    tooltipCache.textContent = formatTokenDetail(log.cache);
    tooltipMakecache.textContent = formatTokenDetail(log.makecache);
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

function appendTextCell(row, value) {
    const cell = document.createElement('td');
    cell.textContent = value;
    row.appendChild(cell);
}

function renderRows(items) {
    const body = document.getElementById('usageLogBody');
    hideTokenBreakdown();
    body.innerHTML = '';
    items.forEach(log => {
        const used = Math.max(0, Math.floor(Number(log.used_tokens) || 0));
        const multiply = Math.max(0, Number(log.multiply) || 0);
        const row = document.createElement('tr');
        [formatTime(log.time), log.model || '-', log.provider || '-'].forEach(value => appendTextCell(row, value));

        const usageCell = document.createElement('td');
        const usageTarget = document.createElement('span');
        usageTarget.className = 'token-total';
        usageTarget.tabIndex = 0;
        usageTarget.textContent = formatTokens(used);
        usageTarget.setAttribute('aria-label', `实际 Token ${formatTokens(used)}，输入 ${formatTokenDetail(log.input)}，输出 ${formatTokenDetail(log.output)}，缓存读取 ${formatTokenDetail(log.cache)}，缓存创建 ${formatTokenDetail(log.makecache)}`);
        usageTarget.addEventListener('mouseenter', () => showTokenBreakdown(usageTarget, log));
        usageTarget.addEventListener('mouseleave', hideTokenBreakdown);
        usageTarget.addEventListener('focus', () => showTokenBreakdown(usageTarget, log));
        usageTarget.addEventListener('blur', hideTokenBreakdown);
        usageCell.appendChild(usageTarget);
        row.appendChild(usageCell);

        appendTextCell(row, formatMultiply(multiply));
        appendTextCell(row, formatTokens(Math.ceil(used * multiply)));
        body.appendChild(row);
    });
}

function updatePagination(start, count) {
    const hasPrevious = currentPage > 1;
    document.getElementById('firstPage').disabled = !hasPrevious;
    document.getElementById('previousPage').disabled = !hasPrevious;
    document.getElementById('nextPage').disabled = !hasNextPage;
    document.getElementById('pageIndicator').textContent = `第 ${currentPage} 页`;
    document.getElementById('recordSummary').textContent = `当前显示第 ${formatTokens(start)} 至 ${formatTokens(start + count - 1)} 条记录`;
}

async function loadPage(page) {
    const id = ++requestId;
    const pageSize = Number(pageSizeSelect.value);
    const start = (page - 1) * pageSize + 1;
    const end = start + pageSize;
    showState(loadingState);
    try {
        const response = await fetch('/api/gpt5_log_list', {
            method:'POST', headers:{'Content-Type':'application/json'},
            body:JSON.stringify({start, end})
        });
        const text = await response.text();
        let data;
        try { data = text ? JSON.parse(text) : {}; } catch (_) { throw new Error(text || '无法读取服务器响应'); }
        const message = data?.error?.message || '';
        if (!response.ok || message) {
            if (response.status === 401 || /log in/i.test(message)) return showState(loginState);
            throw new Error(message || `请求失败（HTTP ${response.status}）`);
        }
        if (id !== requestId) return;
        currentPage = page;
        const items = Array.isArray(data) ? data : [];
        if (!items.length) return showState(emptyState);
        hasNextPage = items.length > pageSize;
        const visibleItems = items.slice(0, pageSize);
        updatePagination(start, visibleItems.length);
        renderRows(visibleItems);
        showState(tableContent);
    } catch (error) {
        if (id !== requestId) return;
        errorState.textContent = error.message || '加载用量记录失败';
        showState(errorState);
    }
}

document.getElementById('firstPage').addEventListener('click', () => loadPage(1));
document.getElementById('previousPage').addEventListener('click', () => loadPage(currentPage - 1));
document.getElementById('nextPage').addEventListener('click', () => loadPage(currentPage + 1));
pageSizeSelect.addEventListener('change', () => loadPage(1));
window.addEventListener('scroll', hideTokenBreakdown, true);
window.addEventListener('resize', hideTokenBreakdown);

loadPage(1);
