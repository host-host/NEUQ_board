let currentGeminiContents = [];

function geminiText(parts) {
    if (!Array.isArray(parts)) return '';
    return parts.filter(part => typeof part?.text === 'string' && !part.thought)
        .map(part => part.text).join('\n');
}

function geminiThinking(parts) {
    if (!Array.isArray(parts)) return '';
    return parts.filter(part => typeof part?.text === 'string' && part.thought)
        .map(part => part.text).join('\n');
}

function geminiAttachment(raw) {
    const match = raw.match(/^<!--IMAGE_ATTACHMENT:(.*?)-->(data:([^;,]+);base64,(.*))$/s);
    if (match) return {
        display: match[2],
        part: {inlineData: {mimeType: match[3], data: match[4]}}
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
        part: {inlineData: {mimeType: dataUrl[1], data: dataUrl[2]}}
    };
}

function geminiPartsFromWrapper(wrapper) {
    const raw = wrapper.dataset.raw || '';
    const image = geminiAttachment(raw);
    return image ? [image.part] : [{text: raw}];
}

function bindGeminiItem(wrapper, contentIndex, partIndex = null) {
    wrapper.dataset.nativeFormat = 'gemini';
    wrapper.dataset.nativeIndex = String(contentIndex);
    if (partIndex === null) delete wrapper.dataset.nativePartIndex;
    else wrapper.dataset.nativePartIndex = String(partIndex);
}

function appendNewGeminiContents(wrappers) {
    const parts = [];
    const index = currentGeminiContents.length;
    wrappers.forEach(wrapper => {
        const wrapperParts = geminiPartsFromWrapper(wrapper);
        bindGeminiItem(wrapper, index, parts.length);
        parts.push(...wrapperParts);
    });
    currentGeminiContents.push({role: 'user', parts});
}

function renderGeminiHistory(data) {
    const chatBox = document.getElementById('chatBox');
    chatBox.innerHTML = '';
    currentGeminiContents = Array.isArray(data.content)
        ? JSON.parse(JSON.stringify(data.content)) : [];
    currentGeminiContents.forEach((content, contentIndex) => {
        const parts = Array.isArray(content?.parts) ? content.parts : [];
        if (content?.role === 'user') {
            parts.forEach((part, partIndex) => {
                let wrapper = null;
                if (typeof part?.text === 'string') wrapper = renderUserMessage(part.text);
                else {
                    const image = part?.inlineData || part?.inline_data;
                    if (image?.data) {
                        const url = `data:${image.mimeType || image.mime_type || 'image/jpeg'};base64,${image.data}`;
                        wrapper = renderUserMessage(`<!--IMAGE_ATTACHMENT:{"id":"","name":"历史图片"}-->${url}`);
                    }
                }
                if (wrapper) bindGeminiItem(wrapper, contentIndex, partIndex);
            });
        } else if (content?.role === 'model') {
            const wrapper = renderAssistantMessage(geminiText(parts), geminiThinking(parts));
            bindGeminiItem(wrapper, contentIndex);
        }
    });
}

function updateGeminiHistoryItem(wrapper, text) {
    const index = Number(wrapper.dataset.nativeIndex);
    const content = currentGeminiContents[index];
    if (!Number.isInteger(index) || !content) return;
    const partIndex = Number(wrapper.dataset.nativePartIndex);
    if (!Array.isArray(content.parts)) content.parts = [];
    if (Number.isInteger(partIndex) && typeof content.parts[partIndex]?.text === 'string') {
        content.parts[partIndex].text = text;
        return;
    }
    const part = content.parts.find(item => typeof item?.text === 'string' && !item.thought);
    if (part) part.text = text;
    else content.parts.push({text});
}

function removeGeminiHistoryItem(wrapper) {
    const index = Number(wrapper.dataset.nativeIndex);
    const content = currentGeminiContents[index];
    if (!Number.isInteger(index) || !content) return;
    const partIndex = Number(wrapper.dataset.nativePartIndex);
    if (Number.isInteger(partIndex) && Array.isArray(content.parts) && content.parts.length > 1) {
        content.parts.splice(partIndex, 1);
        document.querySelectorAll('[data-native-format="gemini"][data-native-index]').forEach(item => {
            const itemIndex = Number(item.dataset.nativeIndex);
            const itemPart = Number(item.dataset.nativePartIndex);
            if (itemIndex === index && itemPart > partIndex) item.dataset.nativePartIndex = String(itemPart - 1);
        });
        return;
    }
    currentGeminiContents.splice(index, 1);
    document.querySelectorAll('[data-native-format="gemini"][data-native-index]').forEach(item => {
        const oldIndex = Number(item.dataset.nativeIndex);
        if (oldIndex > index) item.dataset.nativeIndex = String(oldIndex - 1);
    });
}

async function refreshGeminiState(id) {
    const data = await fetchGpt5History(id, false);
    if (data.format !== 'gemini') throw new Error('会话格式与 Gemini generateContent 不一致');
    renderGeminiHistory(data);
    renderMathAndCode(document.getElementById('chatBox'));
    return data;
}

async function callGeminiStreamingApi(response, wrapper, contentDiv, thinkTextarea, startTime) {
    let responseId = '';
    let rawContent = '';
    let streamError = null;
    let buffer = '';
    let outputTokens = null;
    const nativeParts = [];
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
                if (event.error) {
                    streamError = new Error(event.error.message || 'Gemini API 请求失败');
                    continue;
                }
                responseId = event.responseId || responseId;
                const parts = event.candidates?.[0]?.content?.parts || [];
                parts.forEach(part => {
                    const copy = JSON.parse(JSON.stringify(part));
                    const previous = nativeParts[nativeParts.length - 1];
                    if (typeof copy.text === 'string' && typeof previous?.text === 'string' &&
                        Boolean(copy.thought) === Boolean(previous.thought)) previous.text += copy.text;
                    else if (copy.functionCall && previous?.functionCall) nativeParts[nativeParts.length - 1] = copy;
                    else if (copy.thoughtSignature && previous?.thoughtSignature) nativeParts[nativeParts.length - 1] = copy;
                    else nativeParts.push(copy);
                    if (typeof part.text !== 'string') return;
                    if (part.thought) {
                        thinkTextarea.previousElementSibling.style.display = 'flex';
                        thinkTextarea.style.display = 'block';
                        thinkTextarea.value += part.text;
                    } else rawContent += part.text;
                });
                if (rawContent) {
                    wrapper.dataset.raw = rawContent;
                    contentDiv.innerHTML = safeParseMarkdown(rawContent);
                    contentDiv.style.display = 'block';
                }
                outputTokens = event.usageMetadata?.candidatesTokenCount ?? outputTokens;
            }
        }
    } catch (error) { streamError = error; }
    if (!streamError && nativeParts.length > 0)
        currentGeminiContents.push({role: 'model', parts: nativeParts});
    if (streamError) {
        const text = rawContent ? `${rawContent}\n\n${streamError.message}` : streamError.message;
        wrapper.dataset.raw = text;
        contentDiv.textContent = text;
        contentDiv.style.display = 'block';
    } else if (!rawContent) {
        wrapper.dataset.raw = 'Gemini 没有返回文本内容';
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
    scrollToBottom();
    return streamError ? '' : responseId;
}
