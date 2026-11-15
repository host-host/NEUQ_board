'use strict';
const API = '/api/modeltrace';
const MAX_SAMPLES = 20;   // 每条回复单独作为一组查询，后端一次最多 20 组
const BUSY_RETRIES = 12, BUSY_DELAY = 1500;

const $ = id => document.getElementById(id);
function el(tag, cls, text) {
    const e = document.createElement(tag);
    if (cls) e.className = cls;
    if (text !== undefined && text !== null) e.textContent = text;
    return e;
}
const pct = x => (x * 100).toFixed(x >= 0.9995 || x < 0.0005 ? 0 : 1) + '%';
const sleep = ms => new Promise(r => setTimeout(r, ms));

// 后端同一时刻只处理一个请求，忙的时候会返回 code=busy，这里自动重试
async function call(body, onBusy) {
    for (let i = 0; ; i++) {
        const res = await fetch(API, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
        if (!res.ok) throw new Error('接口返回 ' + res.status);
        const data = await res.json();
        if (!data.error) return data;
        if (data.error.code === 'busy' && i < BUSY_RETRIES) { if (onBusy) onBusy(i + 1); await sleep(BUSY_DELAY); continue; }
        const map = { busy: '后端正忙，请稍后再试', not_ready: '后端还没有准备好：' + data.error.message };
        throw new Error(map[data.error.code] || data.error.message || '查询失败');
    }
}

/* ---------- 校准 ----------
   p.json 里 f[A][B] = 真实模型是 A 时，后端给模型 B 的平均概率。打开页面时读一次。
   后端对一条回复给出概率 q 后，按贝叶斯反推：真实是 A 的概率 = Σ_B q[B] × f[A][B] / Σ_A' f[A'][B]。
   多条回复各自反推后取平均。 */
let ptable = null;
const ptableReady = fetch('/modeltrace/p.json', { cache: 'no-cache' })
    .then(res => { if (!res.ok) throw new Error(res.status); return res.json(); })
    .then(f => { ptable = f; })
    .catch(() => { ptable = null; });

function calibrateOne(r, f, names) {
    const q = {};
    r.top.forEach(t => { q[t.model] = t.score; });
    if (r.top.length !== names.length || names.some(n => !(n in q))) throw new Error('校准表和后端模型不一致，需要重新生成 p.json');
    const p = {};
    names.forEach(a => { p[a] = 0; });
    names.forEach(b => {
        let col = 0;
        names.forEach(a => { col += f[a][b] || 0; });
        if (col > 0) names.forEach(a => { p[a] += q[b] * (f[a][b] || 0) / col; });
        else p[b] += q[b];
    });
    return p;
}
const ranked = p => Object.keys(p).sort((a, b) => p[b] - p[a]).map(m => ({ model: m, score: p[m] }));

// 把后端"每条回复一组"的结果合成一个结果，字段和后端单组结果一致
function calibrate(results, f) {
    const names = Object.keys(f);
    const ok = results.filter(r => !r.error);
    if (!ok.length) return { samples: 0, error: (results[0] && results[0].error) || '没有可用的回复' };
    const avg = {}, votes = {}, per = [];
    names.forEach(a => { avg[a] = 0; });
    ok.forEach(r => {
        const p = calibrateOne(r, f, names), top = ranked(p), s = r.per_sample[0];
        names.forEach(a => { avg[a] += p[a] / ok.length; });
        votes[top[0].model] = (votes[top[0].model] || 0) + 1;
        per.push({ predicted: top[0].model, score: top[0].score, top: top.slice(0, 3), letters: s.letters });
    });
    const top = ranked(avg), family = {};
    top.forEach(t => { const k = t.model.split('-')[0]; family[k] = (family[k] || 0) + t.score; });
    return {
        samples: ok.length, predicted: top[0].model, top: top.slice(0, 5),
        family: Object.fromEntries(Object.entries(family).sort((a, b) => b[1] - a[1])),
        votes: Object.fromEntries(Object.entries(votes).sort((a, b) => b[1] - a[1])), per_sample: per
    };
}

/* ---------- 输入 ---------- */
function getSamples() {
    return Array.from($('sampleList').querySelectorAll('textarea'), t => t.value.trim()).filter(v => /[A-Za-z]/.test(v));
}
function addSample() {
    const box = $('sampleList');
    if (box.children.length >= MAX_SAMPLES) return;
    const n = box.children.length + 1;
    const ta = el('textarea'); ta.spellcheck = false; ta.setAttribute('aria-label', '第 ' + n + ' 条模型回复');
    ta.placeholder = n === 1 ? '把模型的回复粘贴到这里。' : '把同一个模型的第 ' + n + ' 条回复粘贴到这里。';
    ta.addEventListener('input', updateFoot);
    box.append(ta);
    $('addSample').disabled = box.children.length >= MAX_SAMPLES;
    return ta;
}
function updateFoot() {
    const samples = getSamples();
    const letters = samples.map(s => s.replace(/[^A-Za-z]/g, '').length);
    $('samplesFoot').textContent = samples.length
        ? '已填 ' + samples.length + ' 条回复，字母数：' + letters.join('、')
        : '还没有内容';
}
function setStatus(text, isError) {
    const s = $('status'); s.textContent = text || ''; s.classList.toggle('error', !!isError);
}

async function submit(ev) {
    ev.preventDefault();
    const samples = getSamples();
    const total = samples.length;
    if (!total) return setStatus('请先粘贴至少一条模型回复。', true);
    if (total > MAX_SAMPLES) return setStatus('一次最多查询 ' + MAX_SAMPLES + ' 条回复，当前有 ' + total + ' 条。', true);
    const button = $('submitButton');
    button.disabled = true; setStatus('正在查询 ' + total + ' 条回复…');
    try {
        await ptableReady;
        if (!ptable) throw new Error('校准表 p.json 加载失败，请刷新页面重试');
        const data = await call({ action: 'query', top: Object.keys(ptable).length, groups: samples.map(s => ({ samples: [s] })) }, n => setStatus('后端正在处理其他查询，排队重试中（第 ' + n + ' 次）…'));
        renderResults([calibrate(data.results || [], ptable)]);
        setStatus('');
        $('resultsPanel').scrollIntoView({ behavior: 'smooth', block: 'start' });
    } catch (e) {
        setStatus(e.message || '查询失败', true);
    } finally {
        button.disabled = false;
    }
}

/* ---------- 结果 ---------- */
function renderResults(results) {
    const box = $('results');
    box.replaceChildren();
    results.forEach(r => box.append(renderResult(r)));
    $('resultsPanel').hidden = !results.length;
}
function renderResult(r) {
    const card = el('article', 'mt-result');
    const head = el('div', 'mt-result-head');
    card.append(head);
    if (r.error) { card.append(el('div', 'mt-error', r.error)); return card; }
    head.append(el('span', 'mt-result-meta', r.samples + ' 条回复'));

    const hero = el('div', 'mt-hero');
    hero.append(el('strong', '', r.predicted), el('span', '', '匹配率 ' + pct(r.top[0].score) + (r.top[0].score < 0.5 ? '（低置信）' : '')));
    card.append(hero);

    const bars = el('div', 'mt-bars');
    bars.setAttribute('role', 'list'); bars.setAttribute('aria-label', '前几名模型的匹配率');
    r.top.forEach((t, i) => {
        const label = el('span', 'mt-bar-label' + (i ? '' : ' first'), t.model);
        const track = el('span', 'mt-bar-track');
        const fill = el('span', 'mt-bar-fill'); fill.style.width = Math.max(0, Math.min(100, t.score * 100)) + '%';
        track.append(fill); track.title = t.model + '：' + pct(t.score);
        label.setAttribute('role', 'listitem');
        bars.append(label, track, el('span', 'mt-bar-value', pct(t.score)));
    });
    card.append(bars);

    const facts = el('dl', 'mt-facts');
    const fam = Object.entries(r.family).filter(([, v]) => v >= 0.005).map(([k, v]) => k + ' ' + pct(v)).join('，');
    facts.append(el('dt', '', '厂商家族'), el('dd', '', fam || '—'));
    if (r.samples > 1) {
        const votes = Object.entries(r.votes).map(([k, v]) => k + ' ' + v + ' 票').join('，');
        facts.append(el('dt', '', '逐条投票'), el('dd', '', votes));
    }
    card.append(facts);

    const details = el('details', 'mt-details');
    details.append(el('summary', '', '查看每条回复的单独结果'));
    const wrap = el('div', 'mt-table-wrap');
    const table = el('table', 'mt-table');
    const thead = el('thead'); const hr = el('tr');
    [['#', 'num'], ['首选', ''], ['匹配率', 'num'], ['第二名', ''], ['字母数', 'num']].forEach(([t, c]) => hr.append(el('th', c, t)));
    thead.append(hr);
    const tbody = el('tbody');
    r.per_sample.forEach((s, i) => {
        const tr = el('tr');
        const second = s.top && s.top[1] ? s.top[1].model + ' ' + pct(s.top[1].score) : '—';
        tr.append(el('td', 'num', i + 1), el('td', '', s.predicted), el('td', 'num', pct(s.score) + (s.score < 0.5 ? '（低置信）' : '')), el('td', '', second),
            el('td', 'num', s.letters));
        tbody.append(tr);
    });
    table.append(thead, tbody); wrap.append(table); details.append(wrap);
    card.append(details);
    return card;
}

/* ---------- 支持的模型 ---------- */
async function loadModels() {
    const summary = $('modelsSummary'), box = $('models'), button = $('reloadModels');
    button.disabled = true; summary.textContent = '正在加载…';
    try {
        const data = await call({ action: 'models' });
        const parts = ['共 ' + data.model_count + ' 个模型'];
        if (data.sample_count) parts.push('样本库 ' + data.sample_count + ' 条');
        if (data.built_at) parts.push('更新于 ' + new Date(data.built_at * 1000).toLocaleString('zh-CN', { hour12: false }));
        if (data.cv) parts.push('库内交叉验证首选准确率 ' + pct(data.cv.top1) + '、前三 ' + pct(data.cv.top3));
        summary.textContent = parts.join('，') + '。';
        const fams = new Map();
        data.models.forEach(m => { if (!fams.has(m.family)) fams.set(m.family, []); fams.get(m.family).push(m); });
        box.replaceChildren();
        fams.forEach((list, name) => {
            const sec = el('div', 'mt-family');
            sec.append(el('h3', '', name + '（' + list.length + '）'));
            const ul = el('ul', 'mt-chips');
            list.forEach(m => {
                const li = el('li', '', m.name);
                if (m.samples) li.append(el('small', '', m.samples + ' 条样本'));
                ul.append(li);
            });
            sec.append(ul); box.append(sec);
        });
    } catch (e) {
        box.replaceChildren();
        summary.textContent = '加载失败：' + (e.message || '未知错误');
    } finally {
        button.disabled = false;
    }
}

$('copyPrompt').addEventListener('click', async () => {
    const b = $('copyPrompt'), text = $('promptText').textContent;
    try { await navigator.clipboard.writeText(text); b.textContent = '已复制'; }
    catch (_) {
        const r = document.createRange(); r.selectNodeContents($('promptText'));
        const s = getSelection(); s.removeAllRanges(); s.addRange(r); b.textContent = '已选中，请按 Ctrl+C';
    }
    setTimeout(() => { b.textContent = '复制提示词'; }, 2000);
});
$('addSample').addEventListener('click', () => { const t = addSample(); if (t) t.focus(); });
$('queryForm').addEventListener('submit', submit);
$('reloadModels').addEventListener('click', loadModels);
addSample();
updateFoot();
loadModels();
