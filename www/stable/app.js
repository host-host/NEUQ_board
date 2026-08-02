const BUCKET_COUNT = 3 * 24 * 4;
const BUCKET_MS = 15 * 60 * 1000;

const state = {
    items: [],
    filter: 'all',
    search: '',
    sort: 'status'
};

const elements = {
    list: document.getElementById('providerList'),
    notice: document.getElementById('notice'),
    empty: document.getElementById('emptyState'),
    template: document.getElementById('providerTemplate'),
    tooltip: document.getElementById('heatTooltip'),
    refresh: document.getElementById('refreshButton'),
    updatedAt: document.getElementById('updatedAt'),
    search: document.getElementById('searchInput'),
    filters: document.getElementById('statusFilter'),
    sort: document.getElementById('sortSelect')
};

function finalBucketNumber() {
    return Math.floor(Date.now() / BUCKET_MS);
}

function formatRate(stable, total) {
    if (!total) return '--';
    const rate = stable / total * 100;
    if (rate === 100) return '100%';
    if (rate >= 99) return `${rate.toFixed(2)}%`;
    return `${rate.toFixed(1)}%`;
}

function formatNumber(value) {
    return new Intl.NumberFormat('zh-CN').format(value);
}

function bucketLevel(bucket) {
    if (!bucket.total) return 'empty';
    const rate = bucket.stable / bucket.total;
    if (rate >= .99) return 'good';
    if (rate >= .95) return 'warn';
    return 'bad';
}

function sumBuckets(buckets) {
    return buckets.reduce((sum, bucket) => {
        sum.stable += bucket.stable;
        sum.total += bucket.total;
        return sum;
    }, {stable: 0, total: 0});
}

function normalizeBuckets(raw, currentBucket) {
    const values = Array.isArray(raw) ? raw : [];
    const physical = Array.from({length: BUCKET_COUNT}, (_, index) => ({
        stable: Number(values[index * 2]) || 0,
        total: Number(values[index * 2 + 1]) || 0
    }));

    return Array.from({length: BUCKET_COUNT}, (_, index) => {
        const number = currentBucket - BUCKET_COUNT + 1 + index;
        const physicalIndex = ((number % BUCKET_COUNT) + BUCKET_COUNT) % BUCKET_COUNT;
        return {...physical[physicalIndex], number, start: number * BUCKET_MS};
    });
}

function classifyItem(buckets) {
    const recent = sumBuckets(buckets.slice(-24)); // 最近 6 小时
    if (!recent.total) return 'unknown';
    const rate = recent.stable / recent.total;
    if (rate >= .99) return 'healthy';
    if (rate >= .95) return 'degraded';
    return 'outage';
}

function statusText(status) {
    return {healthy: '稳定', degraded: '波动', outage: '异常', unknown: '暂无数据'}[status];
}

function buildCatalog(config) {
    const catalog = [];
    const mappings = config?.model_available_provider || {};
    Object.entries(mappings).forEach(([model, providers]) => {
        if (!Array.isArray(providers)) return;
        providers.forEach(provider => {
            if (typeof provider !== 'string') return;
            catalog.push({model, provider, key: `${model}_${provider}`});
        });
    });
    return catalog;
}

async function fetchJson(url, options) {
    const response = await fetch(url, options);
    const text = await response.text();
    let data;
    try { data = JSON.parse(text); }
    catch (error) { throw new Error(text || `请求失败（HTTP ${response.status}）`); }
    if (!response.ok) throw new Error(data?.error?.message || `请求失败（HTTP ${response.status}）`);
    if (data?.error) throw new Error(data.error?.message || data.error || '接口返回错误');
    return data;
}

async function loadData() {
    elements.refresh.disabled = true;
    elements.refresh.classList.add('refreshing');
    elements.notice.hidden = false;
    elements.notice.className = 'notice loading';
    elements.notice.innerHTML = '<span class="spinner" aria-hidden="true"></span><span>正在获取 Provider 状态</span>';

    try {
        const config = await fetchJson('/api/gpt5_model_list', {method: 'POST'});
        const catalog = buildCatalog(config);
        const stats = catalog.length ? await fetchJson('/api/gpt5_askstable', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(catalog.map(item => item.key))
        }) : {};
        const currentBucket = finalBucketNumber();

        state.items = catalog.map(entry => {
            const buckets = normalizeBuckets(stats?.[entry.key], currentBucket);
            const total = sumBuckets(buckets);
            const hour = sumBuckets(buckets.slice(-4));
            const status = classifyItem(buckets);
            return {...entry, buckets, total, hour, status};
        });

        elements.notice.hidden = true;
        elements.updatedAt.textContent = `${new Date().toLocaleTimeString('zh-CN', {hour: '2-digit', minute: '2-digit'})} 更新`;
        render();
    } catch (error) {
        elements.list.innerHTML = '';
        elements.notice.hidden = false;
        elements.notice.className = 'notice error';
        elements.notice.innerHTML = `<strong>状态数据载入失败</strong><span>${escapeHtml(error.message)}</span>`;
        elements.updatedAt.textContent = '更新失败';
        updateSummary([]);
    } finally {
        elements.refresh.disabled = false;
        elements.refresh.classList.remove('refreshing');
    }
}

function escapeHtml(value) {
    const node = document.createElement('div');
    node.textContent = String(value ?? '');
    return node.innerHTML;
}

function makeHeatmap(item, container) {
    container.innerHTML = '';
    for (let day = 0; day < 3; day++) {
        const dayBuckets = item.buckets.slice(day * 96, (day + 1) * 96);
        const row = document.createElement('div');
        row.className = 'heat-day';
        const label = document.createElement('span');
        label.className = 'day-label';
        const date = new Date(dayBuckets[0].start);
        label.textContent = date.toLocaleString('zh-CN', {month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit'});
        row.appendChild(label);

        dayBuckets.forEach(bucket => {
            const cell = document.createElement('span');
            cell.className = 'heat-cell';
            cell.tabIndex = 0;
            cell.dataset.level = bucketLevel(bucket);
            cell.dataset.start = String(bucket.start);
            cell.dataset.stable = String(bucket.stable);
            cell.dataset.total = String(bucket.total);
            cell.setAttribute('aria-label', bucketAriaLabel(bucket));
            cell.addEventListener('mouseenter', showTooltip);
            cell.addEventListener('mousemove', moveTooltip);
            cell.addEventListener('mouseleave', hideTooltip);
            cell.addEventListener('focus', showTooltip);
            cell.addEventListener('blur', hideTooltip);
            row.appendChild(cell);
        });
        container.appendChild(row);
    }

    const axis = document.createElement('div');
    axis.className = 'time-axis';
    [0, 6, 12, 18].map(hours => new Date(item.buckets[0].start + hours * 60 * 60 * 1000)
        .toLocaleTimeString('zh-CN', {hour: '2-digit', minute: '2-digit'})).forEach(time => {
        const label = document.createElement('span');
        label.textContent = time;
        axis.appendChild(label);
    });
    container.appendChild(axis);
}

function bucketAriaLabel(bucket) {
    const time = new Date(bucket.start).toLocaleString('zh-CN', {
        month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit'
    });
    return bucket.total
        ? `${time}，稳定率 ${formatRate(bucket.stable, bucket.total)}，${bucket.total} 次请求`
        : `${time}，无请求`;
}

function showTooltip(event) {
    const cell = event.currentTarget;
    const start = Number(cell.dataset.start);
    const stable = Number(cell.dataset.stable);
    const total = Number(cell.dataset.total);
    const end = new Date(start + BUCKET_MS);
    const begin = new Date(start);
    const date = begin.toLocaleDateString('zh-CN', {month: 'long', day: 'numeric'});
    const range = `${begin.toLocaleTimeString('zh-CN', {hour: '2-digit', minute: '2-digit'})} - ${end.toLocaleTimeString('zh-CN', {hour: '2-digit', minute: '2-digit'})}`;
    elements.tooltip.innerHTML = total
        ? `<strong>${date} ${range}</strong><span>稳定率 ${formatRate(stable, total)} · ${stable} / ${total} 次稳定</span>`
        : `<strong>${date} ${range}</strong><span>该时段没有请求</span>`;
    elements.tooltip.hidden = false;
    positionTooltip(event, cell);
}

function moveTooltip(event) {
    if (!elements.tooltip.hidden) positionTooltip(event, event.currentTarget);
}

function positionTooltip(event, cell) {
    const rect = cell.getBoundingClientRect();
    const pointerX = Number.isFinite(event.clientX) && event.clientX ? event.clientX : rect.left + rect.width / 2;
    const tooltipRect = elements.tooltip.getBoundingClientRect();
    const left = Math.min(Math.max(12, pointerX - tooltipRect.width / 2), window.innerWidth - tooltipRect.width - 12);
    const preferredTop = rect.top - tooltipRect.height - 9;
    const top = preferredTop > 8 ? preferredTop : rect.bottom + 9;
    elements.tooltip.style.left = `${left}px`;
    elements.tooltip.style.top = `${top}px`;
}

function hideTooltip() {
    elements.tooltip.hidden = true;
}

function createProviderRow(item) {
    const fragment = elements.template.content.cloneNode(true);
    const row = fragment.querySelector('.provider-row');
    row.classList.add(item.status);
    fragment.querySelector('h2').textContent = item.model;
    fragment.querySelector('.provider-tag').textContent = item.provider;
    fragment.querySelector('.status-label').textContent = statusText(item.status);
    fragment.querySelector('.rate-value').textContent = formatRate(item.total.stable, item.total.total);
    fragment.querySelector('.hour-rate').textContent = formatRate(item.hour.stable, item.hour.total);
    fragment.querySelector('.sample-count').textContent = formatNumber(item.total.total);
    fragment.querySelector('.failure-count').textContent = formatNumber(item.total.total - item.total.stable);
    makeHeatmap(item, fragment.querySelector('.heatmap'));
    return fragment;
}

function filteredItems() {
    const query = state.search.trim().toLocaleLowerCase('zh-CN');
    const priority = {outage: 0, degraded: 1, unknown: 2, healthy: 3};
    const items = state.items.filter(item => {
        const matchesStatus = state.filter === 'all' || item.status === state.filter;
        const matchesSearch = !query || `${item.model} ${item.provider}`.toLocaleLowerCase('zh-CN').includes(query);
        return matchesStatus && matchesSearch;
    });

    return items.sort((a, b) => {
        if (state.sort === 'rate') {
            const aRate = a.total.total ? a.total.stable / a.total.total : -1;
            const bRate = b.total.total ? b.total.stable / b.total.total : -1;
            return bRate - aRate || a.model.localeCompare(b.model);
        }
        if (state.sort === 'samples') return b.total.total - a.total.total || a.model.localeCompare(b.model);
        if (state.sort === 'name') return a.model.localeCompare(b.model) || a.provider.localeCompare(b.provider);
        return priority[a.status] - priority[b.status] || b.total.total - a.total.total;
    });
}

function updateSummary(items) {
    const overall = items.reduce((sum, item) => {
        sum.stable += item.total.stable;
        sum.total += item.total.total;
        return sum;
    }, {stable: 0, total: 0});
    const counts = items.reduce((result, item) => {
        result[item.status]++;
        return result;
    }, {healthy: 0, degraded: 0, outage: 0, unknown: 0});
    const status = counts.outage ? 'outage' : counts.degraded ? 'degraded' : counts.healthy ? 'healthy' : 'unknown';

    document.getElementById('overallStatus').textContent = status === 'healthy' ? '运行稳定' : status === 'degraded' ? '部分线路波动' : status === 'outage' ? '检测到异常' : '暂无统计数据';
    document.getElementById('overallRate').textContent = formatRate(overall.stable, overall.total);
    document.getElementById('overallSamples').textContent = formatNumber(overall.total);
    document.getElementById('healthyCount').textContent = `${counts.healthy} / ${items.length}`;
    document.getElementById('issueCount').textContent = formatNumber(counts.degraded + counts.outage);
    const indicator = document.getElementById('overallIndicator');
    indicator.className = `large-indicator ${status}`;
}

function render() {
    updateSummary(state.items);
    const items = filteredItems();
    elements.list.innerHTML = '';
    const fragment = document.createDocumentFragment();
    items.forEach(item => fragment.appendChild(createProviderRow(item)));
    elements.list.appendChild(fragment);
    elements.empty.hidden = items.length > 0;
}

elements.refresh.addEventListener('click', loadData);
elements.search.addEventListener('input', event => {
    state.search = event.target.value;
    render();
});
elements.filters.addEventListener('click', event => {
    const button = event.target.closest('button[data-filter]');
    if (!button) return;
    state.filter = button.dataset.filter;
    elements.filters.querySelectorAll('button').forEach(item => item.classList.toggle('active', item === button));
    render();
});
elements.sort.addEventListener('change', event => {
    state.sort = event.target.value;
    render();
});

loadData();
