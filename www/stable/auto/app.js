(() => {
    'use strict';

    const $ = id => document.getElementById(id);
    const api = window.StableChannelTest;
    const HOURS = 72;
    const hourDetail = $('hourDetail'), hourTitle = $('hourTitle'), hourRows = $('hourRows');//会被移动到表格行内，先留引用
    const state = {catalog: null, model: '', data: null, selected: null, editing: ''};
    const node = (tag, className, text) => {
        const element = document.createElement(tag);
        if (className) element.className = className;
        if (text !== undefined) element.textContent = text;
        return element;
    };
    const fixed = value => Number.isFinite(value) && value >= 0 ? String(Number(value.toFixed(2))) : '-';
    const timeText = seconds => new Date(seconds * 1000).toLocaleString('zh-CN', {hour12: false});

    function notice(message, error = false) {
        $('notice').hidden = !message;
        $('notice').textContent = message;
        $('notice').classList.toggle('error', error);
    }

    // 红：请求失败或未匹配 ans；绿：匹配 ans；白：成功但未配置 ans
    const recordColor = record => record.stable !== 1 || record.match === false ? 'bad' : record.match === true ? 'ok' : 'plain';
    const cellColor = records => !records.length ? 'none'
        : ['bad', 'ok', 'plain'].find(color => records.some(record => recordColor(record) === color));

    function allProviders() {
        const config = state.catalog.model[state.model];
        const ids = Array.isArray(config.all) ? config.all : [];
        return [...new Set(ids)].map(id => {
            const provider = state.catalog.provider?.[id];
            return {id, name: provider?.name || '—',
                ratio: provider ? api.officialPriceRatio(config.price, api.mergePrice(config.price, provider.cost)) : null};
        }).sort((a, b) => (a.ratio ?? Infinity) - (b.ratio ?? Infinity));
    }

    function renderModels() {
        const models = Object.entries(state.catalog.model).filter(([, config]) => config?.auto).map(([name]) => name).sort();
        $('modelList').replaceChildren(...models.map(name => {
            const button = node('button', name === state.model ? 'button button-primary' : 'button', name);
            button.type = 'button';
            button.addEventListener('click', () => { location.hash = encodeURIComponent(name); });
            return button;
        }));
        if (!models.length) $('modelList').textContent = '没有配置 auto 的模型。';
    }

    function replyText(append) {
        const texts = (Array.isArray(append) ? append : []).map(item => item?.content).filter(text => typeof text === 'string');
        return texts.length ? texts.join('\n') : JSON.stringify(append, null, 2);
    }

    function renderHour() {
        const selected = state.selected;
        hourDetail.hidden = !selected;
        if (!selected) return;
        hourTitle.textContent = `${selected.provider} · ${timeText(selected.hour * 3600)} 起 1 小时`;
        const records = state.data.record.filter(record => record.provider === selected.provider && Math.floor(record.time / 3600) === selected.hour);
        hourRows.replaceChildren(...records.map(record => {
            const row = node('tr');
            const color = recordColor(record);
            const result = record.stable !== 1 ? `请求失败 #${record.stable}（HTTP ${record.httpcode}）`
                : record.match === true ? '匹配' : record.match === false ? '未匹配' : '成功（未配置 ans）';
            const reply = node('td', 'answer-cell');
            const details = node('details');
            details.append(node('summary', '', '查看'), node('pre', 'reply-text',
                (record.ans !== undefined ? `预期 ans：${record.ans}\n\n` : '') + replyText(record.append) + (record.info ? `\n\n${record.info}` : '')));
            reply.append(details);
            row.append(node('td', '', timeText(record.time)), node('td', '', record.name),
                node('td', color === 'bad' ? 'result-bad' : color === 'ok' ? 'result-ok' : '', result),
                node('td', 'numeric', fixed(record.first)), node('td', 'numeric', fixed(record.total)),
                node('td', 'numeric', String(record.output || 0)),
                node('td', 'numeric', record.output > 0 && record.total > 0 ? fixed(record.output / record.total) : '-'), reply);
            return row;
        }));
    }

    function renderDetail() {
        const config = state.catalog.model[state.model];
        const autos = Array.isArray(config.auto?.provider) ? config.auto.provider : [];
        const providers = allProviders();
        $('autoLine').textContent = autos.length ? autos.map(id => state.data.state[id]?.blocked ? `${id}（已屏蔽）` : id).join(' → ') : '未配置';
        const current = Math.floor(Date.now() / 3600000);
        $('historyRows').replaceChildren(...providers.flatMap(provider => {
            const row = node('tr');
            const checkbox = node('input');
            checkbox.type = 'checkbox';
            checkbox.checked = state.data.state[provider.id]?.blocked === true;
            checkbox.addEventListener('change', async () => {
                checkbox.disabled = true;
                try {
                    await api.requestJson('/api/gpt5_autotest_set', {model: state.model, provider: provider.id, blocked: checkbox.checked});
                    await loadModel();
                } catch (error) {
                    checkbox.checked = !checkbox.checked;
                    checkbox.disabled = false;
                    notice(error.message, true);
                }
            });
            const blocked = node('td');
            blocked.append(checkbox);
            const name = node('button', 'link-button', provider.id);
            name.type = 'button';
            name.title = '调整测试间隔';
            name.addEventListener('click', () => openVtime(provider.id));
            const nameCell = node('td');
            nameCell.append(name);
            const vtime = state.data.state[provider.id]?.vtime || {};
            const cells = node('div', 'cells');
            for (let hour = current - HOURS + 1; hour <= current; hour++) {
                const records = state.data.record.filter(record => record.provider === provider.id && Math.floor(record.time / 3600) === hour);
                const cell = node('button', `cell ${cellColor(records)}`);
                cell.type = 'button';
                cell.title = `${timeText(hour * 3600)} · ${records.length} 次请求`;
                if (state.selected?.provider === provider.id && state.selected.hour === hour) cell.classList.add('selected');
                if (records.length) cell.addEventListener('click', () => {
                    const same = state.selected?.provider === provider.id && state.selected.hour === hour;
                    state.selected = same ? null : {provider: provider.id, hour};
                    renderDetail();
                });
                cells.append(cell);
            }
            const cellTd = node('td');
            cellTd.append(cells);
            row.append(nameCell, node('td', '', provider.name),
                node('td', 'numeric', Number.isFinite(provider.ratio) ? `x${Number(provider.ratio.toFixed(6))}` : '—'),
                node('td', '', autos.includes(provider.id) ? '是' : '—'), blocked, node('td', '', state.data.question.filter(q => vtime[q] > 0).map(q => `${q}: ${vtime[q]}`).join('，')), cellTd);
            if (state.selected?.provider !== provider.id) return [row];
            const detailRow = node('tr'), detailCell = node('td', 'hour-cell');
            detailCell.colSpan = row.children.length;
            detailCell.append(hourDetail);
            detailRow.append(detailCell);
            return [row, detailRow];
        }));
        renderHour();
    }

    function openVtime(provider) {
        state.editing = provider;
        const vtime = state.data.state[provider]?.vtime || {};
        $('vtimeTitle').textContent = `${state.model} · ${provider}`;
        $('vtimeFields').replaceChildren(...state.data.question.map(question => {
            const label = node('label', '', question);
            const input = node('input');
            input.type = 'number';
            input.min = '0';
            input.step = 'any';
            input.required = true;
            input.name = question;
            input.value = vtime[question] || 0;
            label.append(input);
            return label;
        }));
        if (!state.data.question.length) $('vtimeFields').textContent = 'gpt5.json 中没有配置问题。';
        $('vtimeDialog').showModal();
    }

    async function loadModel() {
        state.data = await api.requestJson('/api/gpt5_autotest_get', {model: state.model});
        renderDetail();
    }

    async function load() {
        notice('正在加载…');
        try {
            state.catalog = await api.requestJson('/api/gpt5_model_list', {});
            const name = decodeURIComponent(location.hash.slice(1));
            state.model = state.catalog.model?.[name]?.auto ? name : '';
            $('detail').hidden = !state.model;
            $('pageTitle').textContent = state.model || '配置了 auto 的模型';
            $('pageHint').textContent = state.model ? '屏蔽与测试间隔修改后立即生效。' : '点击模型查看渠道与测试历史。';
            renderModels();
            if (state.model) await loadModel();
            notice('');
        } catch (error) {
            $('detail').hidden = true;
            notice(error.message || '加载失败', true);
        }
    }

    $('vtimeForm').addEventListener('submit', async event => {
        event.preventDefault();
        const vtime = {};
        $('vtimeFields').querySelectorAll('input').forEach(input => { vtime[input.name] = Number(input.value); });
        try {
            await api.requestJson('/api/gpt5_autotest_set', {model: state.model, provider: state.editing, vtime});
            $('vtimeDialog').close();
            await loadModel();
        } catch (error) {
            $('vtimeDialog').close();
            notice(error.message, true);
        }
    });
    $('vtimeCancel').addEventListener('click', () => $('vtimeDialog').close());
    $('refreshButton').addEventListener('click', load);
    window.addEventListener('hashchange', () => { state.selected = null; load(); });
    load();
})();
