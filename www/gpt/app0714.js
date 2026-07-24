let currentChatId = null;
let currentChatOwned = false;
let currentChatFormat = null;
let currentResponsesInput = [];
let pendingFiles = []; // 暂存待发送文件 [{id, name, content, type, large}]
function toggleHistoryList() {//展开/折叠历史记录
    const list = document.getElementById('historyList');
    const icon = document.getElementById('historyToggleIcon');
    if (list.style.display === 'none' || list.style.display === '') {
        list.style.display = 'flex';
        icon.style.transform = 'rotate(180deg)';
    } else {
        list.style.display = 'none';
        icon.style.transform = 'rotate(0deg)';
    }
}
function toggleSidebar() {//打开/关闭侧边栏
    const sidebar = document.getElementById('sidebarPanel');
    const overlay = document.getElementById('sidebarOverlay');
    sidebar.classList.toggle('active');
    overlay.classList.toggle('active');
}
function startNewChat() {//新建对话
    currentChatId = null;
    currentChatOwned = false;
    currentChatFormat = null;
    currentResponsesInput = [];
    updateUrlParam(null);
    document.getElementById('chatBox').innerHTML = `
        <div class="welcome-box" id="welcomeBox">
            <h2>您好！我是您的 AI 助手</h2>
            <p>请在下方输入框中输入您的问题，或者从左侧历史记录中继续先前的对话。</p>
        </div>
    `;
    document.getElementById('currentChatTitleDisplay').textContent = "新对话";
    document.querySelectorAll('.history-item').forEach(el => el.classList.remove('active'));
    document.getElementById('sidebarPanel').classList.remove('active');
    document.getElementById('sidebarOverlay').classList.remove('active');
    if (typeof selectCompatibleModel === 'function') {
        if (!selectCompatibleModel('responses')) selectCompatibleModel('completions');
    }
    updateHeaderButtons();
}
function responseContentPartsFromWrapper(wrapper) {
    const raw = wrapper.dataset.raw || '';
    let parsed = null;
    if (raw.trim().startsWith('[')) {
        try { parsed = JSON.parse(raw); } catch (error) { parsed = null; }
    }
    if (Array.isArray(parsed)) {
        const content = parsed.map(part => {
            if (part?.type === 'image_url' && part.image_url?.url) {
                return {type: 'input_image', image_url: part.image_url.url};
            }
            if (typeof part?.text === 'string') return {type: 'input_text', text: part.text};
            return null;
        }).filter(Boolean);
        if (content.length > 0) return content;
    }
    return [{type: 'input_text', text: raw}];
}
function bindResponseItem(wrapper, index, partStart = null, partCount = null) {
    if (!wrapper) return;
    wrapper.dataset.responseInputIndex = String(index);
    if (partStart === null) {
        delete wrapper.dataset.responsePartStart;
        delete wrapper.dataset.responsePartCount;
    } else {
        wrapper.dataset.responsePartStart = String(partStart);
        wrapper.dataset.responsePartCount = String(partCount);
    }
}
function appendNewResponseInput(wrappers) {
    const unbound = wrappers.filter(wrapper => wrapper.dataset.responseInputIndex === undefined);
    if (unbound.length === 0) return;
    const index = currentResponsesInput.length;
    const content = [];
    unbound.forEach(wrapper => {
        const parts = responseContentPartsFromWrapper(wrapper);
        bindResponseItem(wrapper, index, content.length, parts.length);
        content.push(...parts);
    });
    currentResponsesInput.push({role: 'user', content});
}
function updateResponseHistoryItem(wrapper, text) {
    const index = Number(wrapper.dataset.responseInputIndex);
    const item = currentResponsesInput[index];
    if (!Number.isInteger(index) || !item || item.type === 'function_call') return;
    if (Array.isArray(item.content)) {
        const start = Number(wrapper.dataset.responsePartStart);
        const count = Number(wrapper.dataset.responsePartCount);
        const scopedContent = Number.isInteger(start) && Number.isInteger(count)
            ? item.content.slice(start, start + count)
            : item.content;
        const textPart = scopedContent.find(part => part?.type === 'input_text' || part?.type === 'output_text');
        if (textPart) textPart.text = text;
        else item.content = text;
    } else item.content = text;
}
function removeResponseHistoryItem(wrapper) {
    const index = Number(wrapper.dataset.responseInputIndex);
    if (!Number.isInteger(index) || !currentResponsesInput[index]) return;
    const sameItemWrappers = [...document.querySelectorAll('.chat-message-wrapper[data-response-input-index]')]
        .filter(item => item !== wrapper && Number(item.dataset.responseInputIndex) === index);
    const start = Number(wrapper.dataset.responsePartStart);
    const count = Number(wrapper.dataset.responsePartCount);
    if (sameItemWrappers.length > 0 && Array.isArray(currentResponsesInput[index].content) &&
        Number.isInteger(start) && Number.isInteger(count)) {
        currentResponsesInput[index].content.splice(start, count);
        sameItemWrappers.forEach(item => {
            const itemStart = Number(item.dataset.responsePartStart);
            if (Number.isInteger(itemStart) && itemStart > start) {
                item.dataset.responsePartStart = String(itemStart - count);
            }
        });
        return;
    }
    currentResponsesInput.splice(index, 1);
    document.querySelectorAll('.chat-message-wrapper[data-response-input-index]').forEach(item => {
        const oldIndex = Number(item.dataset.responseInputIndex);
        if (oldIndex > index) item.dataset.responseInputIndex = String(oldIndex - 1);
    });
}
async function processFile(file) {//处理上传的文件/图片核心逻辑
    if (!file) return;

    const allowedExtensions = ['.txt', '.md', '.json', '.csv', '.pdf', '.docx', '.xlsx', '.png', '.jpg', '.jpeg', '.gif', '.webp'];
    const imageExtensions = ['.png', '.jpg', '.jpeg', '.gif', '.webp'];
    const fileName = file.name;
    const fileExtension = fileName.substring(fileName.lastIndexOf('.')).toLowerCase();
    
    if (!allowedExtensions.includes(fileExtension)) {
        alert("只支持以下文件格式: " + allowedExtensions.join(', '));
        return;
    }

    const sendBtn = document.getElementById('sendButton');
    const loader = document.getElementById('loading_tradeP');
    sendBtn.disabled = true;
    loader.style.display = 'inline-block';

    try {
        let extractedContent = '';
        const isImage = imageExtensions.includes(fileExtension);

        if (isImage) {
            extractedContent = await compressImage(file, 900, 900, 0.8);
        } else {
            const arrayBufferForParsing = await file.arrayBuffer();
            if (fileExtension === '.pdf') {
                extractedContent = await extractTextFromPDF(arrayBufferForParsing);
            } else if (fileExtension === '.docx') {
                extractedContent = await extractTextFromDocx(arrayBufferForParsing);
            } else if (fileExtension === '.xlsx') {
                extractedContent = await extractTextFromXlsx(arrayBufferForParsing);
            } else {
                extractedContent = await file.text();
            }
        }
        const MAX_UPLOAD_SIZE = 1 * 1024 * 1024; // 1MB
        if (file.size > MAX_UPLOAD_SIZE || isImage) {
            pendingFiles.push({
                id: null,            
                name: file.name,
                content: extractedContent,
                type: isImage ? 'image' : 'text',
                large: true          
            });
        } else{
            const response = await fetch('/api/uploads_file', {
                method: 'POST',
                body: file            
            });
            const result = await response.json();
            if (result.error) {
                alert("上传失败: " + result.error);
                return;
            }
            if (result.file_id) {
                pendingFiles.push({
                    id: result.file_id,
                    name: file.name,
                    content: extractedContent,
                    type: isImage ? 'image' : 'text',
                    large: false
                });
            }
        }
        updatePendingFilesUI();
    } catch (error) {
        console.error("处理文件失败: ", error);
        alert("处理或上传文件出错，请确保网络正常或文件未损坏。");
    } finally {
        sendBtn.disabled = false;
        loader.style.display = 'none';
    }
}
async function handleFileSelect(input) {//用户点击按钮选择文件
    const files = input.files;
    if (!files || files.length === 0) return;
    for (let i = 0; i < files.length; i++)await processFile(files[i]);
    input.value = '';
}
function removePendingFile(idx) {//删除待发送附件
    pendingFiles.splice(idx, 1);
    updatePendingFilesUI();
}
async function sendMessage() {//发送消息的核心入口
    const inputElement = document.getElementById('input');
    const userMessage = inputElement.value.trim();
    
    if (userMessage === '' && pendingFiles.length === 0) return;

    const selectButton = document.getElementById('selectButton');
    const modelName = selectButton.textContent;
    const selectedFormat = normalizeModelFormat(selectButton.dataset.format);
    if (!modelCatalog.has(modelName)) {
        alert('请先选择可用模型');
        return;
    }
    if (currentChatFormat && !modelSupportsFormat(modelName, currentChatFormat)) {
        alert(`当前对话使用 ${currentChatFormat} 格式，请选择支持该格式的模型。`);
        return;
    }
    const requestFormat = currentChatFormat || selectedFormat;

    const welcome = document.getElementById('welcomeBox');
    if (welcome) welcome.remove();

    const sendBtn = document.getElementById('sendButton');
    const loader = document.getElementById('loading_tradeP');
    sendBtn.disabled = true;
    loader.style.display = 'inline-block';

    inputElement.value = '';
    document.getElementById('wordCount').textContent = '0 字符';

    const newUserWrappers = [];
    pendingFiles.forEach(file => {
        const meta = JSON.stringify({ id: file.id, name: file.name });
        let filePayload;
        if (file.type === 'image') {
            filePayload = `<!--IMAGE_ATTACHMENT:${meta}-->${file.content}`;
        } else {
            filePayload = `<!--FILE_ATTACHMENT:${meta}-->${file.content}`;
        }
        newUserWrappers.push(renderUserMessage(filePayload));
    });
    pendingFiles = []; 
    updatePendingFilesUI();

    if (userMessage !== '') {
        newUserWrappers.push(renderUserMessage(userMessage));
    }
    scrollToBottom();

    if (requestFormat === 'responses') appendNewResponseInput(newUserWrappers);
    // 从发送请求前开始计时，包含连接、排队以及首字生成前的等待时间。
    const requestStartTime = new Date().getTime();

    let bodyData = requestFormat === 'responses' ? {
        model: modelName,
        stream: true,
        instructions: 'You are a helpful assistant.',
        input: currentResponsesInput
    } : {
        model: modelName,
        stream: true,
        messages: buildPayloadFromDOM(),
        stream_options: {include_usage: true},
        enable_thinking: true
    };
    const maxTokens = Number(requestSettings.max_tokens);
    if (Number.isInteger(maxTokens) && maxTokens > 0) {
        if (requestFormat === 'responses') bodyData.max_output_tokens = maxTokens;
        else bodyData.max_tokens = maxTokens;
    }

    const reply = renderAssistantPlaceholder();
    scrollToBottom();
    const showError = (errorMessage) => {
        const message = `${errorMessage}\n请刷新后再试`;
        reply.wrapper.dataset.raw = message;
        reply.contentDiv.textContent = message;
        reply.contentDiv.style.whiteSpace = 'pre-wrap';
        reply.contentDiv.style.display = 'block';
        scrollToBottom();
    };

    try {
        const apiKey = await ensureGpt5ApiKey();
        const endpoint = requestFormat === 'responses'
            ? '/api/v1/responses'
            : '/api/v1/chat/completions';
        const response = await fetch(endpoint, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${apiKey}`
            },
            body: JSON.stringify(bodyData)
        });
        if (!response.ok) {
            const errorText = await response.text();
            showError(errorText || `请求失败（HTTP ${response.status}）`);
            sendBtn.disabled = false;
            loader.style.display = 'none';
            return;
        }

        const { wrapper, contentDiv, thinkTextarea } = reply;
        const responseId = requestFormat === 'responses'
            ? await callResponsesStreamingApi(response, wrapper, contentDiv, thinkTextarea, requestStartTime)
            : await callStreamingApi(response, wrapper, contentDiv, thinkTextarea, requestStartTime);
        if (responseId) {
            try {
                currentChatId = await resolveGpt5ConversationId(responseId);
                currentChatOwned = true;
                currentChatFormat = requestFormat;
                updateUrlParam(currentChatId);
                updateHeaderButtons();
                if (requestFormat === 'responses') await refreshResponsesState(currentChatId);
            } catch (error) {
                console.error('解析本地会话 ID 失败:', error);
            }
        }
        await loadUserHistory();
    } catch (error) {
        console.error('发送错误:', error);
        showError(error.message || '网络请求失败');
    } finally {
        sendBtn.disabled = false;
        loader.style.display = 'none';
    }
}
function buildPayloadFromDOM() {//组装发送给 AI 的上下文数据
    let payload = [{"role": "system", "content": "You are a helpful assistant."}];
    const wrappers = document.querySelectorAll('.chat-message-wrapper');
    wrappers.forEach(el => {
        if (el.dataset.raw) {
            let contentVal = el.dataset.raw;
            if (contentVal.trim().startsWith('[') && contentVal.trim().endsWith(']')) {
                try {
                    contentVal = JSON.parse(contentVal);
                } catch(e) {
                    console.error("解析图片多模态格式出错:", e);
                }
            }
            payload.push({
                "role": el.dataset.role,
                "content": contentVal
            });
        }
    });
    return payload;
}
document.getElementById('input').addEventListener('input', function() {
    document.getElementById('wordCount').textContent = `${this.value.length} 字符`;
});
document.getElementById('input').addEventListener('paste', async function(event) {
    const items = event.clipboardData && event.clipboardData.items;
    if (!items) return;
    for (let i = 0; i < items.length; i++) {
        if (items[i].type.indexOf('image') !== -1) {
            event.preventDefault();
            const file = items[i].getAsFile();
            if (file) {
                const fileExt = file.type.split('/')[1] || 'png';
                const renamedFile = new File([file], `pasted_image_${Date.now()}.${fileExt}`, { type: file.type });
                await processFile(renamedFile);
            }
        }
    }
});
(function setupDragAndDrop() {//拖拽文件到输入框上传
    const dropZone = document.getElementById('inputWrapBox');
    if (!dropZone) return;
    let dragDepth = 0; // 计数进入/离开，避免子元素冒泡导致高亮闪烁

    // 仅当拖入的是文件时才响应（忽略选中文本拖拽等）
    const hasFiles = (e) => e.dataTransfer && Array.from(e.dataTransfer.types || []).includes('Files');

    dropZone.addEventListener('dragenter', (e) => {
        if (!hasFiles(e)) return;
        e.preventDefault();
        dragDepth++;
        dropZone.classList.add('drag-over');
    });
    dropZone.addEventListener('dragover', (e) => {
        if (!hasFiles(e)) return;
        e.preventDefault();
        e.dataTransfer.dropEffect = 'copy';
    });
    dropZone.addEventListener('dragleave', (e) => {
        if (!hasFiles(e)) return;
        dragDepth--;
        if (dragDepth <= 0) {
            dragDepth = 0;
            dropZone.classList.remove('drag-over');
        }
    });
    dropZone.addEventListener('drop', async (e) => {
        if (!hasFiles(e)) return;
        e.preventDefault();
        dragDepth = 0;
        dropZone.classList.remove('drag-over');
        const files = e.dataTransfer.files;
        if (!files || files.length === 0) return;
        for (let i = 0; i < files.length; i++) await processFile(files[i]);
    });
})();
document.addEventListener('DOMContentLoaded', async () => {
    await loaduser(); // 先拿到权限信息，fetchModels 据此决定默认模型
    await Promise.all([fetchModels(), initializeGpt5ApiKey()]);
    const myHistoryIds = await loadUserHistory();
    const urlParams = new URLSearchParams(window.location.search);
    const urlId = urlParams.get('id');
    if (urlId) {
        currentChatId = null;
        await selectHistoryChat(urlId, false, myHistoryIds.includes(urlId));
    } else startNewChat();
});
