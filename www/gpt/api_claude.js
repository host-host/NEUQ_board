let currentClaudeMessages = [];

function claudeIsToolUse(part) {
    return part?.type === 'tool_use' || part?.type === 'server_tool_use';
}

function claudeIsToolResult(part) {
    return part?.type === 'tool_result' || part?.type === 'web_search_tool_result';
}

function claudeToolInputText(part) {
    const input = part?.input;
    if (input == null || input === '') return '';
    return typeof input === 'string' ? input : JSON.stringify(input, null, 2);
}

function claudeToolResultText(part) {
    if (!part) return '';
    const content = part.content;
    if (typeof content === 'string') return content;
    if (Array.isArray(content)) {
        const text = content.map(item => typeof item === 'string' ? item
            : item?.type === 'text' ? item.text || ''
            : item?.type === 'image' ? '[图片]' : '').filter(Boolean).join('\n');
        if (text) return text;
    }
    return content == null ? '' : JSON.stringify(content, null, 2);
}

// tool_result 在紧随其后的 user 消息里，按 tool_use_id 与 tool_use 配对。
function claudeToolOutputs(messages) {
    const outputs = new Map();
    messages.forEach(message => {
        if (message?.role !== 'user' || !Array.isArray(message.content)) return;
        message.content.forEach(part => {
            if (claudeIsToolResult(part) && part.tool_use_id) outputs.set(part.tool_use_id, part);
        });
    });
    return outputs;
}

function claudeAttachment(raw) {
    const match = raw.match(/^<!--IMAGE_ATTACHMENT:(.*?)-->(data:([^;,]+);base64,(.*))$/s);
    if (match) return {
        display: match[2],
        block: {type: 'image', source: {type: 'base64', media_type: match[3], data: match[4]}}
    };

    // renderUserMessage 会把图片附件转换成 OpenAI image_url 数组供通用 UI 保存。
    let payload;
    try { payload = JSON.parse(raw); } catch (error) { return null; }
    const url = Array.isArray(payload)
        ? payload.find(part => part?.type === 'image_url')?.image_url?.url : null;
    const dataUrl = typeof url === 'string'
        ? url.match(/^data:([^;,]+);base64,(.*)$/s) : null;
    if (!dataUrl) return null;
    return {
        display: url,
        block: {type: 'image', source: {type: 'base64', media_type: dataUrl[1], data: dataUrl[2]}}
    };
}

function claudePartsFromWrapper(wrapper) {
    const raw = wrapper.dataset.raw || '';
    const image = claudeAttachment(raw);
    return image ? [image.block] : [{type: 'text', text: raw}];
}

function bindClaudeItem(wrapper, messageIndex, partIndex = null) {
    wrapper.dataset.nativeFormat = 'claude';
    wrapper.dataset.nativeIndex = String(messageIndex);
    if (partIndex === null) delete wrapper.dataset.nativePartIndex;
    else wrapper.dataset.nativePartIndex = String(partIndex);
}

function appendNewClaudeMessages(wrappers) {
    const content = [];
    const index = currentClaudeMessages.length;
    wrappers.forEach(wrapper => {
        const parts = claudePartsFromWrapper(wrapper);
        bindClaudeItem(wrapper, index, content.length);
        content.push(...parts);
    });
    currentClaudeMessages.push({role: 'user', content});
}

function renderClaudeHistory(data) {
    const chatBox = document.getElementById('chatBox');
    chatBox.innerHTML = '';
    currentClaudeMessages = Array.isArray(data.content)
        ? JSON.parse(JSON.stringify(data.content)) : [];
    const toolOutputs = claudeToolOutputs(currentClaudeMessages);
    currentClaudeMessages.forEach((message, messageIndex) => {
        const content = message?.content;
        if (message?.role === 'user') {
            if (typeof content === 'string') {
                bindClaudeItem(renderUserMessage(content), messageIndex);
                return;
            }
            (Array.isArray(content) ? content : []).forEach((part, partIndex) => {
                let wrapper = null;
                if (part?.type === 'text') wrapper = renderUserMessage(part.text || '');
                else if (part?.type === 'image' && part.source?.type === 'base64') {
                    const url = `data:${part.source.media_type || 'image/jpeg'};base64,${part.source.data || ''}`;
                    wrapper = renderUserMessage(`<!--IMAGE_ATTACHMENT:{"id":"","name":"历史图片"}-->${url}`);
                } else if (claudeIsToolResult(part) && !part.tool_use_id) {
                    // 没有 tool_use_id 无法配对，单独渲染，避免内容凭空消失。
                    wrapper = renderToolCall({name: '工具输出'}, claudeToolResultText(part));
                }
                if (wrapper) bindClaudeItem(wrapper, messageIndex, partIndex);
            });
        } else if (message?.role === 'assistant') {
            if (typeof content === 'string') {
                bindClaudeItem(renderAssistantMessage(content), messageIndex);
                return;
            }
            // thinking 挂到它后面第一个 text / tool_use 上，保持块的原始顺序。
            let pendingThinking = '';
            let rendered = 0;
            (Array.isArray(content) ? content : []).forEach((part, partIndex) => {
                let wrapper = null;
                if (part?.type === 'thinking' || part?.type === 'redacted_thinking') {
                    const text = part.thinking || part.data || '';
                    if (text) pendingThinking += `${pendingThinking ? '\n' : ''}${text}`;
                    return;
                }
                if (part?.type === 'text') wrapper = renderAssistantMessage(part.text || '', pendingThinking);
                else if (claudeIsToolUse(part)) {
                    const output = part.id ? toolOutputs.get(part.id) : null;
                    wrapper = renderToolCall(
                        {name: part.name, input: claudeToolInputText(part),
                         status: output?.is_error ? '出错' : output ? 'completed' : ''},
                        claudeToolResultText(output),
                        pendingThinking
                    );
                }
                if (!wrapper) return;
                pendingThinking = '';
                rendered++;
                bindClaudeItem(wrapper, messageIndex, partIndex);
            });
            // 整条消息只有 thinking，仍要把思考过程显示出来。
            if (pendingThinking || !rendered) bindClaudeItem(renderAssistantMessage('', pendingThinking), messageIndex);
        }
    });
}

function updateClaudeHistoryItem(wrapper, text) {
    const index = Number(wrapper.dataset.nativeIndex);
    const message = currentClaudeMessages[index];
    if (!Number.isInteger(index) || !message) return;
    const partIndex = Number(wrapper.dataset.nativePartIndex);
    if (Array.isArray(message.content)) {
        if (Number.isInteger(partIndex) && message.content[partIndex]?.type === 'text') {
            message.content[partIndex].text = text;
            return;
        }
        const part = message.content.find(item => item?.type === 'text');
        if (part) part.text = text;
        else message.content.push({type: 'text', text});
    } else message.content = text;
}

function removeClaudeHistoryItem(wrapper) {
    const index = Number(wrapper.dataset.nativeIndex);
    const message = currentClaudeMessages[index];
    if (!Number.isInteger(index) || !message) return;
    const partIndex = Number(wrapper.dataset.nativePartIndex);
    if (Number.isInteger(partIndex) && Array.isArray(message.content) && message.content.length > 1) {
        message.content.splice(partIndex, 1);
        document.querySelectorAll('[data-native-format="claude"][data-native-index]').forEach(item => {
            const itemIndex = Number(item.dataset.nativeIndex);
            const itemPart = Number(item.dataset.nativePartIndex);
            if (itemIndex === index && itemPart > partIndex) item.dataset.nativePartIndex = String(itemPart - 1);
        });
        return;
    }
    currentClaudeMessages.splice(index, 1);
    document.querySelectorAll('[data-native-format="claude"][data-native-index]').forEach(item => {
        const oldIndex = Number(item.dataset.nativeIndex);
        if (oldIndex > index) item.dataset.nativeIndex = String(oldIndex - 1);
    });
}

async function refreshClaudeState(id) {
    const data = await fetchGpt5History(id, false);
    if (data.format !== 'claude') throw new Error('会话格式与 Claude Messages 不一致');
    renderClaudeHistory(data);
    renderMathAndCode(document.getElementById('chatBox'));
    return data;
}

async function callClaudeStreamingApi(response, wrapper, contentDiv, thinkTextarea, startTime) {
    let responseId = '';
    let rawContent = '';
    let streamError = null;
    let buffer = '';
    let outputTokens = null;
    let messageRole = 'assistant';
    const nativeBlocks = [];
    const partialToolInput = [];
    try {
        const reader = response.body.getReader();
        const decoder = new TextDecoder('utf-8');
        while (true) {
            const {done, value} = await reader.read();
            if (done) break;
            buffer += decoder.decode(value, {stream: true});
            const lines = buffer.split('\n');
            buffer = lines.pop();
            for (const line of lines) {
                if (!line.trim().startsWith('data:')) continue;
                const payload = line.trim().slice(5).trim();
                if (!payload) continue;
                let event;
                try { event = JSON.parse(payload); } catch (error) { continue; }
                if (event.type === 'message_start') {
                    responseId = event.message?.id || responseId;
                    messageRole = event.message?.role || messageRole;
                } else if (event.type === 'content_block_start') {
                    nativeBlocks[event.index || 0] = JSON.parse(JSON.stringify(event.content_block || {}));
                } else if (event.type === 'content_block_delta') {
                    const index = event.index || 0;
                    const block = nativeBlocks[index] || (nativeBlocks[index] = {});
                    if (event.delta?.type === 'text_delta') {
                        block.type ||= 'text';
                        block.text = (block.text || '') + (event.delta.text || '');
                        rawContent += event.delta.text || '';
                        wrapper.dataset.raw = rawContent;
                        contentDiv.innerHTML = safeParseMarkdown(rawContent);
                        contentDiv.style.display = 'block';
                    } else if (event.delta?.type === 'thinking_delta') {
                        block.type ||= 'thinking';
                        block.thinking = (block.thinking || '') + (event.delta.thinking || '');
                        thinkTextarea.previousElementSibling.style.display = 'flex';
                        thinkTextarea.style.display = 'block';
                        thinkTextarea.value += event.delta.thinking || '';
                    } else if (event.delta?.type === 'signature_delta') {
                        block.signature = (block.signature || '') + (event.delta.signature || '');
                    } else if (event.delta?.type === 'input_json_delta') {
                        partialToolInput[index] = (partialToolInput[index] || '') + (event.delta.partial_json || '');
                    }
                } else if (event.type === 'message_delta') {
                    outputTokens = event.usage?.output_tokens ?? outputTokens;
                } else if (event.type === 'error') {
                    streamError = new Error(event.error?.message || 'Claude API 请求失败');
                }
            }
        }
    } catch (error) { streamError = error; }
    if (!streamError) {
        partialToolInput.forEach((input, index) => {
            if (!input || !nativeBlocks[index]) return;
            try { nativeBlocks[index].input = JSON.parse(input); } catch (error) { /* 保留 start 中的 input */ }
        });
        const content = nativeBlocks.filter(Boolean);
        if (content.length > 0) currentClaudeMessages.push({role: messageRole, content});
    }
    if (streamError) {
        const text = rawContent ? `${rawContent}\n\n${streamError.message}` : streamError.message;
        wrapper.dataset.raw = text;
        contentDiv.textContent = text;
        contentDiv.style.display = 'block';
    } else if (!rawContent) {
        // 只调用工具、没有文本的一轮，别报成「没有返回内容」。
        const names = nativeBlocks.filter(claudeIsToolUse).map(block => block.name).filter(Boolean);
        wrapper.dataset.raw = names.length ? `调用工具：${names.join('、')}` : 'Claude 没有返回文本内容';
        contentDiv.textContent = wrapper.dataset.raw;
        contentDiv.style.display = 'block';
    }
    if (Number.isFinite(outputTokens)) {
        const elapsed = Math.max((Date.now() - startTime) / 1000, 0.01);
        const stats = document.createElement('span');
        stats.className = 'token-stats-item';
        stats.textContent = `${outputTokens} tokens | ${(outputTokens / elapsed).toFixed(2)} tokens/s`;
        wrapper.querySelector('.contentcalc')?.prepend(stats);
    }
    renderMathAndCode(contentDiv);
    updateAssistantCollapse(wrapper, true);
    scrollToBottom();
    return streamError ? '' : responseId;
}
