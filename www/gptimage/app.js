let Models = [];          // /api/gpts2 返回的模型分组
let currentMode = 'create'; // 'create' | 'edit'
let sourceImages = [];      // 修改模式下的源图 [{name, dataUrl}]
let isGenerating = false;

// ---------- 用户 ----------
function loaduser() {
    document.getElementById('loginLink').href = "/login?" + window.location.href;
    fetch('/api/user')
        .then(r => { if (!r.ok) throw new Error('net'); return r.json(); })
        .then(data => {
            if (!data.name) {
                document.getElementById('loginLink').style.display = 'block';
                document.getElementById('userContainer').style.display = 'none';
            } else {
                document.getElementById('loginLink').style.display = 'none';
                document.getElementById('userContainer').style.display = 'flex';
                document.getElementById('username').textContent = data.name;
            }
        })
        .catch(() => {});
}
async function handleLogout(event) {
    event.preventDefault();
    await fetch('/api/logout', { method: 'POST' });
    location.reload();
}

// ---------- 模型列表 ----------
async function fetchModels() {
    const selectButton = document.getElementById('selectButton');
    try {
        const response = await fetch('/api/gpts2', { method: 'POST' });
        Models = await response.json();
        const paintBox = document.getElementById('paintModelsContainer');
        const otherBox = document.getElementById('otherModelsContainer');
        paintBox.innerHTML = '';
        otherBox.innerHTML = '';
        let firstAssigned = false;

        for (let i = 0; i < Models.length; i++) {
            const m = Models[i];
            if (m.available === false) continue;
            const isPaint = m.onlypaint === true;
            (m.model || []).forEach(modelName => {
                const item = document.createElement('div');
                item.className = 'model-item';
                item.textContent = modelName;
                if (isPaint) item.classList.add('is-paint');
                item.onclick = () => selectModel(modelName, i);
                // 默认优先选中绘图模型
                if (!firstAssigned && isPaint) {
                    firstAssigned = true;
                    applySelection(modelName, i);
                }
                (isPaint ? paintBox : otherBox).appendChild(item);
            });
        }
        // 没有任何绘图模型时，退而选第一个可用模型
        if (!firstAssigned) {
            const first = otherBox.querySelector('.model-item');
            if (first) {
                const ida = findIdaByModelName(first.textContent);
                applySelection(first.textContent, ida);
            } else {
                selectButton.textContent = '无可用模型';
            }
        }
        if (paintBox.innerHTML === '') {
            paintBox.innerHTML = '<div class="model-empty">当前没有专用绘图模型，可尝试下方支持图片输出的对话模型。</div>';
        }
    } catch (e) {
        selectButton.textContent = '模型加载失败';
        document.getElementById('paintModelsContainer').innerHTML =
            '<div class="model-empty" style="color:var(--danger-color)">模型加载失败</div>';
    }
}
function findIdaByModelName(name) {
    for (let i = 0; i < Models.length; i++) {
        if ((Models[i].model || []).includes(name)) return i;
    }
    return 0;
}
function applySelection(modelName, ida) {
    const btn = document.getElementById('selectButton');
    btn.textContent = modelName;
    btn.setAttribute('ida', ida);
}
function selectModel(modelName, ida) {
    applySelection(modelName, ida);
    document.getElementById('selectionModal').style.display = 'none';
}

// ---------- 模式切换 ----------
function setMode(mode) {
    currentMode = mode;
    document.getElementById('modeCreate').classList.toggle('active', mode === 'create');
    document.getElementById('modeEdit').classList.toggle('active', mode === 'edit');
    document.getElementById('sourceImageGroup').style.display = mode === 'edit' ? 'block' : 'none';
    const promptInput = document.getElementById('promptInput');
    promptInput.placeholder = mode === 'edit'
        ? '描述你想如何修改这张图，例如：把背景换成雪山，整体改为水彩风格'
        : '描述你想要的画面，例如：一只戴着宇航头盔的橘猫，赛博朋克风格，霓虹灯光';
}

// ---------- 源图上传 ----------
async function handleFileSelect(input) {
    const files = input.files;
    if (!files || files.length === 0) return;
    for (let i = 0; i < files.length; i++) {
        try {
            const dataUrl = await compressImage(files[i]);
            sourceImages.push({ name: files[i].name, dataUrl });
        } catch (e) {
            alert('图片处理失败：' + files[i].name);
        }
    }
    input.value = '';
    renderSourceThumbs();
}
function removeSourceImage(idx) {
    sourceImages.splice(idx, 1);
    renderSourceThumbs();
}
function renderSourceThumbs() {
    const box = document.getElementById('sourceThumbs');
    box.innerHTML = sourceImages.map((img, idx) => `
        <div class="source-thumb">
            <img src="${img.dataUrl}" alt="${img.name}" onclick="zoomImage(this.src)">
            <span class="thumb-remove" onclick="removeSourceImage(${idx})">&times;</span>
        </div>
    `).join('');
}

// ---------- 生成 ----------
function buildMessages(prompt) {
    if (currentMode === 'edit' && sourceImages.length > 0) {
        const content = [{ type: 'text', text: prompt || '请根据上面的图片进行修改。' }];
        sourceImages.forEach(img => {
            content.push({ type: 'image_url', image_url: { url: img.dataUrl } });
        });
        return [
            { role: 'system', content: 'You are an image generation and editing assistant. Return the resulting image.' },
            { role: 'user', content }
        ];
    }
    return [
        { role: 'system', content: 'You are an image generation assistant. Generate an image for the user prompt and return it.' },
        { role: 'user', content: prompt }
    ];
}

function buildBodyData(messages) {
    const btn = document.getElementById('selectButton');
    const ida = parseInt(btn.getAttribute('ida'));
    const modelName = btn.textContent;
    let bodyData = {
        model: modelName,
        stream: true,
        messages,
        stream_options: { include_usage: true },
        ida
    };
    // 若该分组对此模型有自定义请求格式，则套用
    const customConfig = Models[ida] && Models[ida].custom;
    if (customConfig) {
        for (const c of customConfig) {
            if (c.model === modelName) {
                bodyData = { ...c, messages, ida };
                break;
            }
        }
    }
    return bodyData;
}

async function generate() {
    if (isGenerating) return;
    const prompt = document.getElementById('promptInput').value.trim();
    const btn = document.getElementById('selectButton');
    const ida = btn.getAttribute('ida');

    if (ida === null || btn.textContent === '无可用模型' || btn.textContent === '载入中...') {
        alert('请先选择一个可用模型');
        return;
    }
    if (!prompt && !(currentMode === 'edit' && sourceImages.length > 0)) {
        alert('请输入提示词');
        return;
    }
    if (currentMode === 'edit' && sourceImages.length === 0) {
        alert('修改模式下请至少上传一张源图片');
        return;
    }

    const welcome = document.getElementById('welcomeBox');
    if (welcome) welcome.remove();

    setGenerating(true);
    const card = createResultCard(prompt);

    try {
        const messages = buildMessages(prompt);
        const bodyData = buildBodyData(messages);
        await streamGenerate(bodyData, card);
    } catch (e) {
        finishCardError(card, '请求失败：' + e.message);
    } finally {
        setGenerating(false);
    }
}

function setGenerating(state) {
    isGenerating = state;
    const btn = document.getElementById('generateBtn');
    btn.disabled = state;
    document.getElementById('generateBtnText').textContent = state ? '生成中...' : '开始生成';
}

// 调用 /api/gpt2，读取 SSE 流，边接收边解析图片
async function streamGenerate(bodyData, card) {
    const response = await fetch('/api/gpt2', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(bodyData)
    });
    if (!response.ok) throw new Error('接口请求失败 (HTTP ' + response.status + ')');

    const reader = response.body.getReader();
    const decoder = new TextDecoder('utf-8');
    let buffer = '';
    let rawContent = '';

    while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split('\n');
        buffer = lines.pop();
        for (const line of lines) {
            const trimmed = line.trim();
            if (!trimmed || trimmed === 'data: [DONE]') continue;
            if (!trimmed.startsWith('data: ')) continue;
            try {
                const parsed = JSON.parse(trimmed.slice(6));
                const delta = parsed.choices && parsed.choices[0] && parsed.choices[0].delta;
                if (delta && delta.content) {
                    rawContent += delta.content;
                    updateCard(card, rawContent);
                }
            } catch (err) {
                // 非 JSON 的 data 行（比如后端直接返回的错误文本），整体并入内容
                rawContent += trimmed.slice(6);
                updateCard(card, rawContent);
            }
        }
    }
    finalizeCard(card, rawContent);
}

// ---------- 结果卡片渲染 ----------
function createResultCard(prompt) {
    const gallery = document.getElementById('gallery');
    const card = document.createElement('div');
    card.className = 'result-card';
    card.innerHTML = `
        <div class="result-meta">
            <span class="result-mode">${currentMode === 'edit' ? '✏️ 修改' : '🎨 创建'}</span>
            <span class="result-model">${document.getElementById('selectButton').textContent}</span>
        </div>
        ${prompt ? `<div class="result-prompt">${escapeHtml(prompt)}</div>` : ''}
        <div class="result-images"></div>
        <div class="result-text"></div>
        <div class="result-status"><span class="spinner"></span> 正在生成...</div>
    `;
    gallery.insertBefore(card, gallery.firstChild);
    return card;
}

function updateCard(card, rawContent) {
    const images = extractImagesFromText(rawContent);
    const imgBox = card.querySelector('.result-images');
    const rendered = imgBox.querySelectorAll('img').length;
    if (images.length > rendered) {
        for (let i = rendered; i < images.length; i++) {
            appendImage(imgBox, images[i]);
        }
        const status = card.querySelector('.result-status');
        if (status) status.innerHTML = '<span class="spinner"></span> 已收到图片，继续接收...';
    }
}

function appendImage(imgBox, src) {
    const wrap = document.createElement('div');
    wrap.className = 'gen-image-wrap';
    wrap.innerHTML = `
        <img src="${src}" class="gen-image" onclick="zoomImage(this.src)" loading="lazy">
        <div class="gen-image-actions">
            <button class="img-action-btn" title="下载">⬇ 下载</button>
            <button class="img-action-btn" title="作为源图修改">✏️ 二次修改</button>
        </div>
    `;
    const [dlBtn, editBtn] = wrap.querySelectorAll('.img-action-btn');
    dlBtn.onclick = (e) => { e.stopPropagation(); downloadImage(src, `image_${Date.now()}.png`); };
    editBtn.onclick = (e) => { e.stopPropagation(); useImageAsSource(src); };
    imgBox.appendChild(wrap);
}

function finalizeCard(card, rawContent) {
    const images = extractImagesFromText(rawContent);
    const textPart = stripImagesFromText(rawContent);
    const textBox = card.querySelector('.result-text');
    if (textPart) {
        try { textBox.innerHTML = marked.parse(textPart); }
        catch { textBox.textContent = textPart; }
    }
    const status = card.querySelector('.result-status');
    if (images.length === 0) {
        // 没有图片：可能是模型不支持出图，或返回了错误信息
        status.className = 'result-status error';
        status.textContent = textPart
            ? '该模型未返回图片（上方为文字回复）。请改用绘图模型。'
            : '未获取到图片，请确认所选模型支持图片输出，或稍后重试。';
    } else {
        status.remove();
    }
}

function finishCardError(card, msg) {
    const status = card.querySelector('.result-status');
    if (status) { status.className = 'result-status error'; status.textContent = msg; }
}

// 把生成的图片直接作为修改模式的源图
async function useImageAsSource(src) {
    let dataUrl = src;
    if (!src.startsWith('data:')) {
        try {
            const res = await fetch(src);
            const blob = await res.blob();
            dataUrl = await new Promise((resolve, reject) => {
                const r = new FileReader();
                r.onload = () => resolve(r.result);
                r.onerror = reject;
                r.readAsDataURL(blob);
            });
        } catch {
            alert('该图片为外链且无法跨域读取，请先下载后手动上传。');
            return;
        }
    }
    sourceImages = [{ name: 'previous_result', dataUrl }];
    setMode('edit');
    renderSourceThumbs();
    document.getElementById('sidebarPanel').classList.add('active');
    document.getElementById('sidebarOverlay').classList.add('active');
}

function clearGallery() {
    if (!confirm('确定清空所有生成结果吗？')) return;
    document.getElementById('gallery').innerHTML = `
        <div class="welcome-box" id="welcomeBox">
            <div class="welcome-icon">🎨</div>
            <h2>AI 绘图工作台</h2>
            <p>在左侧输入提示词即可<strong>创建图片</strong>；切换到<strong>修改图片</strong>模式并上传源图，可对图片进行二次编辑。</p>
        </div>`;
}

function escapeHtml(s) {
    return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

// ---------- 交互 ----------
function toggleSidebar() {
    document.getElementById('sidebarPanel').classList.toggle('active');
    document.getElementById('sidebarOverlay').classList.toggle('active');
}

// Ctrl+Enter 快捷生成
document.getElementById('promptInput').addEventListener('keydown', (e) => {
    if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
        e.preventDefault();
        generate();
    }
});

// 粘贴图片直接进入修改模式
document.getElementById('promptInput').addEventListener('paste', async (e) => {
    const items = e.clipboardData && e.clipboardData.items;
    if (!items) return;
    for (let i = 0; i < items.length; i++) {
        if (items[i].type.indexOf('image') !== -1) {
            const file = items[i].getAsFile();
            if (file) {
                const dataUrl = await compressImage(file);
                sourceImages.push({ name: `pasted_${Date.now()}.jpg`, dataUrl });
                if (currentMode !== 'edit') setMode('edit');
                renderSourceThumbs();
            }
        }
    }
});

document.addEventListener('DOMContentLoaded', () => {
    fetchModels();
    loaduser();
    setMode('create');
});
