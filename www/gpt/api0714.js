let Models = [];
let isAdmin = false; // 当前用户是否有授权模型权限
const chatTitleCache = {};
let requestSettings = { max_tokens: '' };

function setSelectedModel(name, provider) {
    const selectButton = document.getElementById('selectButton');
    if (!selectButton) return;
    selectButton.textContent = name;
    selectButton.setAttribute('provider', provider);
    requestSettings.max_tokens = '';
    showMaxTokensPresets();
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
        if (!data.name) {
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
        const models = await response.json();
        Models = models;
        const container = document.getElementById('modelsContainer');
        const container2 = document.getElementById('modelsContainer2');
        container.innerHTML = '';
        container2.innerHTML = '';
        let firstModelAssigned = false;
        let firstPrivateModel = null;

        for (const m of models) {
            const publicModels = Array.isArray(m.public) ? m.public : [];
            const privateModels = Array.isArray(m.private) ? m.private : [];

            publicModels.forEach(modelName => {
                const li = document.createElement('li');
                li.textContent = modelName;
                if (!firstModelAssigned) {
                    firstModelAssigned = true;
                    const selectButton = document.getElementById('selectButton');
                    setSelectedModel(modelName, m.provider);
                }
                li.onclick = () => {
                    const selectButton = document.getElementById('selectButton');
                    setSelectedModel(modelName, m.provider);
                    document.getElementById('selectionModal').style.display = 'none';
                };
                li.setAttribute("provider", m.provider);
                container.appendChild(li);
            });

            privateModels.forEach(modelName => {
                const li = document.createElement('li');
                li.textContent = modelName;
                if (!firstPrivateModel) {
                    firstPrivateModel = { name: modelName, provider: m.provider };
                }
                li.onclick = () => {
                    setSelectedModel(modelName, m.provider);
                    document.getElementById('selectionModal').style.display = 'none';
                };
                li.setAttribute("provider", m.provider);
                container2.appendChild(li);
            });
        }

        if (isAdmin && firstPrivateModel) {
            setSelectedModel(firstPrivateModel.name, firstPrivateModel.provider);
        }
    } catch (error) {
        document.getElementById('modelsContainer').innerHTML = '<li style="color: red">模型加载失败</li>';
    }
}
async function loadUserHistory() {//加载用户历史记录
    const historyList = document.getElementById('historyList');
    try {
        const response = await fetch('/api/gpt_getuserhistory', { method: 'POST' });
        if (!response.ok) throw new Error('Failed');
        const ids = await response.json();
        historyList.innerHTML = '';
        if (!ids || ids.length === 0) {
            historyList.innerHTML = '<div class="no-history-tip">暂无历史对话</div>';
            return [];
        }
        for (let id of ids) {
            const itemDiv = document.createElement('a');
            itemDiv.className = `history-item ${id === currentChatId ? 'active' : ''}`;
            itemDiv.dataset.id = id;
            itemDiv.href = `?id=${id}`; 
            
            const iconSpan = document.createElement('span');
            iconSpan.className = 'history-icon';
            iconSpan.innerHTML = `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"></path></svg>`;
            
            const titleSpan = document.createElement('span');
            titleSpan.className = 'history-title';
            titleSpan.textContent = '载入中...';
            
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

            itemDiv.appendChild(iconSpan);
            itemDiv.appendChild(titleSpan);
            itemDiv.appendChild(renameBtn);
            itemDiv.appendChild(deleteBtn);

            itemDiv.onclick = (e) => {
                if (e.button === 0 && !e.ctrlKey && !e.metaKey && !e.shiftKey) {
                    e.preventDefault();
                    selectHistoryChat(id);
                }
            };
            
            historyList.appendChild(itemDiv);

            fetchTitleForId(id, titleSpan);
        }
        return ids;
    } catch (error) {
        historyList.innerHTML = '<div class="no-history-tip" style="color:red">历史记录加载失败</div>';
        return [];
    }
}
async function fetchTitleForId(id, element) {//获取对话标题
    if (chatTitleCache[id]) {
        element.textContent = chatTitleCache[id];
        if (id === currentChatId) document.getElementById('currentChatTitleDisplay').textContent = chatTitleCache[id];
        return;
    }
    try {
        const response = await fetch('/api/gpt_idname', { method: 'POST', body: id });
        if (response.ok) {
            const title = await response.json();
            const titleText = title.title || "新对话";
            chatTitleCache[id] = titleText;
            element.textContent = titleText;
            if (id === currentChatId) document.getElementById('currentChatTitleDisplay').textContent = titleText;
        } else {
            element.textContent = "New Chat";
        }
    } catch {
        element.textContent = "New Chat";
    }
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
        const response = await fetch('/api/gpt_changename', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ id: id, name: trimmedName })
        });
        const result = await response.text();
        if (result === 'ok') {
            chatTitleCache[id] = trimmedName;
            if (titleElement) titleElement.textContent = trimmedName;
            if (id === currentChatId) {
                document.getElementById('currentChatTitleDisplay').textContent = trimmedName;
            }
        } else {
            alert(result.startsWith("Error:") ? result : "重命名失败：" + result);
        }
    } catch (error) {
        alert("请求失败，请检查网络！");
    }
}
function renameCurrentChat() {//重命名当前对话
    if (!currentChatId) return;
    const activeTitleSpan = document.querySelector(`.history-item[data-id="${currentChatId}"] .history-title`);
    const currentTitleDisplay = document.getElementById('currentChatTitleDisplay');
    renameHistoryChat(currentChatId, activeTitleSpan || currentTitleDisplay);
}
async function shareCurrentChat() {//分享当前对话
    if (!currentChatId) return;
    try {
        const response = await fetch('/api/gpt_share', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ id: currentChatId, publish: true })
        });
        const result = await response.text();
        if (result === 'ok') {
            const shareUrl = location.href;
            try {
                await navigator.clipboard.writeText(shareUrl);
                alert(`分享成功！链接已复制到剪贴板：\n${shareUrl}`);
            } catch (err) {
                alert(`分享成功！请手动复制以下链接：\n${shareUrl}`);
            }
        } else {
            alert(result.startsWith("Error:") ? result : "分享失败：" + result);
        }
    } catch (error) {
        alert("请求错误，分享失败，请检查网络！");
    }
}
async function loadSharedChat(id) {//加载他人分享的对话
    currentChatId = id;
    updateUrlParam(id);
    updateHeaderButtons();
    const chatBox = document.getElementById('chatBox');
    chatBox.innerHTML = '<div class="history-loading">正在拉取共享的聊天记录...</div>';
    document.getElementById('sidebarPanel').classList.remove('active');
    document.getElementById('sidebarOverlay').classList.remove('active');
    try {
        const titleResponse = await fetch('/api/gpt_idname', { method: 'POST', body: id });
        let title = "分享的对话";
        if (titleResponse.ok) {
            const fetchedTitle = await titleResponse.json();
            if (fetchedTitle) title = fetchedTitle.title+"（来自 "+fetchedTitle.owner+" 的分享）";
        }
        document.getElementById('currentChatTitleDisplay').textContent = title;

        const response = await fetch('/api/gpt_idcontent', { method: 'POST', body: id });
        if (!response.ok) throw new Error();
        const data = await response.json();
        
        chatBox.innerHTML = ''; 
        if (data.content && Array.isArray(data.content)) {
            data.content.forEach((msg) => {
                let renderText = msg.content;
                if (Array.isArray(msg.content)) {
                    const imageItem = msg.content.find(item => item.type === 'image_url');
                    if (imageItem) {
                        renderText = `<!--IMAGE_ATTACHMENT:{"id":"","name":"分享图片"}-->${imageItem.image_url.url}`;
                    } else {
                        renderText = JSON.stringify(msg.content);
                    }
                }
                if (msg.role === 'user') renderUserMessage(renderText);
                else if (msg.role === 'assistant') renderAssistantMessage(msg.content);
            });
        }
        
        renderMathAndCode(chatBox);
        scrollToBottom();
    } catch (error) {
        chatBox.innerHTML = '<div class="history-loading" style="color:red">加载聊天失败，可能是链接失效或无查看权限！</div>';
    }
}
async function selectHistoryChat(id, updateUrl = true) {//选择并载入历史对话
    if (id === currentChatId) return;
    currentChatId = id;
    if (updateUrl) updateUrlParam(id);
    document.querySelectorAll('.history-item').forEach(el => el.classList.toggle('active', el.dataset.id === id));
    const chatBox = document.getElementById('chatBox');
    chatBox.innerHTML = '<div class="history-loading">正在拉取聊天记录...</div>';
    document.getElementById('sidebarPanel').classList.remove('active');
    document.getElementById('sidebarOverlay').classList.remove('active');

    try {
        const response = await fetch('/api/gpt_idcontent', { method: 'POST', body: id });
        if (!response.ok) throw new Error();
        const data = await response.json();
        
        chatBox.innerHTML = ''; 
        if (data.content && Array.isArray(data.content)) {
            data.content.forEach((msg) => {
                let renderText = msg.content;
                if (Array.isArray(msg.content)) {
                    const imageItem = msg.content.find(item => item.type === 'image_url');
                    if (imageItem) {
                        renderText = `<!--IMAGE_ATTACHMENT:{"id":"","name":"历史图片"}-->${imageItem.image_url.url}`;
                    } else {
                        renderText = JSON.stringify(msg.content);
                    }
                }
                if (msg.role === 'user') renderUserMessage(renderText);
                else if (msg.role === 'assistant') renderAssistantMessage(msg.content);
            });
        }
        
        renderMathAndCode(chatBox);
        scrollToBottom();

        const activeItem = document.querySelector(`.history-item[data-id="${id}"] .history-title`);
        if (activeItem) document.getElementById('currentChatTitleDisplay').textContent = activeItem.textContent;
        
        updateHeaderButtons();
    } catch (error) {
        chatBox.innerHTML = '<div class="history-loading" style="color:red">加载历史聊天失败，请重试！</div>';
    }
}
async function deleteHistoryChat(id) {//删除历史对话
    if (!confirm("确定要删除这个对话记录吗？此操作无法撤销。")) return;
    try {
        const response = await fetch('/api/gpt_deletehistory', { method: 'POST', body: id });
        if (await response.text() === 'ok') {
            if (currentChatId === id) startNewChat();
            loadUserHistory();
        } else alert("删除失败");
    } catch (error) {
        alert("请求失败，请检查网络！");
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
async function callStreamingApi(response, wrapper, contentDiv, thinkTextarea, startTime) {//读取 gpt_chat 的流式响应
    let rawContent = ''; 
    let streamError = null;
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
            return;
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
}
