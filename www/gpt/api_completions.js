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
            updateAssistantCollapse(wrapper, true);
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
    updateAssistantCollapse(wrapper, true);
    scrollToBottom();
    return responseId;
}
