marked.setOptions({
    breaks: true,
    smartypants: false, 
    highlight: function(code, lang) {
        const language = hljs.getLanguage(lang) ? lang : 'plaintext';
        return hljs.highlight(code, { language }).value;
    }
});
function safeParseMarkdown(markdownText) {
    if (!markdownText) return '';

    let processedText = markdownText;


    const mathBlocks = [];
    const codeBlocks = [];
    let mathCounter = 0;
    let codeCounter = 0;
    let protectedText = processedText;

    // 保护多行代码块
    protectedText = protectedText.replace(/(```[\s\S]*?```)/g, (match) => {
        const id = `<!--CODE_BLOCK_PLACEHOLDER_${codeCounter++}-->`;
        codeBlocks.push({ id, content: match });
        return id;
    });
    // 保护单行行内代码
    protectedText = protectedText.replace(/(`[^`\n]+?`)/g, (match) => {
        const id = `<!--CODE_BLOCK_PLACEHOLDER_${codeCounter++}-->`;
        codeBlocks.push({ id, content: match });
        return id;
    });

    // 提取并隔离所有 LaTeX 公式
    function protectMath(mathContent, isBlock) {
        const id = `<!--MATH_PLACEHOLDER_${mathCounter++}-->`;
        mathBlocks.push({ id, content: mathContent, isBlock });
        return id;
    }

    // 1. 匹配块级公式 \[ ... \]
    protectedText = protectedText.replace(/\\\[([\s\S]*?)\\\]/g, (match, g1) => protectMath(g1, true));
    
    // 2. 匹配块级公式 $$ ... $$
    protectedText = protectedText.replace(/\$\$([\s\S]*?)\$\$/g, (match, g1) => protectMath(g1, true));
    
    // 3. 匹配行内公式 \( ... \)
    protectedText = protectedText.replace(/\\\(([\s\S]*?)\\\)/g, (match, g1) => protectMath(g1, false));
    
    // 4. 匹配行内公式 $ ... $ 
    protectedText = protectedText.replace(/\$(?!\s)([^\$\n]{1,500}?)(?<!\s)\$/g, (match, g1) => {return protectMath(g1, false);});

    // 手动处理 CJK 兼容的加粗 ---
    protectedText = protectedText.replace(/\*\*([^\*]+?(?:\*[^\*]+?)*)\*\*/g, '<strong>$1</strong>');
    // 收窄双下划线加粗范围，只匹配纯字母/数字/中文，彻底防止污染 URL 链接
    protectedText = protectedText.replace(/__([a-zA-Z0-9\u4e00-\u9fa5]+?)__/g, '<strong>$1</strong>');

    // 还原
    codeBlocks.forEach(({ id, content }) => {
        protectedText = protectedText.split(id).join(content);
    });
    let html = marked.parse(protectedText);
    mathBlocks.forEach(({ id, content, isBlock }) => {
        try {
            const renderedHtml = katex.renderToString(content.trim(), {
                displayMode: isBlock,
                throwOnError: false,
                trust: false,
                output: 'html'
            });
            
            // 安全回填：【关键修复】利用正则兼容带任意空格/换行符的 <p> 包裹层
            if (isBlock) {
                const blockRegex = new RegExp(`<p>\\s*${id}\\s*</p>`, 'g');
                if (blockRegex.test(html)) {
                    // 使用匿名函数返回值回填，完美避免特殊字符（如 $&）引发的 JS replace 漏洞
                    html = html.replace(blockRegex, () => renderedHtml);
                    return;
                }
            }
            
            // 安全回填行内公式
            html = html.split(id).join(renderedHtml);
            
        } catch (err) {
            console.error("KaTeX rendering error:", err);
            const errorSpan = `<span class="katex-error" style="color:var(--danger-color);">${err.message}</span>`;
            html = html.split(id).join(errorSpan);
        }
    });
    return DOMPurify.sanitize(html, {
        FORBID_TAGS: ['script', 'iframe', 'object', 'embed', 'form', 'base'],
        ALLOW_DATA_ATTR: false,
        ALLOW_UNKNOWN_PROTOCOLS: false
    });
}

// 渲染用户消息气泡
function renderUserMessage(text) {
    const chatBox = document.getElementById('chatBox');
    const wrapper = document.createElement('div');
    wrapper.className = 'chat-message-wrapper';
    wrapper.dataset.role = 'user';
    wrapper.dataset.raw = text;
    
    const attachmentRegex = /^<!--FILE_ATTACHMENT:(\{.*?\})-->([\s\S]*)$/;
    const imageRegex = /^<!--IMAGE_ATTACHMENT:(\{.*?\})-->([\s\S]*)$/;

    const matchFile = text.match(attachmentRegex);
    const matchImage = text.match(imageRegex);

    if (matchImage) {
        let fileMeta = { id: "", name: "Unknown" };
        try { fileMeta = JSON.parse(matchImage[1]); } catch (e) { console.error(e); }
        const base64Data = matchImage[2];
        const apiPayload = [{ "type": "image_url", "image_url": { "url": base64Data } }];
        wrapper.dataset.raw = JSON.stringify(apiPayload);

        wrapper.innerHTML = `
            <div class="chat-user-bubble-container">
                <div class="chat-user" style="background: transparent; border: none; padding: 0;">
                    <div class="image-attachment-card">
                        <img class="chat-uploaded-image" />
                        <div class="image-attachment-info">
                            <span class="image-name"></span>
                        </div>
                    </div>
                </div>
            </div>
            <div class="action-bar user-actions">
                <button class="del-btn copy-btn" style="color:var(--danger-color);">🗑️ 删除</button>
            </div>
        `;
        const image = wrapper.querySelector('.chat-uploaded-image');
        const imageName = wrapper.querySelector('.image-name');
        imageName.textContent = `🖼️ ${fileMeta.name || '未知图片'}`;
        if (base64Data) {
            image.src = base64Data;
            image.alt = fileMeta.name || '图片附件';
            image.addEventListener('click', () => zoomImage(base64Data));
        } else {
            image.alt = '无效图片地址';
            image.style.display = 'none';
        }
        chatBox.appendChild(wrapper);
        setupEditDelete(wrapper, 'user');
    } else if (matchFile) {
        let fileMeta = { id: "", name: "Unknown" };
        try { fileMeta = JSON.parse(matchFile[1]); } catch (e) { console.error(e); }
        const textLength = matchFile[2].length;
        const hasId = fileMeta.id && fileMeta.id !== "";
        const isLarge = !hasId;

        wrapper.innerHTML = `
            <div class="chat-user-bubble-container">
                <div class="chat-user" style="background: transparent; border: none; padding: 0;">
                    <div class="file-attachment-card">
                        <span class="file-attachment-icon">📄</span>
                        <div class="file-attachment-info">
                            <span class="file-attachment-name"></span>
                            <span class="file-attachment-size">
                                ${isLarge ? '(已提取文本)' : `大小: ${(textLength / 1024).toFixed(2)} KB (已提取文本)`}
                            </span>
                        </div>
                    </div>
                </div>
            </div>
            <div class="action-bar user-actions">
                <button class="del-btn copy-btn" style="color:var(--danger-color);">🗑️ 删除</button>
            </div>
        `;
        const fileCard = wrapper.querySelector('.file-attachment-card');
        wrapper.querySelector('.file-attachment-name').textContent = fileMeta.name || '未知文件';
        if (hasId) {
            fileCard.title = '点击下载此文件';
            fileCard.addEventListener('click', () => downloadFile(fileMeta.id, fileMeta.name || 'download'));
        } else {
            fileCard.style.opacity = '0.7';
            fileCard.style.cursor = 'not-allowed';
        }
        chatBox.appendChild(wrapper);
        setupEditDelete(wrapper, 'user');
    } else {
        wrapper.innerHTML = `
            <div class="chat-user-bubble-container">
                <div class="chat-user">
                    <div class="user-text-display"></div>
                    <textarea class="user-text-area" style="display:none;"></textarea>
                </div>
            </div>
            <div class="action-bar user-actions">
                <button class="edit-btn copy-btn">✏️ 修改</button>
                <button class="del-btn copy-btn" style="color:var(--danger-color);">🗑️ 删除</button>
            </div>
        `;
        chatBox.appendChild(wrapper);
        
        const displayDiv = wrapper.querySelector('.user-text-display');
        const ta = wrapper.querySelector('.user-text-area');
        displayDiv.textContent = text;
        ta.value = text;

        setupEditDelete(wrapper, 'user');
    }
    return wrapper;
}

// 渲染 AI 消息气泡
function renderAssistantMessage(text, reasoning = '') {
    const chatBox = document.getElementById('chatBox');
    const wrapper = document.createElement('div');
    wrapper.className = 'chat-message-wrapper';
    wrapper.dataset.role = 'assistant';
    wrapper.dataset.raw = text;
    wrapper.innerHTML = `
        ${reasoning ? '<div class="think">▶ 思考过程</div><textarea class="chat-think" readonly></textarea>' : ''}
        <div class="chat-content-markdown">${safeParseMarkdown(text)}</div>
        <div class="contentcalc">
            <div class="action-bar">
                <button class="copy-btn copy-text-btn">📋 复制</button>
                <button class="edit-btn copy-btn">✏️ 修改</button>
                <button class="del-btn copy-btn" style="color:var(--danger-color);">🗑️ 删除</button>
            </div>
        </div>
    `;
    if (reasoning) {
        const thinkHeader = wrapper.querySelector('.think');
        const thinkTextarea = wrapper.querySelector('.chat-think');
        thinkTextarea.value = reasoning;
        thinkTextarea.style.display = 'none';
        thinkHeader.onclick = () => {
            thinkTextarea.style.display = thinkTextarea.style.display === 'none' ? 'block' : 'none';
        };
    }
    chatBox.appendChild(wrapper);
    setupEditDelete(wrapper, 'assistant');
    return wrapper;
}

function renderToolCall(item, outputText = '', reasoning = '') {
    const chatBox = document.getElementById('chatBox');
    const wrapper = document.createElement('div');
    wrapper.className = 'chat-message-wrapper tool-call-wrapper';
    wrapper.dataset.role = 'assistant';

    const details = document.createElement('details');
    details.className = 'tool-call';
    const summary = document.createElement('summary');
    summary.className = 'tool-call-summary';
    summary.innerHTML = `
        <svg class="tool-call-icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><polyline points="4 17 10 11 4 5"></polyline><line x1="12" y1="19" x2="20" y2="19"></line></svg>
        <span class="tool-call-name"></span>
        <span class="tool-call-status"></span>
        <svg class="tool-call-chevron" width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><polyline points="9 18 15 12 9 6"></polyline></svg>
    `;
    summary.querySelector('.tool-call-name').textContent = item?.name || '工具调用';
    const status = item?.status || (outputText ? 'completed' : '');
    const statusText = status === 'completed' ? '已完成' : status === 'in_progress' ? '运行中' : status;
    const statusElement = summary.querySelector('.tool-call-status');
    statusElement.textContent = statusText;
    if (!statusText) statusElement.style.display = 'none';

    const body = document.createElement('div');
    body.className = 'tool-call-body';
    const appendField = (label, value, className = '') => {
        if (value === '' || value == null) return;
        const field = document.createElement('div');
        field.className = `tool-call-field ${className}`.trim();
        const title = document.createElement('div');
        title.className = 'tool-call-field-label';
        title.textContent = label;
        const content = document.createElement('pre');
        content.className = 'tool-call-field-content';
        content.textContent = value;
        field.append(title, content);
        body.appendChild(field);
    };
    appendField('调用 ID', item?.call_id || '', 'tool-call-id');
    appendField('输入', item?.input ?? item?.arguments ?? '');
    appendField('输出', outputText);
    details.append(summary, body);

    if (reasoning) {
        const thinkHeader = document.createElement('div');
        thinkHeader.className = 'think';
        thinkHeader.textContent = '▶ 思考过程';
        const thinkTextarea = document.createElement('textarea');
        thinkTextarea.className = 'chat-think';
        thinkTextarea.readOnly = true;
        thinkTextarea.value = reasoning;
        thinkTextarea.style.display = 'none';
        thinkHeader.onclick = () => {
            thinkTextarea.style.display = thinkTextarea.style.display === 'none' ? 'block' : 'none';
        };
        wrapper.append(thinkHeader, thinkTextarea);
    }
    wrapper.appendChild(details);
    chatBox.appendChild(wrapper);
    return wrapper;
}

// 渲染 AI 正在回复的占位气泡
function renderAssistantPlaceholder() {
    const chatBox = document.getElementById('chatBox');
    const wrapper = document.createElement('div');
    wrapper.className = 'chat-message-wrapper';
    wrapper.dataset.role = 'assistant';
    wrapper.dataset.raw = '';

    wrapper.innerHTML = `
        <div class="think" style="display:none;" onclick="this.nextElementSibling.style.display = this.nextElementSibling.style.display === 'none' ? 'block' : 'none';">▶ 正在思考...</div>
        <textarea class="chat-think" style="display:none;"></textarea>
        <div class="chat-content-markdown" style="display:none;"></div>
        <div class="contentcalc">
            <div class="action-bar">
                <button class="copy-btn copy-text-btn">📋 复制</button>
                <button class="edit-btn copy-btn">✏️ 修改</button>
                <button class="del-btn copy-btn" style="color:var(--danger-color);">🗑️ 删除</button>
            </div>
        </div>
    `;
    
    chatBox.appendChild(wrapper);
    setupEditDelete(wrapper, 'assistant');

    return { 
        wrapper, 
        contentDiv: wrapper.querySelector('.chat-content-markdown'),
        thinkTextarea: wrapper.querySelector('.chat-think')
    };
}

// 设置消息的修改和删除按钮
function setupEditDelete(wrapper, role) {
    const editBtn = wrapper.querySelector('.edit-btn');
    const delBtn = wrapper.querySelector('.del-btn');
    const copyBtn = wrapper.querySelector('.copy-text-btn');
    let isEditing = false;
    
    if (copyBtn) {
        copyBtn.onclick = async () => {
            try {
                await navigator.clipboard.writeText(wrapper.dataset.raw);
                copyBtn.innerHTML = '✅ 已复制'; copyBtn.style.color = 'green';
                setTimeout(() => { copyBtn.innerHTML = '📋 复制'; copyBtn.style.color = ''; }, 2000);
            } catch (err) { alert('复制失败'); }
        };
    }

    if (delBtn) {
        delBtn.onclick = () => {
            if(confirm("确定删除这条聊天记录吗？")) {
                if (typeof removeResponseHistoryItem === 'function') removeResponseHistoryItem(wrapper);
                wrapper.remove();
            }
        };
    }

    if (editBtn) {
        editBtn.onclick = () => {
            if (role === 'user') {
                const displayDiv = wrapper.querySelector('.user-text-display');
                const ta = wrapper.querySelector('.user-text-area');
                const chatUser = wrapper.querySelector('.chat-user');
                if (!isEditing) {
                    if (chatUser) chatUser.style.width = '100%';
                    displayDiv.style.display = 'none';
                    ta.style.display = 'block';
                    ta.style.height = 'auto';
                    ta.style.height = ta.scrollHeight + 'px';
                    ta.focus();
                    editBtn.innerHTML = '💾 保存';
                    isEditing = true;
                } else {
                    const newText = ta.value;
                    displayDiv.textContent = newText;
                    wrapper.dataset.raw = newText;
                    if (typeof updateResponseHistoryItem === 'function') updateResponseHistoryItem(wrapper, newText);
                    displayDiv.style.display = 'block';
                    ta.style.display = 'none';
                    if (chatUser) chatUser.style.width = '';
                    editBtn.innerHTML = '✏️ 修改';
                    isEditing = false;
                }
            } else {
                const contentDiv = wrapper.querySelector('.chat-content-markdown');
                let editTa = wrapper.querySelector('.assistant-edit-area');
                
                if (!isEditing) {
                    if (!editTa) {
                        editTa = document.createElement('textarea');
                        editTa.className = 'assistant-edit-area';
                        contentDiv.parentNode.insertBefore(editTa, contentDiv);
                    }
                    editTa.value = wrapper.dataset.raw;
                    editTa.style.display = 'block';
                    contentDiv.style.display = 'none';
                    
                    editTa.style.height = 'auto';
                    editTa.style.height = editTa.scrollHeight + 10 + 'px';

                    editBtn.innerHTML = '💾 保存';
                    isEditing = true;
                } else {
                    wrapper.dataset.raw = editTa.value;
                    if (typeof updateResponseHistoryItem === 'function') updateResponseHistoryItem(wrapper, editTa.value);
                    contentDiv.innerHTML = safeParseMarkdown(editTa.value);
                    editTa.style.display = 'none';
                    contentDiv.style.display = 'block';
                    editBtn.innerHTML = '✏️ 修改';
                    isEditing = false;
                }
            }
        };
    }
}

// 静态 HTML 中渲染数学公式和代码高亮的后置处理（现仅处理代码块的高亮，公式在生成 HTML 时已处理完毕）
function renderMathAndCode(element) {
    element.querySelectorAll('pre').forEach((preBlock) => {
        // 确保 pre 容器具备相对定位属性，以便定位绝对布局的复制按钮
        preBlock.style.position = 'relative';

        const codeBlock = preBlock.querySelector('code');
        if (!codeBlock) return;

        // 避免重复添加复制按钮
        if (preBlock.querySelector('.code-copy-btn')) return;

        // 代码高亮
        if (!codeBlock.classList.contains('hljs')) {
            hljs.highlightElement(codeBlock);
        }

        // 创建复制按钮
        const copyBtn = document.createElement('button');
        copyBtn.className = 'code-copy-btn';
        copyBtn.innerHTML = '📋 复制';
        copyBtn.title = '复制整个代码块';
        copyBtn.style.cssText = `
            position: absolute;
            top: 8px;
            right: 8px;
            background: #f8fafc;
            border: 1px solid #cbd5e1;
            border-radius: 4px;
            cursor: pointer;
            font-size: 11px;
            padding: 3px 8px;
            z-index: 10;
            color: var(--text-muted);
            transition: all 0.2s;
        `;

        // 按钮悬停交互
        copyBtn.onmouseenter = () => {
            copyBtn.style.background = '#e2e8f0';
            copyBtn.style.color = 'var(--text-dark)';
        };
        copyBtn.onmouseleave = () => {
            copyBtn.style.background = '#f8fafc';
            copyBtn.style.color = 'var(--text-muted)';
        };

        // 复制事件处理
        copyBtn.onclick = async () => {
            try {
                await navigator.clipboard.writeText(codeBlock.innerText);
                copyBtn.innerHTML = '✅ 已复制';
                setTimeout(() => {
                    copyBtn.innerHTML = '📋 复制';
                }, 2000);
            } catch (err) {
                alert('复制失败');
            }
        };

        preBlock.appendChild(copyBtn);
    });
}

// 更新待发送附件的显示
function updatePendingFilesUI() {
    const container = document.getElementById('pendingFilesContainer');
    if (pendingFiles.length === 0) {
        container.style.display = 'none';
        container.innerHTML = '';
        return;
    }
    container.style.display = 'flex';
    container.innerHTML = pendingFiles.map((file, idx) => {
        const isImg = file.type === 'image';
        return `
            <div class="pending-file-chip">
                <span>${isImg ? '🖼️' : '📄'} ${file.name}</span>
                ${file.large ? `<span style="color:#888;font-size:0.8em;"></span>` : ''}
                <span class="remove-btn" onclick="removePendingFile(${idx})">&times;</span>
            </div>
        `;
    }).join('');
}

// 刷新顶部按钮
function updateHeaderButtons() {
    const renameBtn = document.getElementById('renameChatBtn');
    const shareBtn = document.getElementById('shareChatBtn');
    if (currentChatId && currentChatOwned) {
        renameBtn.style.display = 'inline-block';
        shareBtn.style.display = 'flex';
    } else {
        renameBtn.style.display = 'none';
        shareBtn.style.display = 'none';
    }
}
