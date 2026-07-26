let Models = [];
const modelCatalog = new Map();
let isAdmin = false; // 当前用户是否有授权模型权限
let isLoggedIn = false;
let gpt5ApiKey = '';
const chatTitleCache = {};
const chatContentCache = {};
let requestSettings = { max_tokens: '' };
let batchDeleteMode = false;
const selectedHistoryIds = new Set();

function syncChatTitle(id, title) {
    const normalizedTitle = typeof title === 'string' && title ? title : '新对话';
    chatTitleCache[id] = normalizedTitle;
    const historyItem = [...document.querySelectorAll('.history-item')]
        .find(item => item.dataset.id === id);
    const historyTitle = historyItem?.querySelector('.history-title');
    if (historyTitle) historyTitle.textContent = normalizedTitle;
    if (id === currentChatId && currentChatOwned) {
        document.getElementById('currentChatTitleDisplay').textContent = normalizedTitle;
    }
    return normalizedTitle;
}

async function parseGpt5Json(response) {
    const text = await response.text();
    let data;
    try {
        data = text ? JSON.parse(text) : {};
    } catch (error) {
        throw new Error(text || `请求失败（HTTP ${response.status}）`);
    }
    const apiMessage = data?.error?.message;
    if (!response.ok || apiMessage) {
        throw new Error(apiMessage || `请求失败（HTTP ${response.status}）`);
    }
    return data;
}

async function ensureGpt5ApiKey() {
    if (gpt5ApiKey) return gpt5ApiKey;
    if (!isLoggedIn) throw new Error('请先登录后再发送消息');
    const response = await fetch('/api/gpt5_apikey', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: '{}'
    });
    const data = await parseGpt5Json(response);
    if (typeof data.api_key !== 'string' || !data.api_key.startsWith('sk-')) {
        throw new Error('服务器没有返回有效的 API 密钥');
    }
    gpt5ApiKey = data.api_key;
    return gpt5ApiKey;
}

async function initializeGpt5ApiKey() {
    if (!isLoggedIn) return;
    try {
        await ensureGpt5ApiKey();
    } catch (error) {
        console.error('初始化 GPT API 密钥失败:', error);
    }
}

async function resolveGpt5ConversationId(responseId) {
    let lastError = null;
    for (let attempt = 0; attempt < 20; attempt++) {
        try {
            const response = await fetch('/api/gpt5_resolve', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({response_id: responseId})
            });
            const data = await parseGpt5Json(response);
            if (data.con_id) return data.con_id;
        } catch (error) {
            lastError = error;
        }
        await new Promise(resolve => setTimeout(resolve, 100));
    }
    throw lastError || new Error('无法定位本地会话记录');
}

function normalizeModelFormat(format) {
    return format === 'responses' ? 'responses' : 'completions';
}

function availableModelVariants(name, requireAccess = true) {
    const model = modelCatalog.get(name);
    if (!model) return [];
    return model.variants.filter(variant => !requireAccess || variant.isPublic || isAdmin);
}

function modelSupportsFormat(name, format) {
    return availableModelVariants(name).some(variant => variant.format === format);
}

function setSelectedModel(name, preferredFormat = null) {
    const selectButton = document.getElementById('selectButton');
    const allVariants = availableModelVariants(name, false);
    const variants = availableModelVariants(name);
    if (!selectButton || allVariants.length === 0) return;
    if (variants.length === 0) {
        alert('该模型需要授权后使用');
        return;
    }
    const targetFormat = currentChatFormat || preferredFormat;
    const variant = variants.find(item => item.format === targetFormat)
        || variants.find(item => item.format === 'responses')
        || variants[0];
    selectButton.textContent = name;
    selectButton.dataset.provider = variant.provider;
    selectButton.dataset.format = variant.format;
    requestSettings.max_tokens = '';
    showMaxTokensPresets();
    const label = document.querySelector('.max-tokens-label');
    const tokenFieldName = (currentChatFormat || variant.format) === 'responses'
        ? 'max_output_tokens'
        : 'max_tokens';
    if (label) label.textContent = tokenFieldName;
    const customInput = document.getElementById('settingsMaxTokens');
    if (customInput) customInput.placeholder = tokenFieldName;
}

function selectCompatibleModel(format) {
    const currentName = document.getElementById('selectButton')?.textContent;
    if (currentName && modelSupportsFormat(currentName, format)) {
        setSelectedModel(currentName, format);
        return true;
    }
    const model = Models.find(item => availableModelVariants(item.name)
        .some(variant => variant.format === format));
    if (!model) return false;
    setSelectedModel(model.name, format);
    return true;
}

function toggleRequestSettings(event) {
    event.stopPropagation();
    document.getElementById('requestSettingsPopover').classList.toggle('active');
}

function setMaxTokensPreset(value, event) {
    requestSettings.max_tokens = String(value);
    document.querySelectorAll('.max-tokens-option').forEach(button => button.classList.remove('active'));
    event.currentTarget.classList.add('active');
}

function showCustomMaxTokens() {
    document.getElementById('maxTokensPresets').style.display = 'none';
    document.getElementById('maxTokensCustom').style.display = 'flex';
    const input = document.getElementById('settingsMaxTokens');
    input.value = requestSettings.max_tokens || '';
    input.focus();
}

function showMaxTokensPresets() {
    document.getElementById('maxTokensPresets').style.display = 'flex';
    document.getElementById('maxTokensCustom').style.display = 'none';
    const currentValue = requestSettings.max_tokens;
    const options = document.querySelectorAll('.max-tokens-option');
    options.forEach(button => button.classList.remove('active'));
    const selectedOption = [...options].find(button => button.dataset.value === currentValue);
    (selectedOption || options[options.length - 1])?.classList.add('active');
}

function setCustomMaxTokens(value) {
    const number = Number(value);
    requestSettings.max_tokens = Number.isInteger(number) && number > 0 ? String(number) : '';
}

document.addEventListener('click', () => {
    document.getElementById('requestSettingsPopover')?.classList.remove('active');
});
function loaduser() {//加载用户信息
    sessionStorage.setItem(
        'login_next',
        window.location.pathname + window.location.search + window.location.hash
    );
    document.getElementById('drop').href = "/login";
    return fetch('/api/user')
    .then(response => {
        if(!response.ok) throw new Error('Network response was not ok');
        return response.json();
    })
    .then(data => {
        isAdmin = data.admin === true;
        isLoggedIn = Boolean(data.name);
        if (!data.name) {
            gpt5ApiKey = '';
            document.getElementById('drop').style.display = 'block';
            document.getElementById('userContainer').style.display = 'none';
        } else {
            document.getElementById('drop').style.display = 'none';
            document.getElementById('userContainer').style.display = 'flex';
            document.getElementById('username').textContent = data.name;
        }
    })
    .catch(error => {
        console.log("用户信息加载失败：", error);
    });
}
async function handleLogout(event) {//注销登录
    event.preventDefault();
    const response = await fetch('/api/logout', { method: 'POST' });
    location.reload();
}
async function fetchModels() {//获取 AI 模型列表
    try {
        const response = await fetch('/api/gpts2', { method: 'POST' });
        const configs = await response.json();
        modelCatalog.clear();
        const addModelVariant = (name, config, isPublic) => {
            if (typeof name !== 'string' || !name) return;
            if (!modelCatalog.has(name)) {
                modelCatalog.set(name, {name, hasPublic: false, hasPrivate: false, variants: []});
            }
            const model = modelCatalog.get(name);
            if (isPublic) model.hasPublic = true;
            else model.hasPrivate = true;
            const format = normalizeModelFormat(config.format);
            const existing = model.variants.find(variant =>
                variant.provider === config.provider && variant.format === format);
            if (existing) existing.isPublic = existing.isPublic || isPublic;
            else model.variants.push({provider: config.provider, format, isPublic});
        };
        for (const config of configs) {
            (Array.isArray(config.public) ? config.public : [])
                .forEach(name => addModelVariant(name, config, true));
            (Array.isArray(config.private) ? config.private : [])
                .forEach(name => addModelVariant(name, config, false));
        }
        Models = [...modelCatalog.values()];
        const container = document.getElementById('modelsContainer');
        const container2 = document.getElementById('modelsContainer2');
        container.innerHTML = '';
        container2.innerHTML = '';
        const appendModelOption = (model, target) => {
            const li = document.createElement('li');
            li.textContent = model.name;
            li.title = [...new Set(model.variants.map(variant => variant.format))].join(' / ');
            li.onclick = () => {
                setSelectedModel(model.name);
                document.getElementById('selectionModal').style.display = 'none';
            };
            target.appendChild(li);
        };
        Models.filter(model => model.hasPublic).forEach(model => appendModelOption(model, container));
        Models.filter(model => !model.hasPublic && model.hasPrivate)
            .forEach(model => appendModelOption(model, container2));

        const accessibleModels = Models.filter(model => availableModelVariants(model.name).length > 0);
        const defaultModel = accessibleModels.find(model => modelSupportsFormat(model.name, 'responses'))
            || accessibleModels[0];
        if (defaultModel) setSelectedModel(defaultModel.name, 'responses');
        else document.getElementById('selectButton').textContent = '暂无可用模型';
    } catch (error) {
        document.getElementById('modelsContainer').innerHTML = '<li style="color: red">模型加载失败</li>';
    }
}
async function loadUserHistory() {//加载用户历史记录
    const historyList = document.getElementById('historyList');
    batchDeleteMode = false;
    selectedHistoryIds.clear();
    updateBatchDeleteControls();
    try {
        if (!isLoggedIn) {
            historyList.innerHTML = '<div class="no-history-tip">登录后可保存和查看历史对话</div>';
            updateBatchDeleteControls();
            return [];
        }
        const response = await fetch('/api/gpt5_history_list', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: '{}'
        });
        const histories = await parseGpt5Json(response);
        if (!Array.isArray(histories)) throw new Error('历史记录格式错误');
        historyList.innerHTML = '';
        if (histories.length === 0) {
            historyList.innerHTML = '<div class="no-history-tip">暂无历史对话</div>';
            updateBatchDeleteControls();
            return [];
        }
        const ids = [];
        for (const history of [...histories].reverse()) {
            const id = history?.con_id;
            if (typeof id !== 'string' || !id) continue;
            ids.push(id);
            const title = syncChatTitle(id, history.name);
            const itemDiv = document.createElement('a');
            itemDiv.className = `history-item ${id === currentChatId ? 'active' : ''}`;
            itemDiv.dataset.id = id;
            itemDiv.href = `?id=${id}`; 

            const selectCheckbox = document.createElement('input');
            selectCheckbox.type = 'checkbox';
            selectCheckbox.className = 'history-select-checkbox';
            selectCheckbox.title = '选择此对话';
            selectCheckbox.setAttribute('aria-label', '选择此对话');
            selectCheckbox.onclick = (e) => {
                e.stopPropagation();
                toggleHistorySelection(id, selectCheckbox.checked);
            };
            
            const iconSpan = document.createElement('span');
            iconSpan.className = 'history-icon';
            iconSpan.innerHTML = `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"></path></svg>`;
            
            const titleSpan = document.createElement('span');
            titleSpan.className = 'history-title';
            titleSpan.textContent = title;
            
            const renameBtn = document.createElement('button');
            renameBtn.className = 'history-rename-btn';
            renameBtn.innerHTML = `<svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"></path><path d="M18.5 2.5a2.121 2.121 0 1 1 3 3L12 15l-4 1 1-4 9.5-9.5z"></path></svg>`;
            renameBtn.title = '重命名';
            renameBtn.onclick = (e) => { e.stopPropagation(); e.preventDefault(); renameHistoryChat(id, titleSpan); }; 

            const deleteBtn = document.createElement('button');
            deleteBtn.className = 'history-delete-btn';
            deleteBtn.innerHTML = '×';
            deleteBtn.title = '删除对话';
            deleteBtn.onclick = (e) => { e.stopPropagation(); e.preventDefault(); deleteHistoryChat(id); }; 

            itemDiv.appendChild(selectCheckbox);
            itemDiv.appendChild(iconSpan);
            itemDiv.appendChild(titleSpan);
            itemDiv.appendChild(renameBtn);
            itemDiv.appendChild(deleteBtn);

            itemDiv.onclick = (e) => {
                if (e.button === 0 && !e.ctrlKey && !e.metaKey && !e.shiftKey) {
                    e.preventDefault();
                    if (batchDeleteMode) {
                        toggleHistorySelection(id, !selectedHistoryIds.has(id));
                        return;
                    }
                    selectHistoryChat(id);
                }
            };
            
            historyList.appendChild(itemDiv);
        }
        updateBatchDeleteControls();
        return ids;
    } catch (error) {
        historyList.innerHTML = '<div class="no-history-tip" style="color:red">历史记录加载失败</div>';
        updateBatchDeleteControls();
        return [];
    }
}
function enterBatchDeleteMode() {//进入历史记录批量管理模式
    if (!document.querySelector('.history-item')) return;
    batchDeleteMode = true;
    selectedHistoryIds.clear();
    document.getElementById('historyList').style.display = 'flex';
    document.getElementById('historyToggleIcon').style.transform = 'rotate(180deg)';
    updateBatchDeleteControls();
}
function exitBatchDeleteMode() {//退出历史记录批量管理模式
    batchDeleteMode = false;
    selectedHistoryIds.clear();
    updateBatchDeleteControls();
}
function toggleHistorySelection(id, selected) {//选中或取消选中单条历史记录
    if (!batchDeleteMode) return;
    if (selected) selectedHistoryIds.add(id);
    else selectedHistoryIds.delete(id);
    updateBatchDeleteControls();
}
function toggleSelectAllHistory(selected) {//全选或取消全选历史记录
    document.querySelectorAll('.history-item').forEach(item => {
        if (selected) selectedHistoryIds.add(item.dataset.id);
        else selectedHistoryIds.delete(item.dataset.id);
    });
    updateBatchDeleteControls();
}
function updateBatchDeleteControls() {//同步批量管理工具栏和历史项状态
    const items = [...document.querySelectorAll('.history-item')];
    const manageBtn = document.getElementById('historyManageBtn');
    const toolbar = document.getElementById('historyBatchToolbar');
    const selectAll = document.getElementById('historySelectAll');
    const selectedCount = document.getElementById('historySelectedCount');
    const deleteBtn = document.getElementById('historyBatchDeleteBtn');
    if (!manageBtn || !toolbar || !selectAll || !selectedCount || !deleteBtn) return;

    manageBtn.style.display = items.length > 0 && !batchDeleteMode ? '' : 'none';
    toolbar.style.display = batchDeleteMode ? 'flex' : 'none';
    const selectedSize = selectedHistoryIds.size;
    selectedCount.textContent = `已选 ${selectedSize} 项`;
    selectAll.checked = items.length > 0 && selectedSize === items.length;
    selectAll.indeterminate = selectedSize > 0 && selectedSize < items.length;
    deleteBtn.disabled = selectedSize === 0;

    items.forEach(item => {
        const selected = selectedHistoryIds.has(item.dataset.id);
        item.classList.toggle('batch-mode', batchDeleteMode);
        item.classList.toggle('selected', selected);
        const checkbox = item.querySelector('.history-select-checkbox');
        if (checkbox) checkbox.checked = selected;
    });
}
async function renameHistoryChat(id, titleElement) {//重命名历史对话
    const currentName = titleElement ? titleElement.textContent : "新对话";
    const newName = prompt("请输入新的对话标题：", currentName);
    if (newName === null) return;
    const trimmedName = newName.trim();
    if (trimmedName === "") {
        alert("标题不能为空！");
        return;
    }
    try {
        const response = await fetch('/api/gpt5_history_rename', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({con_id: id, title: trimmedName})
        });
        const result = await parseGpt5Json(response);
        if (result.ok === true) {
            syncChatTitle(id, trimmedName);
            if (chatContentCache[id]) chatContentCache[id].name = trimmedName;
        } else alert("重命名失败");
    } catch (error) {
        alert("请求失败，请检查网络！");
    }
}
function renameCurrentChat() {//重命名当前对话
    if (!currentChatId || !currentChatOwned) return;
    const activeTitleSpan = document.querySelector(`.history-item[data-id="${currentChatId}"] .history-title`);
    const currentTitleDisplay = document.getElementById('currentChatTitleDisplay');
    renameHistoryChat(currentChatId, activeTitleSpan || currentTitleDisplay);
}
async function shareCurrentChat() {//公开当前对话并复制分享链接
    if (!currentChatId || !currentChatOwned) return;
    if (!confirm('分享后，任何获得链接的人都可以查看此对话。确定要继续吗？')) return;
    try {
        const response = await fetch('/api/gpt5_share', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({con_id: currentChatId, publish: true})
        });
        const result = await parseGpt5Json(response);
        if (result.ok !== true) throw new Error('分享失败');
        const shareUrl = new URL(window.location.href);
        shareUrl.search = '';
        shareUrl.searchParams.set('id', currentChatId);
        try {
            await navigator.clipboard.writeText(shareUrl.href);
            alert(`分享成功！链接已复制到剪贴板：\n${shareUrl.href}`);
        } catch (error) {
            alert(`分享成功！请手动复制以下链接：\n${shareUrl.href}`);
        }
    } catch (error) {
        alert(error.message || '请求错误，分享失败，请检查网络！');
    }
}
async function fetchGpt5History(id, useCache = true) {
    if (useCache && chatContentCache[id]) return chatContentCache[id];
    const response = await fetch('/api/gpt5_history_get', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({con_id: id})
    });
    const data = await parseGpt5Json(response);
    chatContentCache[id] = data;
    syncChatTitle(id, data.name);
    return data;
}

async function selectHistoryChat(id, updateUrl = true, owned = true) {//选择并载入历史对话
    if (id === currentChatId) return;
    currentChatId = id;
    currentChatOwned = owned;
    if (updateUrl) updateUrlParam(id);
    document.querySelectorAll('.history-item').forEach(el => el.classList.toggle('active', el.dataset.id === id));
    const chatBox = document.getElementById('chatBox');
    chatBox.innerHTML = '<div class="history-loading">正在拉取聊天记录...</div>';
    document.getElementById('sidebarPanel').classList.remove('active');
    document.getElementById('sidebarOverlay').classList.remove('active');

    try {
        const data = await fetchGpt5History(id, false);
        currentChatFormat = normalizeModelFormat(data.format);
        selectCompatibleModel(currentChatFormat);
        if (currentChatFormat === 'responses') renderResponsesHistory(data);
        else {
            currentResponsesInput = [];
            renderGpt5History(data);
        }
        
        renderMathAndCode(chatBox);
        scrollToBottom();

        const title = syncChatTitle(id, data.name);
        document.getElementById('currentChatTitleDisplay').textContent = owned ? title : `${title}（来自 ${data.ownername} 的分享）`;
        
        updateHeaderButtons();
    } catch (error) {
        chatBox.innerHTML = '<div class="history-loading" style="color:red"></div>';
        chatBox.firstElementChild.textContent = error.message || '加载历史聊天失败，请重试！';
    }
}
async function deleteHistoryChat(id) {//删除历史对话
    if (!confirm("确定要删除这个对话记录吗？此操作无法撤销。")) return;
    try {
        if (await requestDeleteHistory(id)) {
            delete chatTitleCache[id];
            if (currentChatId === id) startNewChat();
            await loadUserHistory();
        } else alert("删除失败");
    } catch (error) {
        alert("请求失败，请检查网络！");
    }
}
async function requestDeleteHistory(id) {//调用单条删除接口，供单删和批量删除复用
    const response = await fetch('/api/gpt5_history_delete', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({con_id: id})
    });
    const result = await parseGpt5Json(response);
    if (result.ok) delete chatContentCache[id];
    return result.ok === true;
}
async function deleteSelectedHistory() {//批量删除已选择的历史对话
    const ids = [...selectedHistoryIds];
    if (ids.length === 0) return;
    if (!confirm(`确定要删除选中的 ${ids.length} 条对话记录吗？此操作无法撤销。`)) return;

    const deleteBtn = document.getElementById('historyBatchDeleteBtn');
    deleteBtn.disabled = true;
    deleteBtn.textContent = '删除中...';
    const failedIds = [];
    let deletedCurrentChat = false;
    try {
        for (const id of ids) {
            try {
                if (await requestDeleteHistory(id)) {
                    delete chatTitleCache[id];
                    if (id === currentChatId) deletedCurrentChat = true;
                } else failedIds.push(id);
            } catch (error) {
                failedIds.push(id);
            }
        }
        if (deletedCurrentChat) startNewChat();
        await loadUserHistory();
        if (failedIds.length > 0) {
            alert(`已删除 ${ids.length - failedIds.length} 条，${failedIds.length} 条删除失败，请稍后重试。`);
        }
    } finally {
        deleteBtn.textContent = '删除';
        updateBatchDeleteControls();
    }
}
async function downloadFile(fileId, fileName) {//下载文件附件
    if (!fileId) {
        alert("该文件不支持下载（文件过大或未成功上传）");
        return;
    }
    try {
        const response = await fetch('/api/download_file', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ file_id: fileId })
        });
        if (!response.ok) throw new Error("下载服务异常");
        
        const contentType = response.headers.get('content-type');
        if (contentType && contentType.includes('application/json')) {
            const errResult = await response.json();
            alert("下载失败: " + (errResult.error || "文件不存在或未找到"));
            return;
        }

        const blob = await response.blob();
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = fileName;
        document.body.appendChild(a);
        a.click();
        a.remove();
        window.URL.revokeObjectURL(url);
    } catch (error) {
        alert("下载文件失败: " + error.message);
    }
}
