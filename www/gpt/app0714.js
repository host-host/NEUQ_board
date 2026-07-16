let currentChatId = null;
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
    updateHeaderButtons();
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

    const welcome = document.getElementById('welcomeBox');
    if (welcome) welcome.remove();

    const sendBtn = document.getElementById('sendButton');
    const loader = document.getElementById('loading_tradeP');
    sendBtn.disabled = true;
    loader.style.display = 'inline-block';

    inputElement.value = '';
    document.getElementById('wordCount').textContent = '0 字符';

    pendingFiles.forEach(file => {
        const meta = JSON.stringify({ id: file.id, name: file.name });
        let filePayload;
        if (file.type === 'image') {
            filePayload = `<!--IMAGE_ATTACHMENT:${meta}-->${file.content}`;
        } else {
            filePayload = `<!--FILE_ATTACHMENT:${meta}-->${file.content}`;
        }
        renderUserMessage(filePayload); 
    });
    pendingFiles = []; 
    updatePendingFilesUI();

    if (userMessage !== '') {
        renderUserMessage(userMessage);
    }
    scrollToBottom();

    const messagePayload = buildPayloadFromDOM();

    const selectButton = document.getElementById('selectButton');
    const provider = selectButton.getAttribute("provider");
    const modelName = selectButton.textContent;
    // 从发送请求前开始计时，包含连接、排队以及首字生成前的等待时间。
    const requestStartTime = new Date().getTime();

    let bodyData = {
        "model": modelName,
        "ida": {
            "provider": provider,
            "id": currentChatId || ""
        },
        "stream": true,
        "messages": messagePayload,
        "stream_options": {"include_usage": true},
        "enable_thinking": true
    };

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
        const response = await fetch('/api/gpt_chat', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(bodyData)
        });
        if (!response.ok) {
            const errorText = await response.text();
            showError(errorText || `请求失败（HTTP ${response.status}）`);
            sendBtn.disabled = false;
            loader.style.display = 'none';
            return;
        }

        const conversationId = response.headers.get('X-Conversation-ID') || '';
        currentChatId = conversationId;
        updateUrlParam(conversationId);
        updateHeaderButtons();

        const { wrapper, contentDiv, thinkTextarea } = reply;

        await callStreamingApi(response, wrapper, contentDiv, thinkTextarea, requestStartTime);

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
    fetchModels();
    const myHistoryIds = await loadUserHistory();
    const urlParams = new URLSearchParams(window.location.search);
    const urlId = urlParams.get('id');
    if (urlId) {
        if (myHistoryIds.includes(urlId)) {
            currentChatId = null;
            await selectHistoryChat(urlId, false);
        } else await loadSharedChat(urlId);
    } else startNewChat();
});
