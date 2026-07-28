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

function decodeExecStringLiteral(literal) {
    if (!literal) return '';
    if (literal[0] === '"') {
        try { return JSON.parse(literal); }
        catch (error) { /* 使用下面的宽松解码 */ }
    }
    return literal.slice(1, -1)
        .replace(/\\n/g, '\n')
        .replace(/\\r/g, '\r')
        .replace(/\\t/g, '\t')
        .replace(/\\([\\'"`])/g, '$1');
}

function extractExecCommands(item) {
    const input = item?.input ?? item?.arguments ?? '';
    if (input && typeof input === 'object') {
        if (typeof input.cmd === 'string') return [input.cmd];
        return [];
    }
    if (typeof input !== 'string' || !input.trim()) return [];
    try {
        const parsed = JSON.parse(input);
        if (typeof parsed?.cmd === 'string') return [parsed.cmd];
    } catch (error) {
        // functions.exec 的 input 通常是 JavaScript，不是 JSON。
    }
    const commands = [];
    const pattern = /\btools\.exec_command\s*\(\s*\{[\s\S]*?\bcmd\s*:\s*("(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|`(?:\\.|[^`\\])*`)/g;
    for (let match = pattern.exec(input); match; match = pattern.exec(input)) {
        commands.push(decodeExecStringLiteral(match[1]));
    }
    return commands;
}

function shortenToolCommand(command, maxLength = 120) {
    const line = command.trim().split('\n')[0].replace(/\s+/g, ' ');
    return line.length > maxLength ? `${line.slice(0, maxLength - 1)}…` : line;
}

function summarizeExecCommand(command) {
    const preview = shortenToolCommand(command);
    if (!preview) return null;
    if (/^git\s+diff\b/.test(preview)) return {title: 'Ran', detail: preview};

    const readsFile = /(?:^|[|;&]\s*)?(?:nl|sed|head|tail|cat)\b/.test(preview);
    if (readsFile) {
        const filePattern = /(?:[A-Za-z0-9_.-]+\/)*[A-Za-z0-9_.-]+\.(?:cpp|cc|c|h|hpp|js|ts|jsx|tsx|json|md|css|html|txt|py|rs|go|java|sh|ya?ml)\b/g;
        const files = [...new Set((preview.match(filePattern) || []).map(path => path.split('/').pop()))];
        if (files.length > 0) return {title: 'Explored', detail: `Read ${files.join(', ')}`};
    }
    if (/^(?:rg|find|ls|pwd|git\s+(?:status|log|show))\b/.test(preview)) {
        return {title: 'Explored', detail: preview};
    }
    return {title: 'Ran', detail: preview};
}

function responsesToolTrace(item) {
    const name = String(item?.name || '');
    if (!/(?:^|[._])exec$/i.test(name)) return '';
    const seen = new Set();
    const traces = [];
    extractExecCommands(item).forEach(command => {
        const trace = summarizeExecCommand(command);
        if (!trace) return;
        const key = `${trace.title}\n${trace.detail}`;
        if (seen.has(key)) return;
        seen.add(key);
        traces.push(trace);
    });
    return traces.map(trace => `${trace.title}: ${trace.detail}`).join('  •  ');
}

function responseRenderableEntries(input) {
    const entries = [];
    input.forEach((item, index) => {
        if (item?.role === 'user' || item?.role === 'assistant' || isResponsesToolCall(item)) {
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
            wrapper = renderToolCall(
                item,
                responsesToolOutputText(output?.item),
                pendingReasoning,
                responsesToolTrace(item)
            );
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
    updateAssistantCollapse(wrapper, true);
    scrollToBottom();
    return streamError ? '' : responseId;
}
