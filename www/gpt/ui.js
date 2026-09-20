// 在 Markdown 分词时识别公式，让代码块、行内代码和转义符由 marked 处理。
function readChatMath(source) {
    const opening = /^(\$\$|\\\[|\\\(|\$)/.exec(source)?.[0];
    if (!opening) return;
    const closing = opening === '\\[' ? '\\]' : opening === '\\(' ? '\\)' : opening;
    const display = opening === '$$' || opening === '\\[';
    if (opening === '$' && /\s/.test(source[1] || ' ')) return;

    for (let index = opening.length; index < source.length; index++) {
        if (opening === '$' && source[index] === '\n') return;
        if (source.startsWith(closing, index)) {
            // 不把金额中的美元符号或相邻的块级分隔符当作行内公式结尾。
            if (opening === '$' && (/\s/.test(source[index - 1]) || /[\d$]/.test(source[index + 1] || ''))) return;
            const text = source.slice(opening.length, index);
            if (!text.trim()) return;
            return {raw: source.slice(0, index + closing.length), text, display};
        }
        // 跳过成对的转义字符，包含公式里的 \$、\\ 和转义的括号。
        if (source[index] === '\\') index++;
    }
}

function renderChatMath(token) {
    return katex.renderToString(token.text.trim(), {
        displayMode: token.display,
        throwOnError: false,
        trust: false,
        output: 'html'
    });
}

const chatMarkdown = new marked.Marked({
    gfm: true,
    breaks: true,
    extensions: [
        {
            name: 'chatMathBlock',
            level: 'block',
            start(source) { return source.search(/^ {0,3}(?:\$\$|\\\[)/m); },
            tokenizer(source) {
                const prefix = /^ {0,3}(?=\$\$|\\\[)/.exec(source);
                if (!prefix) return;
                const math = readChatMath(source.slice(prefix[0].length));
                if (!math) return;
                const ending = /^[ \t]*(?:\n|$)/.exec(source.slice(prefix[0].length + math.raw.length));
                if (!ending) return;
                return {...math, type: 'chatMathBlock', raw: prefix[0] + math.raw + ending[0]};
            },
            renderer(token) { return renderChatMath(token) + '\n'; }
        },
        {
            name: 'chatMathInline',
            level: 'inline',
            start(source) { return source.search(/\$|\\[\[(]/); },
            tokenizer(source) {
                const math = readChatMath(source);
                if (math) return {...math, type: 'chatMathInline'};
            },
            renderer: renderChatMath
        }
    ],
    tokenizer: {
        emStrong(source, maskedSource, previousCharacter) {
            const token = marked.Tokenizer.prototype.emStrong.call(this, source, maskedSource, previousCharacter);
            if (token) return token;
            // 中文紧邻引号时仍允许加粗，如：用**“找规律”**的方法。
            const match = /^\*\*(?!\*)(\S[^\n]*?)\*\*(?!\*)/.exec(source);
            if (!match || !/[\u3400-\u9fff]/.test(match[1]) || /\s$|`/.test(match[1])) return false;
            return {type: 'strong', raw: match[0], text: match[1], tokens: this.lexer.inlineTokens(match[1])};
        }
    }
});

function safeParseMarkdown(markdownText) {
    if (!markdownText) return '';
    return DOMPurify.sanitize(chatMarkdown.parse(markdownText), {
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
        updateAssistantCollapse(wrapper);
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
        updateAssistantCollapse(wrapper);
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
        updateAssistantCollapse(wrapper);
    }
    return wrapper;
}

function resizeThinkTextarea(textarea) {
    textarea.style.height = 'auto';
    textarea.style.height = `${textarea.scrollHeight}px`;
}

function toggleThinking(header) {
    const textarea = header.nextElementSibling;
    const expanded = textarea.style.display === 'none';
    textarea.style.display = expanded ? 'block' : 'none';
    if (expanded) resizeThinkTextarea(textarea);
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
        thinkHeader.onclick = () => toggleThinking(thinkHeader);
    }
    chatBox.appendChild(wrapper);
    setupEditDelete(wrapper, 'assistant');
    updateAssistantCollapse(wrapper);
    return wrapper;
}

// AI 与用户文本消息都提供折叠按钮，默认保持展开。
function updateAssistantCollapse(wrapper, reset = false) {
    const contentDiv = wrapper?.querySelector('.chat-content-markdown, .user-text-display');
    if (!contentDiv) return;

    requestAnimationFrame(() => {
        if (contentDiv.style.display === 'none') return;

        let toggleBtn = wrapper.querySelector('.assistant-collapse-btn');
        if (!toggleBtn) {
            toggleBtn = document.createElement('button');
            toggleBtn.type = 'button';
            toggleBtn.className = 'assistant-collapse-btn copy-btn';
            wrapper.querySelector('.contentcalc .action-bar, .action-bar.user-actions')?.append(toggleBtn);
            toggleBtn.onclick = () => {
                const willExpand = contentDiv.classList.contains('is-collapsed');
                contentDiv.classList.toggle('is-collapsed', !willExpand);
                toggleBtn.setAttribute('aria-expanded', String(willExpand));
                toggleBtn.textContent = willExpand ? '收起 ↑' : '展开 ↓';
                if (!willExpand) wrapper.scrollIntoView({block: 'start', behavior: 'smooth'});
            };
        }

        if (reset || !toggleBtn.hasAttribute('aria-expanded')) {
            contentDiv.classList.remove('is-collapsed');
            toggleBtn.setAttribute('aria-expanded', 'true');
            toggleBtn.textContent = '收起 ↑';
        }
    });
}

function renderToolCall(item, outputText = '', reasoning = '', traceText = '') {
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
    const isExecTool = /(?:^|[._])exec$/i.test(String(item?.name || ''));
    const statusText = status === 'completed'
        ? (isExecTool ? '' : '已完成')
        : status === 'in_progress' ? '运行中' : status;
    const statusElement = summary.querySelector('.tool-call-status');
    statusElement.textContent = statusText;
    if (!statusText) statusElement.style.display = 'none';
    if (traceText) {
        const trace = document.createElement('div');
        trace.className = 'tool-call-trace';
        trace.textContent = traceText;
        summary.insertBefore(trace, statusElement);
    }

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
        thinkHeader.onclick = () => toggleThinking(thinkHeader);
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
        <div class="think" style="display:none;" onclick="toggleThinking(this)">▶ 正在思考...</div>
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
                if (wrapper.dataset.nativeFormat === 'claude' && typeof removeClaudeHistoryItem === 'function')
                    removeClaudeHistoryItem(wrapper);
                else if (wrapper.dataset.nativeFormat === 'gemini' && typeof removeGeminiHistoryItem === 'function')
                    removeGeminiHistoryItem(wrapper);
                else if (typeof removeResponseHistoryItem === 'function') removeResponseHistoryItem(wrapper);
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
                    const collapseBtn = wrapper.querySelector('.assistant-collapse-btn');
                    if (collapseBtn) collapseBtn.style.display = 'none';
                    ta.focus();
                    editBtn.innerHTML = '💾 保存';
                    isEditing = true;
                } else {
                    const newText = ta.value;
                    displayDiv.textContent = newText;
                    wrapper.dataset.raw = newText;
                    if (wrapper.dataset.nativeFormat === 'claude' && typeof updateClaudeHistoryItem === 'function')
                        updateClaudeHistoryItem(wrapper, newText);
                    else if (wrapper.dataset.nativeFormat === 'gemini' && typeof updateGeminiHistoryItem === 'function')
                        updateGeminiHistoryItem(wrapper, newText);
                    else if (typeof updateResponseHistoryItem === 'function') updateResponseHistoryItem(wrapper, newText);
                    displayDiv.style.display = 'block';
                    ta.style.display = 'none';
                    if (chatUser) chatUser.style.width = '';
                    editBtn.innerHTML = '✏️ 修改';
                    isEditing = false;
                    const collapseBtn = wrapper.querySelector('.assistant-collapse-btn');
                    if (collapseBtn) collapseBtn.style.display = '';
                    updateAssistantCollapse(wrapper, true);
                }
            } else {
                    const contentDiv = wrapper.querySelector('.chat-content-markdown, .user-text-display');
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
                    const collapseBtn = wrapper.querySelector('.assistant-collapse-btn');
                    if (collapseBtn) collapseBtn.style.display = 'none';
                    
                    editTa.style.height = 'auto';
                    editTa.style.height = editTa.scrollHeight + 10 + 'px';

                    editBtn.innerHTML = '💾 保存';
                    isEditing = true;
                } else {
                    wrapper.dataset.raw = editTa.value;
                    if (wrapper.dataset.nativeFormat === 'claude' && typeof updateClaudeHistoryItem === 'function')
                        updateClaudeHistoryItem(wrapper, editTa.value);
                    else if (wrapper.dataset.nativeFormat === 'gemini' && typeof updateGeminiHistoryItem === 'function')
                        updateGeminiHistoryItem(wrapper, editTa.value);
                    else if (typeof updateResponseHistoryItem === 'function') updateResponseHistoryItem(wrapper, editTa.value);
                    contentDiv.innerHTML = safeParseMarkdown(editTa.value);
                    editTa.style.display = 'none';
                    contentDiv.style.display = 'block';
                    renderMathAndCode(contentDiv);
                    editBtn.innerHTML = '✏️ 修改';
                    isEditing = false;
                    const collapseBtn = wrapper.querySelector('.assistant-collapse-btn');
                    if (collapseBtn) collapseBtn.style.display = '';
                    updateAssistantCollapse(wrapper, true);
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
