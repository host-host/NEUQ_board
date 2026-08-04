const tableContent = document.getElementById('tableContent');
const loadingState = document.getElementById('loadingState');
const emptyState = document.getElementById('emptyState');
const loginState = document.getElementById('loginState');
const errorState = document.getElementById('errorState');
const pageSizeSelect = document.getElementById('pageSize');
let currentPage = 1;
let hasNextPage = false;
let requestId = 0;

function formatTokens(value) {
    return new Intl.NumberFormat('zh-CN').format(Math.max(0, Math.floor(Number(value) || 0)));
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
    [tableContent, loadingState, emptyState, loginState, errorState].forEach(item => { item.hidden = item !== element; });
}

function renderRows(items) {
    const body = document.getElementById('usageLogBody');
    body.innerHTML = '';
    items.forEach(log => {
        const used = Math.max(0, Math.floor(Number(log.used_tokens) || 0));
        const multiply = Math.max(0, Number(log.multiply) || 0);
        const row = document.createElement('tr');
        [formatTime(log.time), log.model || '-', log.provider || '-', formatTokens(used),
            formatMultiply(multiply), formatTokens(Math.ceil(used * multiply))].forEach(value => {
            const cell = document.createElement('td');
            cell.textContent = value;
            row.appendChild(cell);
        });
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

loadPage(1);
