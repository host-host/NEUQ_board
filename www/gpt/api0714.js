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
        for (const history of histories) {
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

function historyMessageText(content) {
    if (typeof content === 'string') return content;
    if (!Array.isArray(content)) return content == null ? '' : JSON.stringify(content);
    return content.map(part => part?.text || part?.content || '').filter(Boolean).join('\n') || JSON.stringify(content);
}

function renderGpt5History(data) {
    const chatBox = document.getElementById('chatBox');
    chatBox.innerHTML = '';
    if (!Array.isArray(data.content)) return;
    data.content.forEach(msg => {
        if (msg.role === 'user') {
            const imageItem = Array.isArray(msg.content)
                ? msg.content.find(item => item?.type === 'image_url' && item?.image_url?.url)
                : null;
            const text = imageItem
                ? `<!--IMAGE_ATTACHMENT:{"id":"","name":"历史图片"}-->${imageItem.image_url.url}`
                : historyMessageText(msg.content);
            renderUserMessage(text);
        } else if (msg.role === 'assistant') {
            let text = historyMessageText(msg.content);
            if (!text && Array.isArray(msg.tool_calls)) text = JSON.stringify(msg.tool_calls, null, 2);
            renderAssistantMessage(text);
        }
    });
}
function responsesMessageText(item) {
    if (typeof item?.content === 'string') return item.content;
    if (!Array.isArray(item?.content)) return '';
    return item.content.map(part => part?.text || part?.refusal || '').filter(Boolean).join('\n');
}
function responsesReasoningText(item) {
    if (item?.type !== 'reasoning' || !Array.isArray(item.summary)) return '';
    return item.summary.map(part => part?.text || '').filter(Boolean).join('\n');
}
function isResponsesToolCall(item) {
    return item?.type === 'custom_tool_call' || item?.type === 'function_call';
}
function isResponsesToolOutput(item) {
    return item?.type === 'custom_tool_call_output' || item?.type === 'function_call_output';
}
function responsesToolOutputText(item) {
    if (!item) return '';
    if (typeof item.output === 'string') return item.output;
    if (!Array.isArray(item.output)) return item.output == null ? '' : JSON.stringify(item.output, null, 2);
    return item.output.map(part => part?.text || part?.content || '')
        .filter(Boolean).join('') || JSON.stringify(item.output, null, 2);
}
function isHiddenResponsesContext(item) {
    if (item?.role !== 'user') return false;
    const text = responsesMessageText(item).trim();
    return text.startsWith('<environment_context>') && text.endsWith('</environment_context>');
}
function responseRenderableEntries(input) {
    const entries = [];
    input.forEach((item, index) => {
        if (!isHiddenResponsesContext(item) &&
            (item?.role === 'user' || item?.role === 'assistant' || isResponsesToolCall(item))) {
            entries.push({item, index});
        }
    });
    return entries;
}
function rebindResponsesDomItems() {
    const wrappers = [...document.querySelectorAll('.chat-message-wrapper')];
    const entries = responseRenderableEntries(currentResponsesInput);
    const validIndexes = new Set(entries.map(entry => entry.index));
    const usedIndexes = new Set();
    wrappers.forEach(wrapper => {
        const index = Number(wrapper.dataset.responseInputIndex);
        if (Number.isInteger(index) && validIndexes.has(index)) usedIndexes.add(index);
        else delete wrapper.dataset.responseInputIndex;
    });
    wrappers.filter(wrapper => wrapper.dataset.responseInputIndex === undefined).forEach(wrapper => {
        const role = wrapper.dataset.role;
        const entry = entries.find(candidate => {
            if (usedIndexes.has(candidate.index)) return false;
            const candidateRole = candidate.item.role || 'assistant';
            return candidateRole === role;
        });
        if (!entry) return;
        bindResponseItem(wrapper, entry.index);
        usedIndexes.add(entry.index);
    });
}
function renderResponsesHistory(data) {
    const chatBox = document.getElementById('chatBox');
    chatBox.innerHTML = '';
    currentResponsesInput = Array.isArray(data.content)
        ? JSON.parse(JSON.stringify(data.content))
        : [];
    const toolOutputs = new Map();
    currentResponsesInput.forEach((item, index) => {
        if (!isResponsesToolOutput(item)) return;
        const callId = item.call_id || '';
        if (!callId) return;
        if (!toolOutputs.has(callId)) toolOutputs.set(callId, []);
        toolOutputs.get(callId).push({item, index});
    });
    const renderedOutputIndexes = new Set();
    let pendingReasoning = '';
    currentResponsesInput.forEach((item, index) => {
        const reasoning = responsesReasoningText(item);
        if (reasoning) {
            pendingReasoning += `${pendingReasoning ? '\n' : ''}${reasoning}`;
            return;
        }
        if (isHiddenResponsesContext(item)) return;
        let wrapper;
        if (item.role === 'user') {
            if (Array.isArray(item.content)) {
                let renderedParts = 0;
                item.content.forEach((part, partIndex) => {
                    const imageUrl = (part?.type === 'input_image' || part?.type === 'image_url')
                        ? part.image_url || part.url || ''
                        : '';
                    const text = part?.text || '';
                    if (!imageUrl && !text) return;
                    const partWrapper = imageUrl
                        ? renderUserMessage(`<!--IMAGE_ATTACHMENT:{"id":"","name":"历史图片"}-->${imageUrl}`)
                        : renderUserMessage(text);
                    bindResponseItem(partWrapper, index, partIndex, 1);
                    renderedParts++;
                });
                if (renderedParts > 0) return;
            }
            wrapper = renderUserMessage(responsesMessageText(item));
        } else if (item.role === 'assistant') {
            wrapper = renderAssistantMessage(responsesMessageText(item), pendingReasoning);
            pendingReasoning = '';
        } else if (isResponsesToolCall(item)) {
            const matches = item.call_id ? toolOutputs.get(item.call_id) || [] : [];
            const output = matches.find(entry => !renderedOutputIndexes.has(entry.index));
            if (output) renderedOutputIndexes.add(output.index);
            wrapper = renderToolCall(item, responsesToolOutputText(output?.item), pendingReasoning);
            pendingReasoning = '';
        } else if (isResponsesToolOutput(item) && !renderedOutputIndexes.has(index)) {
            wrapper = renderToolCall(
                {call_id: item.call_id, name: '工具输出'},
                responsesToolOutputText(item),
                pendingReasoning
            );
            pendingReasoning = '';
        } else return;
        bindResponseItem(wrapper, index);
    });
}
async function refreshResponsesState(id) {
    const data = await fetchGpt5History(id, false);
    if (data.format !== 'responses') throw new Error('会话格式与 Responses 请求不一致');
    currentResponsesInput = Array.isArray(data.content)
        ? JSON.parse(JSON.stringify(data.content))
        : [];
    rebindResponsesDomItems();
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
async function callStreamingApi(response, wrapper, contentDiv, thinkTextarea, startTime) {//读取 Chat Completions 流式响应
    let rawContent = ''; 
    let streamError = null;
    let responseId = '';
    try {
        if (!response.ok) {
            throw new Error('API Request Failed!');
        }
        
        const contentType = response.headers.get('content-type');
        if (!contentType || !contentType.includes('text/event-stream')){
            const upstreamText = await response.text();
            const message = `${upstreamText || '服务器没有返回内容'}\n请刷新后再试`;
            wrapper.dataset.raw = message;
            contentDiv.textContent = message;
            contentDiv.style.whiteSpace = 'pre-wrap';
            contentDiv.style.display = 'block';
            return '';
        }

        const reader = response.body.getReader();
        const decoder = new TextDecoder('utf-8');
        let hasRenderedNormalContent = false;
        let thinkCounter = 0;
        let buffer = '';

        while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            buffer += decoder.decode(value, { stream: true });
            const lines = buffer.split('\n');
            buffer = lines.pop(); 
            for (const line of lines) {
                let trimmedLine = line.trim();
                if (!trimmedLine) continue;
                if (trimmedLine === 'data: [DONE]') continue;

                try {
                    if (trimmedLine.startsWith('data: ')) {
                        const parsed = JSON.parse(trimmedLine.slice(6));
                        if (!responseId && parsed.id) responseId = parsed.id;
                        const deltaContent = parsed.choices[0]?.delta?.content || '';
                        const reasoningContent = parsed.choices[0]?.delta?.reasoning_content || '';
                        const time = ((new Date().getTime() - startTime) / 1000).toFixed(2);

                        if (parsed.usage) {
                            let tokens = parsed.usage.completion_tokens;
                            const calcContainer = wrapper.querySelector('.contentcalc');
                            if (calcContainer) {
                                let statsSpan = calcContainer.querySelector('.token-stats-item');
                                if (!statsSpan) {
                                    statsSpan = document.createElement('span');
                                    statsSpan.className = 'token-stats-item';
                                    calcContainer.insertBefore(statsSpan, calcContainer.firstChild);
                                }
                                statsSpan.textContent = `${tokens} tokens | ${(tokens / time).toFixed(2)} tokens/s`;
                            }
                        }

                        if (deltaContent !== '') {
                            rawContent += deltaContent;
                            wrapper.dataset.raw = rawContent;
                            contentDiv.innerHTML = safeParseMarkdown(rawContent);
                            contentDiv.style.display = 'block';

                            contentDiv.querySelectorAll('pre code').forEach((block) => {
                                if (!block.classList.contains('hljs')) hljs.highlightElement(block);
                            });

                            if (!hasRenderedNormalContent) {
                                hasRenderedNormalContent = true;
                                thinkTextarea.style.height = thinkTextarea.scrollHeight + 'px';
                                thinkTextarea.style.display = 'none';
                                const thinkHeader = thinkTextarea.previousElementSibling;
                                if (thinkHeader) thinkHeader.textContent = `▶ 思考过程 (耗时 ${time} 秒)`;
                            }
                        }

                        if (reasoningContent !== '') {
                            const thinkHeader = thinkTextarea.previousElementSibling;
                            if (thinkHeader) thinkHeader.style.display = 'flex';
                            thinkTextarea.style.display = 'block';
                            thinkTextarea.value += reasoningContent;
                            if (++thinkCounter % 3 == 0) thinkTextarea.style.height = thinkTextarea.scrollHeight + 'px';
                        }
                    }
                } catch (error) {
                    console.error("解析流式JSON出错:", error);
                }
            }
        }
    } catch (error) {
        console.error("请求或读取流失败:", error);
        streamError = error;
    }

    if (streamError) {
        const errorMessage = streamError.message || '流式响应中断';
        if (rawContent.trim()) {
            rawContent += `\n\n${errorMessage}\n请刷新后再试`;
            wrapper.dataset.raw = rawContent;
            contentDiv.innerHTML = safeParseMarkdown(rawContent);
            contentDiv.style.display = 'block';
        } else {
            const message = `${errorMessage}\n请刷新后再试`;
            wrapper.dataset.raw = message;
            contentDiv.textContent = message;
            contentDiv.style.whiteSpace = 'pre-wrap';
            contentDiv.style.display = 'block';
        }
    } else if (!rawContent.trim()) {
        const message = '服务器没有返回内容\n请刷新后再试';
        wrapper.dataset.raw = message;
        contentDiv.textContent = message;
        contentDiv.style.whiteSpace = 'pre-wrap';
        contentDiv.style.display = 'block';
    }

    renderMathAndCode(contentDiv);
    scrollToBottom();
    return responseId;
}

async function callResponsesStreamingApi(response, wrapper, contentDiv, thinkTextarea, startTime) {
    let rawContent = '';
    let streamError = null;
    let responseId = '';
    try {
        const contentType = response.headers.get('content-type');
        if (!response.ok || !contentType?.includes('text/event-stream')) {
            const text = await response.text();
            throw new Error(text || `请求失败（HTTP ${response.status}）`);
        }
        const reader = response.body.getReader();
        const decoder = new TextDecoder('utf-8');
        let buffer = '';
        let thinkCounter = 0;
        let hasRenderedContent = false;
        while (true) {
            const {done, value} = await reader.read();
            if (done) break;
            buffer += decoder.decode(value, {stream: true});
            const lines = buffer.split('\n');
            buffer = lines.pop();
            for (const line of lines) {
                const trimmedLine = line.trim();
                if (!trimmedLine.startsWith('data:')) continue;
                const payload = trimmedLine.slice(5).trim();
                if (!payload || payload === '[DONE]') continue;
                let event;
                try { event = JSON.parse(payload); }
                catch (error) {
                    console.error('解析 Responses 流式 JSON 出错:', error);
                    continue;
                }
                const eventResponse = event.response || {};
                if (!responseId) responseId = eventResponse.id || event.response_id || '';
                const type = event.type || '';
                if (type === 'response.output_text.delta' && typeof event.delta === 'string') {
                    rawContent += event.delta;
                    wrapper.dataset.raw = rawContent;
                    contentDiv.innerHTML = safeParseMarkdown(rawContent);
                    contentDiv.style.display = 'block';
                    if (!hasRenderedContent) {
                        hasRenderedContent = true;
                        const elapsed = ((Date.now() - startTime) / 1000).toFixed(2);
                        const thinkHeader = thinkTextarea.previousElementSibling;
                        if (thinkTextarea.value) {
                            thinkTextarea.style.display = 'none';
                            if (thinkHeader) thinkHeader.textContent = `▶ 思考过程 (耗时 ${elapsed} 秒)`;
                        }
                    }
                } else if (type === 'response.output_text.done' && !rawContent && typeof event.text === 'string') {
                    rawContent = event.text;
                    wrapper.dataset.raw = rawContent;
                    contentDiv.innerHTML = safeParseMarkdown(rawContent);
                    contentDiv.style.display = 'block';
                } else if (type === 'response.refusal.delta' && typeof event.delta === 'string') {
                    rawContent += event.delta;
                    wrapper.dataset.raw = rawContent;
                    contentDiv.textContent = rawContent;
                    contentDiv.style.display = 'block';
                } else if ((type === 'response.reasoning_summary_text.delta' ||
                            type === 'response.reasoning_text.delta') && typeof event.delta === 'string') {
                    const thinkHeader = thinkTextarea.previousElementSibling;
                    if (thinkHeader) thinkHeader.style.display = 'flex';
                    thinkTextarea.style.display = 'block';
                    thinkTextarea.value += event.delta;
                    if (++thinkCounter % 3 === 0) thinkTextarea.style.height = `${thinkTextarea.scrollHeight}px`;
                } else if (type === 'response.completed') {
                    responseId = eventResponse.id || responseId;
                    const tokens = eventResponse.usage?.output_tokens;
                    if (Number.isFinite(tokens)) {
                        const elapsed = Math.max((Date.now() - startTime) / 1000, 0.01);
                        const stats = document.createElement('span');
                        stats.className = 'token-stats-item';
                        stats.textContent = `${tokens} tokens | ${(tokens / elapsed).toFixed(2)} tokens/s`;
                        wrapper.querySelector('.contentcalc')?.prepend(stats);
                    }
                } else if (type === 'error' || type === 'response.failed') {
                    const message = event.error?.message || eventResponse.error?.message || event.message;
                    streamError = new Error(message || 'Responses API 请求失败');
                }
            }
        }
    } catch (error) {
        streamError = error;
    }
    if (streamError) {
        const message = rawContent
            ? `${rawContent}\n\n${streamError.message}`
            : streamError.message;
        wrapper.dataset.raw = message;
        contentDiv.innerHTML = safeParseMarkdown(message);
        contentDiv.style.display = 'block';
    } else if (!rawContent.trim()) {
        wrapper.dataset.raw = '服务器没有返回文本内容';
        contentDiv.textContent = wrapper.dataset.raw;
        contentDiv.style.display = 'block';
    }
    renderMathAndCode(contentDiv);
    scrollToBottom();
    return streamError ? '' : responseId;
}
