(function () {
    'use strict';

    const form = document.getElementById('drawForm');
    const prompt = document.getElementById('prompt');
    const promptCount = document.getElementById('promptCount');
    const promptLabel = document.getElementById('promptLabel');
    const generationPlaceholder = prompt.placeholder;
    const sourceImageInput = document.getElementById('sourceImageInput');
    const sourceImagePreview = document.getElementById('sourceImagePreview');
    const sourceImageStatus = document.getElementById('sourceImageStatus');
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
    const loginLink = document.getElementById('loginLink');
    const loginNotice = document.getElementById('loginNotice');
    const logoutButton = document.getElementById('logoutButton');
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
    let sourceImages = [];
    let sourceImageLoading = false;
    let isSubmitting = false;
    let accessAllowed = false;
    let accountLoading = false;

    function setMessage(message, type) {
        formMessage.textContent = message || '';
        formMessage.className = `hint${type ? ` ${type}` : ''}`;
    }

    function setAccess(allowed, message) {
        accessAllowed = allowed;
        syncFormState();
        accessStatus.textContent = allowed ? '可用' : '暂不可用';
        accessStatus.className = `status-pill ${allowed ? 'ok' : 'error'}`;
        if (message) setMessage(message, allowed ? 'success' : 'error');
    }

    function setModelState(message) {
        const option = document.createElement('option');
        option.value = '';
        option.textContent = message;
        model.replaceChildren(option);
        model.disabled = true;
    }

    async function loadModels(isAdmin) {
        try {
            const response = await fetch('/api/gpt5_model_list', {method: 'POST'});
            const {data: config} = await readJson(response);
            if (!config?.model || typeof config.model !== 'object' || Array.isArray(config.model)) {
                throw new Error('无法读取模型列表');
            }
            model.replaceChildren();
            for (const [name, entry] of Object.entries(config.model)) {
                if (entry?.suggest_format !== 'image' || !Array.isArray(entry.provider)) continue;
                if (!entry.provider.some(id => config.provider?.[id] && (isAdmin || config.provider[id].public === true))) continue;
                const option = document.createElement('option');
                option.value = name;
                option.textContent = name;
                model.appendChild(option);
            }
            if (!model.options.length) {
                setModelState('暂无可用图像模型');
                return false;
            }
            model.disabled = false;
            return true;
        } catch (error) {
            setModelState('模型列表加载失败');
            throw error;
        }
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
        accountLoading = true;
        siteApiKey = '';
        setAccess(false);
        try {
            const userResponse = await fetch('/api/user', {credentials: 'same-origin'});
            if (!userResponse.ok) throw new Error(`账户读取失败（HTTP ${userResponse.status}）`);
            const user = await userResponse.json();
            loginLink.hidden = Boolean(user?.name);
            loginNotice.hidden = Boolean(user?.name);
            logoutButton.hidden = !user?.name;
            if (!user?.name) {
                accountText.textContent = '未登录';
                setModelState('请先登录');
                setAccess(false, '请先登录后生成或编辑图片。');
                return;
            }
            accountText.textContent = user.admin === true ? `${user.name} · 管理员` : `${user.name} · 普通用户`;
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
            if (!await loadModels(data.admin === true)) {
                setAccess(false, '暂无可用图像模型');
                return;
            }
            siteApiKey = data.api_key;
            setAccess(true, '权限检查通过，可以开始生成。');
        } catch (error) {
            accountText.textContent = '状态读取失败';
            loginLink.hidden = !logoutButton.hidden;
            loginNotice.hidden = loginLink.hidden;
            if (!model.value) setModelState('加载失败');
            setAccess(false, error.message || '无法读取登录状态');
        } finally {
            accountLoading = false;
            syncFormState();
        }
    }

    async function logout() {
        if (isSubmitting || accountLoading) return;
        accountLoading = true;
        logoutButton.textContent = '退出中…';
        syncFormState();
        try {
            const response = await fetch('/api/logout', {method: 'POST', credentials: 'same-origin'});
            if (!response.ok) throw new Error(`退出失败（HTTP ${response.status}）`);
            await loadAccount();
        } catch (error) {
            setMessage(error.message || '退出失败，请重试。', 'error');
        } finally {
            accountLoading = false;
            logoutButton.textContent = '退出';
            syncFormState();
        }
    }

    function updateCount() {
        promptCount.textContent = String(prompt.value.length);
    }

    function syncFormState() {
        generateButton.disabled = !accessAllowed || accountLoading || isSubmitting || sourceImageLoading;
        logoutButton.disabled = accountLoading || isSubmitting;
        sourceImageInput.disabled = isSubmitting;
        sourceImagePreview.querySelectorAll('button').forEach(button => {
            button.disabled = isSubmitting;
        });
        clearButton.disabled = isSubmitting;
        imageGrid.querySelectorAll('[data-edit-image]').forEach(button => {
            button.disabled = isSubmitting || sourceImageLoading;
        });
        promptLabel.textContent = sourceImages.length ? '修改要求' : '提示词';
        prompt.placeholder = sourceImages.length
            ? sourceImages.length > 1
                ? '例如：保留图 1 的人物，使用图 2 的背景，统一成暖色灯光'
                : '例如：保留人物和构图，把背景改成雨后的校园，使用暖色灯光'
            : generationPlaceholder;
        if (!isSubmitting) {
            generateLabel.textContent = sourceImageLoading ? '读取图片中…' : sourceImages.length ? '编辑图片' : '生成图片';
        }
    }

    function setSourceStatus(message, error = false) {
        sourceImageStatus.textContent = message;
        sourceImageStatus.className = error ? 'hint error' : 'hint';
    }

    function readAsDataUrl(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = () => resolve(reader.result);
            reader.onerror = () => reject(new Error('图片读取失败'));
            reader.onabort = () => reject(new Error('图片读取已取消'));
            reader.readAsDataURL(file);
        });
    }

    function sourceReadyMessage() {
        if (sourceImageLoading) return '正在读取图片…';
        return sourceImages.length ? `已添加 ${sourceImages.length} 张原图，请输入修改要求。` : '';
    }

    function renderSourceImages() {
        const fragment = document.createDocumentFragment();
        sourceImages.forEach((source, index) => {
            const item = document.createElement('div');
            item.className = 'source-image-item';
            const image = document.createElement(source.dataUrl ? 'img' : 'span');
            if (source.dataUrl) {
                image.src = source.dataUrl;
                image.alt = `原图 ${index + 1}`;
            } else {
                image.className = 'source-image-pending';
                image.textContent = '读取中…';
            }
            const info = document.createElement('div');
            info.className = 'source-image-info';
            const name = document.createElement('span');
            name.textContent = `图 ${index + 1} · ${source.name}`;
            name.title = name.textContent;
            const remove = document.createElement('button');
            remove.type = 'button';
            remove.className = 'text-button';
            remove.textContent = '移除';
            remove.setAttribute('aria-label', `移除原图 ${index + 1}`);
            remove.addEventListener('click', () => {
                if (isSubmitting) return;
                sourceImages = sourceImages.filter(image => image !== source);
                renderSourceImages();
                setSourceStatus(sourceReadyMessage());
            });
            info.append(name, remove);
            item.append(image, info);
            fragment.appendChild(item);
        });
        sourceImagePreview.replaceChildren(fragment);
        sourceImagePreview.hidden = !sourceImages.length;
        sourceImageLoading = sourceImages.some(image => !image.dataUrl);
        syncFormState();
    }

    async function addSourceImages(entries) {
        if (isSubmitting || !entries.length) return false;
        const pending = entries.map(entry => ({...entry, dataUrl: ''}));
        sourceImages.push(...pending);
        renderSourceImages();
        setSourceStatus(sourceReadyMessage());
        const results = await Promise.allSettled(pending.map(async entry => {
            const dataUrl = await entry.load();
            if (typeof dataUrl !== 'string' || !dataUrl.startsWith('data:image/')) throw new Error('无效的图片');
            const picture = new Image();
            picture.src = dataUrl;
            await picture.decode();
            return dataUrl;
        }));
        if (!pending.some(image => sourceImages.includes(image))) return false;
        const errors = [];
        let added = false;
        results.forEach((result, index) => {
            const image = pending[index];
            if (!sourceImages.includes(image)) return;
            if (result.status === 'fulfilled') {
                image.dataUrl = result.value;
                added = true;
            } else {
                sourceImages = sourceImages.filter(source => source !== image);
                errors.push(image.failureMessage);
            }
            delete image.load;
        });
        renderSourceImages();
        setSourceStatus(errors.length ? `${errors.join(' ')} ${sourceReadyMessage()}`.trim() : sourceReadyMessage(), errors.length > 0);
        return added;
    }

    function loadImageFiles(files) {
        return addSourceImages(Array.from(files, file => ({
            name: file.name || '粘贴的图片',
            load: () => {
                if (!file.type.startsWith('image/')) throw new Error('请选择图片文件');
                return readAsDataUrl(file);
            },
            failureMessage: `无法读取“${file.name || '粘贴的图片'}”，请重新选择图片文件。`
        })));
    }

    function clearSourceImages() {
        sourceImages = [];
        sourceImageInput.value = '';
        renderSourceImages();
        setSourceStatus('');
    }

    async function editResult(src, index) {
        const loaded = await addSourceImages([{
            name: `生成图片 ${index + 1}`,
            load: async () => {
                if (src.startsWith('data:image/')) return src;
                const response = await fetch(src, {credentials: 'omit', referrerPolicy: 'no-referrer'});
                if (!response.ok) throw new Error('图片下载失败');
                return readAsDataUrl(await response.blob());
            },
            failureMessage: '无法读取这张结果图，请先“打开 / 下载”保存，再选择图片上传。'
        }]);
        if (loaded) {
            prompt.focus();
            prompt.select();
        }
    }

    function createPayload() {
        const text = prompt.value.trim();
        if (!text) throw new Error('请先输入提示词');
        if (model.disabled || !model.value) throw new Error('请选择可用的图像模型');
        const payload = {
            model: model.value,
            prompt: text,
            size: size.value,
            n: Number(count.value)
        };
        // 暂按上游接受 image 为单个 Data URL 或 Data URL 数组处理。
        if (sourceImages.length) {
            payload.image = sourceImages.length === 1 ? sourceImages[0].dataUrl : sourceImages.map(image => image.dataUrl);
        }
        if (quality.value) payload.quality = quality.value;
        const extraText = extraJson.value.trim();
        if (extraText) {
            let extra;
            try { extra = JSON.parse(extraText); } catch (_) { throw new Error('附加 JSON 参数格式错误'); }
            if (!extra || Array.isArray(extra) || typeof extra !== 'object') throw new Error('附加 JSON 参数必须是对象');
            Object.assign(payload, extra);
        }
        payload.model = model.value;
        return payload;
    }

    function showRequest(payload) {
        latestRequest = payload;
        requestSection.hidden = false;
        requestPreview.textContent = JSON.stringify(payload, (key, value) => {
            if (typeof value === 'string' && value.startsWith('data:image/') && value.length > 120) {
                return `${value.slice(0,80)}…（图片内容已缩略，共 ${value.length} 字符）`;
            }
            return value;
        }, 2);
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
            const edit = document.createElement('button');
            edit.type = 'button';
            edit.className = 'text-button';
            edit.dataset.editImage = '';
            edit.textContent = '继续编辑';
            edit.disabled = isSubmitting || sourceImageLoading;
            edit.addEventListener('click', () => editResult(src, index));
            actions.append(label, download, edit);
            item.append(image, actions);
            imageGrid.appendChild(item);
        });
        return images.length;
    }

    async function submit(event) {
        event.preventDefault();
        if (isSubmitting || sourceImageLoading || accountLoading) return;
        if (!siteApiKey) return setMessage('没有可用的站内 API Key，请刷新页面重试。', 'error');
        let payload;
        try { payload = createPayload(); } catch (error) { return setMessage(error.message, 'error'); }
        showRequest(payload);
        const action = payload.image ? '编辑' : '生成';
        isSubmitting = true;
        syncFormState();
        generateButton.classList.add('loading');
        generateLabel.textContent = `${action}中…`;
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
            setMessage(amount ? `${action}成功，图片仅保留在当前页面。` : '接口返回成功，但未识别到图片字段。', amount ? 'success' : '');
        } catch (error) {
            const elapsed = Math.round(performance.now() - started);
            rawResponse.textContent = error.raw || error.message || String(error);
            rawDetails.hidden = false;
            resultState.hidden = false;
            resultState.innerHTML = `<p class="no-image">${action}失败</p><small>${escapeHtml(error.message || '未知错误')}</small>`;
            resultMeta.textContent = `${error.status ? `HTTP ${error.status}` : '请求错误'} · ${elapsed} ms`;
            requestStatus.textContent = error.status ? `HTTP ${error.status}` : '请求错误';
            requestStatus.className = 'request-status error';
            setMessage(error.message || `${action}失败`, 'error');
        } finally {
            isSubmitting = false;
            generateButton.classList.remove('loading');
            syncFormState();
        }
    }

    function escapeHtml(value) {
        const div = document.createElement('div');
        div.textContent = value;
        return div.innerHTML;
    }

    logoutButton.addEventListener('click', logout);
    prompt.addEventListener('input', updateCount);
    sourceImageInput.addEventListener('change', () => {
        const files = Array.from(sourceImageInput.files);
        sourceImageInput.value = '';
        loadImageFiles(files);
    });
    document.addEventListener('paste', event => {
        if (isSubmitting || event.defaultPrevented) return;
        const files = Array.from(event.clipboardData?.items || [])
            .filter(item => item.kind === 'file' && item.type.startsWith('image/'))
            .map(item => item.getAsFile()).filter(Boolean);
        if (!files.length) return;
        event.preventDefault();
        loadImageFiles(files);
    });
    form.addEventListener('submit', submit);
    clearButton.addEventListener('click', () => {
        prompt.value = '';
        extraJson.value = '';
        quality.value = '';
        clearSourceImages();
        updateCount();
        latestRequest = null;
        requestSection.hidden = true;
        requestPreview.textContent = '';
        copyRequestButton.hidden = true;
        requestStatus.textContent = '';
        clearResult();
        if (siteApiKey) setMessage('已清空，可以输入新的提示词。');
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
