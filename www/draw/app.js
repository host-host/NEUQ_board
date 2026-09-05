(function () {
    'use strict';

    const form = document.getElementById('drawForm');
    const prompt = document.getElementById('prompt');
    const promptCount = document.getElementById('promptCount');
    const model = document.getElementById('model');
    const size = document.getElementById('size');
    const count = document.getElementById('count');
    const quality = document.getElementById('quality');
    const extraJson = document.getElementById('extraJson');
    const generateButton = document.getElementById('generateButton');
    const generateLabel = document.getElementById('generateLabel');
    const clearButton = document.getElementById('clearButton');
    const formMessage = document.getElementById('formMessage');
    const accountText = document.getElementById('accountText');
    const accessStatus = document.getElementById('accessStatus');
    const resultState = document.getElementById('resultState');
    const resultMeta = document.getElementById('resultMeta');
    const imageGrid = document.getElementById('imageGrid');
    const rawDetails = document.getElementById('rawResponseDetails');
    const rawResponse = document.getElementById('rawResponse');
    const copyRequestButton = document.getElementById('copyRequestButton');
    const requestSection = document.getElementById('requestPreviewSection');
    const requestPreview = document.getElementById('requestPreview');
    const requestStatus = document.getElementById('requestStatus');

    let siteApiKey = '';
    let latestRequest = null;

    function setMessage(message, type) {
        formMessage.textContent = message || '';
        formMessage.className = `hint${type ? ` ${type}` : ''}`;
    }

    function setAccess(allowed, message) {
        generateButton.disabled = !allowed;
        accessStatus.textContent = allowed ? '管理员可用' : '暂不可用';
        accessStatus.className = `status-pill ${allowed ? 'ok' : 'error'}`;
        if (message) setMessage(message, allowed ? 'success' : 'error');
    }

    async function readJson(response) {
        const text = await response.text();
        let data = null;
        try { data = text ? JSON.parse(text) : {}; } catch (_) { data = null; }
        if (!response.ok || data?.error?.message) {
            const detail = data?.error?.message || text || `请求失败（HTTP ${response.status}）`;
            const error = new Error(detail);
            error.status = response.status;
            error.raw = text;
            throw error;
        }
        return {data, text};
    }

    async function loadAccount() {
        try {
            const userResponse = await fetch('/api/user', {credentials: 'same-origin'});
            const user = await userResponse.json();
            if (!user?.name) {
                accountText.textContent = '未登录';
                setAccess(false, '请先登录；图片接口需要管理员权限。');
                return;
            }
            accountText.textContent = user.admin === true ? `${user.name} · 管理员` : `${user.name} · 普通用户`;
            if (user.admin !== true) {
                setAccess(false, '当前账号不是管理员，图片生成暂未开放。');
                return;
            }

            const keyResponse = await fetch('/api/gpt5_apikey', {
                method: 'POST',
                credentials: 'same-origin',
                headers: {'Content-Type': 'application/json'},
                body: '{}'
            });
            const {data} = await readJson(keyResponse);
            if (typeof data?.api_key !== 'string' || !data.api_key.startsWith('sk-')) {
                throw new Error('服务器没有返回有效的站内 API Key');
            }
            siteApiKey = data.api_key;
            setAccess(true, '权限检查通过，可以开始生成。');
        } catch (error) {
            accountText.textContent = '状态读取失败';
            setAccess(false, error.message || '无法读取登录状态');
        }
    }

    function updateCount() {
        promptCount.textContent = String(prompt.value.length);
    }

    function createPayload() {
        const text = prompt.value.trim();
        if (!text) throw new Error('请先输入提示词');
        const payload = {
            model: model.value.trim() || 'gpt-image-2',
            prompt: text,
            size: size.value,
            n: Number(count.value)
        };
        if (quality.value) payload.quality = quality.value;
        const extraText = extraJson.value.trim();
        if (extraText) {
            let extra;
            try { extra = JSON.parse(extraText); } catch (_) { throw new Error('附加 JSON 参数格式错误'); }
            if (!extra || Array.isArray(extra) || typeof extra !== 'object') throw new Error('附加 JSON 参数必须是对象');
            Object.assign(payload, extra);
        }
        return payload;
    }

    function showRequest(payload) {
        latestRequest = payload;
        requestSection.hidden = false;
        requestPreview.textContent = JSON.stringify(payload, null, 2);
        copyRequestButton.hidden = false;
    }

    function clearResult() {
        imageGrid.replaceChildren();
        rawDetails.hidden = true;
        rawResponse.textContent = '';
        resultState.hidden = false;
        resultMeta.textContent = '提交请求后，图片会显示在这里';
    }

    function imageSource(item) {
        if (!item || typeof item !== 'object') return null;
        if (typeof item.url === 'string' && item.url) return item.url;
        if (typeof item.b64_json === 'string' && item.b64_json) return `data:image/png;base64,${item.b64_json}`;
        if (typeof item.base64 === 'string' && item.base64) return `data:image/png;base64,${item.base64}`;
        return null;
    }

    function renderImages(data) {
        const items = Array.isArray(data?.data) ? data.data : Array.isArray(data) ? data : [];
        const images = items.map(imageSource).filter(Boolean);
        imageGrid.replaceChildren();
        if (!images.length) {
            resultState.hidden = false;
            resultState.innerHTML = '<p class="no-image">请求已返回，但响应中没有识别到图片 URL 或 Base64。</p><small>请展开“查看原始响应”检查上游返回格式。</small>';
            return 0;
        }
        resultState.hidden = true;
        images.forEach((src, index) => {
            const item = document.createElement('article');
            item.className = 'image-item';
            const image = document.createElement('img');
            image.src = src;
            image.alt = `生成图片 ${index + 1}`;
            image.loading = 'lazy';
            image.addEventListener('error', () => { image.alt = '图片加载失败'; });
            const actions = document.createElement('div');
            actions.className = 'image-actions';
            const label = document.createElement('span');
            label.className = 'image-label';
            label.textContent = `图片 ${index + 1}`;
            const download = document.createElement('a');
            download.href = src;
            download.download = `neuq-image-${Date.now()}-${index + 1}.png`;
            download.target = '_blank';
            download.rel = 'noopener';
            download.textContent = '打开 / 下载';
            actions.append(label, download);
            item.append(image, actions);
            imageGrid.appendChild(item);
        });
        return images.length;
    }

    async function submit(event) {
        event.preventDefault();
        if (!siteApiKey) return setMessage('没有可用的站内 API Key，请刷新页面重试。', 'error');
        let payload;
        try { payload = createPayload(); } catch (error) { return setMessage(error.message, 'error'); }
        showRequest(payload);
        generateButton.classList.add('loading');
        generateButton.disabled = true;
        generateLabel.textContent = '生成中…';
        setMessage('正在等待上游响应，请不要重复提交…');
        const started = performance.now();
        try {
            const response = await fetch('/api/v1/images/generations', {
                method: 'POST',
                credentials: 'same-origin',
                headers: {'Content-Type': 'application/json', 'Authorization': `Bearer ${siteApiKey}`},
                body: JSON.stringify(payload)
            });
            const {data, text} = await readJson(response);
            const elapsed = Math.round(performance.now() - started);
            rawResponse.textContent = text || JSON.stringify(data, null, 2);
            rawDetails.hidden = false;
            const amount = renderImages(data);
            resultMeta.textContent = `HTTP ${response.status} · ${elapsed} ms · 识别到 ${amount} 张图片`;
            requestStatus.textContent = `HTTP ${response.status} · ${elapsed} ms`;
            requestStatus.className = 'request-status ok';
            setMessage(amount ? '生成成功，图片仅保留在当前页面。' : '接口返回成功，但未识别到图片字段。', amount ? 'success' : '');
        } catch (error) {
            const elapsed = Math.round(performance.now() - started);
            rawResponse.textContent = error.raw || error.message || String(error);
            rawDetails.hidden = false;
            resultState.hidden = false;
            resultState.innerHTML = `<p class="no-image">生成失败</p><small>${escapeHtml(error.message || '未知错误')}</small>`;
            resultMeta.textContent = `${error.status ? `HTTP ${error.status}` : '请求错误'} · ${elapsed} ms`;
            requestStatus.textContent = error.status ? `HTTP ${error.status}` : '请求错误';
            requestStatus.className = 'request-status error';
            setMessage(error.message || '生成失败', 'error');
        } finally {
            generateButton.classList.remove('loading');
            generateButton.disabled = false;
            generateLabel.textContent = '生成图片';
        }
    }

    function escapeHtml(value) {
        const div = document.createElement('div');
        div.textContent = value;
        return div.innerHTML;
    }

    prompt.addEventListener('input', updateCount);
    form.addEventListener('submit', submit);
    clearButton.addEventListener('click', () => {
        prompt.value = '';
        extraJson.value = '';
        quality.value = '';
        updateCount();
        requestSection.hidden = true;
        requestStatus.textContent = '';
        clearResult();
        setMessage(siteApiKey ? '已清空，可以输入新的提示词。' : '正在读取管理员权限…');
    });
    copyRequestButton.addEventListener('click', async () => {
        if (!latestRequest) return;
        try {
            await navigator.clipboard.writeText(JSON.stringify(latestRequest, null, 2));
            copyRequestButton.textContent = '已复制';
            setTimeout(() => { copyRequestButton.textContent = '复制请求 JSON'; }, 1300);
        } catch (_) { setMessage('复制失败，请手动选择下方请求 JSON。', 'error'); }
    });

    updateCount();
    loadAccount();
})();
