/* Shared by the page and the request/stream regression checks. */
(() => {
    'use strict';

    const SYSTEM = 'You are a helpful assistant.';
    const number = value => typeof value === 'number' && Number.isFinite(value) && value >= 0 ? value : null;
    const formatFor = value => ['responses', 'claude', 'gemini'].includes(value) ? value : 'completions';
    const messageOf = (data, fallback) => data?.error?.message || data?.response?.error?.message ||
        (typeof data?.error === 'string' ? data.error : '') || data?.message || fallback;

    function providersForModel(catalog, model, account) {
        const ids = catalog?.model?.[model]?.provider;
        return [...new Set(Array.isArray(ids) ? ids : [])]
            .filter(id => typeof id === 'string' && id !== 'auto')
            .map(id => {
                const config = catalog.provider?.[id];
                return {id, multiply: number(config?.multiply),
                    available: Boolean(config && account && (account.admin === true || config.public === true))};
            });
    }

    function buildRequest(model, suggestedFormat, question) {
        const format = formatFor(suggestedFormat);
        if (format === 'responses') return {format, endpoint: '/api/v1/responses', body: {
            model, stream: true, instructions: SYSTEM,
            input: [{role: 'user', content: [{type: 'input_text', text: question}]}]
        }};
        if (format === 'claude') return {format, endpoint: '/api/v1/messages', body: {
            model, stream: true, max_tokens: 8192, system: SYSTEM,
            messages: [{role: 'user', content: [{type: 'text', text: question}]}]
        }};
        if (format === 'gemini') return {format,
            endpoint: `/api/v1beta/models/${encodeURIComponent(model)}:streamGenerateContent?alt=sse`, body: {
                systemInstruction: {parts: [{text: SYSTEM}]},
                contents: [{role: 'user', parts: [{text: question}]}], generationConfig: {}
            }};
        return {format, endpoint: '/api/v1/chat/completions', body: {
            model, stream: true, messages: [{role: 'system', content: SYSTEM}, {role: 'user', content: question}],
            stream_options: {include_usage: true}, enable_thinking: true
        }};
    }

    async function requestJson(path, body, fetcher = fetch) {
        const controller = new AbortController();
        const timeout = setTimeout(() => controller.abort(), 20000);
        try {
            const response = await fetcher(path, {method: 'POST', credentials: 'same-origin', cache: 'no-store',
                headers: {'Content-Type': 'application/json'}, body: JSON.stringify(body), signal: controller.signal});
            const text = await response.text();
            let data;
            try { data = JSON.parse(text); } catch (_) { data = null; }
            const message = messageOf(data, typeof data === 'string' ? data : text.slice(0, 300));
            if (response.status === 401 || /log\s*in|not.logged.in/i.test(message)) {
                throw Object.assign(new Error('登录已失效，请重新登录。'), {auth: true});
            }
            if (!response.ok || data?.error) throw new Error(message || `请求失败（HTTP ${response.status}）`);
            if (!data || typeof data !== 'object' || Array.isArray(data)) throw new Error('服务器响应格式异常。');
            return data;
        } catch (error) {
            if (error.name === 'AbortError') throw new Error('请求超时，请刷新后重试。');
            throw error;
        } finally { clearTimeout(timeout); }
    }

    // Keep SSE parsing independent of network chunk boundaries (including UTF-8 and CRLF).
    async function readEvents(response, onEvent) {
        const reader = response.body.getReader();
        const decoder = new TextDecoder();
        let buffer = '';
        let data = [];
        let eventName = '';
        const dispatch = () => {
            if (data.length) onEvent(data.join('\n'), eventName);
            data = [];
            eventName = '';
        };
        const line = value => {
            if (value === '') { dispatch(); return; }
            if (value.startsWith(':')) return;
            const colon = value.indexOf(':');
            const name = colon < 0 ? value : value.slice(0, colon);
            const raw = colon < 0 ? '' : value.slice(colon + 1);
            const content = raw.startsWith(' ') ? raw.slice(1) : raw;
            if (name === 'data') data.push(content);
            if (name === 'event') eventName = content;
        };
        const consume = eof => {
            let start = 0;
            for (let i = 0; i < buffer.length; i++) {
                if (buffer[i] !== '\n' && buffer[i] !== '\r') continue;
                if (buffer[i] === '\r' && i === buffer.length - 1 && !eof) break;
                line(buffer.slice(start, i));
                if (buffer[i] === '\r' && buffer[i + 1] === '\n') i++;
                start = i + 1;
            }
            buffer = buffer.slice(start);
            if (eof) {
                if (buffer) line(buffer);
                dispatch();
            }
        };
        try {
            while (true) {
                const {done, value} = await reader.read();
                buffer += done ? decoder.decode() : decoder.decode(value, {stream: true});
                consume(done);
                if (done) break;
            }
        } finally { reader.releaseLock(); }
    }

    function createResult() {
        return {text: '', latency: null, elapsed: null, inputTokens: null, outputTokens: null, totalTokens: null, tps: null};
    }

    async function ask({model, format: suggestedFormat, question, apiKey}, {fetcher = fetch, now = () => performance.now(), onUpdate = () => {}} = {}) {
        const request = buildRequest(model, suggestedFormat, question);
        const result = createResult();
        const started = now();
        let stream = false;
        let completed = false;
        let failure = null;
        let bodyRead = false;
        let usage = {};
        const responseParts = new Map();
        const fail = message => { failure ||= new Error(message); };
        const markFirst = () => { if (stream && result.latency === null) result.latency = (now() - started) / 1000; };
        const append = value => {
            if (typeof value !== 'string' || !value) return;
            markFirst();
            result.text += value;
        };
        const setUsage = data => {
            if (!data || typeof data !== 'object') return;
            for (const [key, value] of Object.entries(data)) if (number(value) !== null) usage[key] = value;
            if (request.format === 'claude') {
                result.inputTokens = number(usage.input_tokens);
                result.outputTokens = number(usage.output_tokens);
                result.totalTokens = result.inputTokens !== null && result.outputTokens !== null
                    ? result.inputTokens + result.outputTokens + (usage.cache_read_input_tokens || 0) + (usage.cache_creation_input_tokens || 0) : null;
            } else if (request.format === 'gemini') {
                result.inputTokens = number(usage.promptTokenCount);
                result.outputTokens = number(usage.candidatesTokenCount);
                if (result.outputTokens !== null) result.outputTokens += usage.thoughtsTokenCount || 0;
                result.totalTokens = number(usage.totalTokenCount);
            } else {
                result.inputTokens = number(usage.input_tokens ?? usage.prompt_tokens);
                result.outputTokens = number(usage.output_tokens ?? usage.completion_tokens);
                result.totalTokens = number(usage.total_tokens);
            }
            if (result.totalTokens === null && result.inputTokens !== null && result.outputTokens !== null) {
                result.totalTokens = result.inputTokens + result.outputTokens;
            }
        };
        const responsePart = (event, text, final = false) => {
            if (typeof text !== 'string') return;
            if (text) markFirst();
            const key = `${event.output_index ?? 0}:${event.content_index ?? 0}`;
            responseParts.set(key, final ? text : (responseParts.get(key) || '') + text);
            result.text = [...responseParts.values()].join('');
        };
        const outputText = output => (Array.isArray(output) ? output : [])
            .filter(item => item?.type === 'message' && (!item.role || item.role === 'assistant'))
            .flatMap(item => Array.isArray(item.content) ? item.content : [])
            .map(part => part.type === 'output_text' ? part.text || '' : part.type === 'refusal' ? part.refusal || '' : '').join('');
        const consume = (event, eventName = '', json = false) => {
            if (!event || typeof event !== 'object') { fail('接口返回了无效的数据。'); return; }
            const type = event.type || eventName;
            if (event.error || type === 'error' || type === 'response.failed') {
                fail(messageOf(event, '渠道返回错误。'));
            }
            if (request.format === 'completions') {
                setUsage(event.usage);
                const choice = event.choices?.find(item => item.index === 0) || event.choices?.[0];
                const delta = choice?.delta || choice?.message;
                if (delta?.reasoning_content || delta?.reasoning || delta?.tool_calls?.length) markFirst();
                if (typeof delta?.content === 'string') append(delta.content);
                else if (Array.isArray(delta?.content)) delta.content.forEach(part => { if (part.type === 'text') append(part.text); });
                append(delta?.refusal);
                if (choice?.finish_reason) {
                    completed = true;
                    if (['length', 'content_filter'].includes(choice.finish_reason)) fail(`回复未完整生成：${choice.finish_reason}`);
                }
                if (json && choice?.message) completed = true;
            } else if (request.format === 'responses') {
                const response = event.response || event;
                setUsage(response.usage);
                if (type === 'response.output_text.delta' || type === 'response.refusal.delta') responsePart(event, event.delta);
                if (type === 'response.output_text.done') responsePart(event, event.text, true);
                if (type === 'response.refusal.done') responsePart(event, event.refusal, true);
                if (type.includes('reasoning') && typeof event.delta === 'string' && event.delta) markFirst();
                if (type === 'response.output_item.done' && !responseParts.size) append(outputText([event.item]));
                if (type === 'response.completed' || type === 'response.incomplete' || json) {
                    const text = outputText(response.output);
                    if (text) { markFirst(); result.text = text; }
                    completed = response.status === 'completed' || type === 'response.completed';
                    if (type === 'response.incomplete' || response.status === 'incomplete') {
                        fail(`回复未完整生成：${response.incomplete_details?.reason || 'incomplete'}`);
                    }
                    if (response.status === 'failed') fail(messageOf(response, '渠道返回错误。'));
                }
            } else if (request.format === 'claude') {
                setUsage(event.message?.usage);
                setUsage(event.usage);
                if (type === 'content_block_start') {
                    if (event.content_block?.type === 'text') append(event.content_block.text);
                    if (event.content_block?.thinking || event.content_block?.type === 'tool_use') markFirst();
                }
                if (type === 'content_block_delta') {
                    if (event.delta?.type === 'text_delta') append(event.delta.text);
                    else if (event.delta?.thinking || event.delta?.partial_json) markFirst();
                }
                const stop = event.delta?.stop_reason || event.stop_reason;
                if (stop === 'max_tokens') fail('回复未完整生成：max_tokens');
                if (type === 'message_stop') completed = true;
                if (json && type === 'message') {
                    (event.content || []).forEach(part => { if (part.type === 'text') append(part.text); });
                    completed = true;
                }
            } else {
                setUsage(event.usageMetadata);
                const candidate = event.candidates?.[0];
                (candidate?.content?.parts || []).forEach(part => {
                    if (part.text || part.functionCall) markFirst();
                    if (!part.thought) append(part.text);
                });
                if (candidate?.finishReason) {
                    completed = true;
                    if (candidate.finishReason !== 'STOP') fail(`回复未完整生成：${candidate.finishReason}`);
                }
                if (event.promptFeedback?.blockReason) fail(`问题被拦截：${event.promptFeedback.blockReason}`);
            }
            onUpdate({...result});
        };
        try {
            const response = await fetcher(request.endpoint, {method: 'POST', credentials: 'same-origin',
                headers: {'Content-Type': 'application/json', Authorization: `Bearer ${apiKey}`}, body: JSON.stringify(request.body)});
            if (!response.ok) {
                const text = await response.text();
                bodyRead = true;
                let data;
                try { data = JSON.parse(text); } catch (_) { data = null; }
                const error = new Error(messageOf(data, text.slice(0, 500) || `请求失败（HTTP ${response.status}）`));
                error.stopBatch = response.status === 401 || response.status === 403 || response.status === 504;
                throw error;
            }
            stream = /text\/event-stream/i.test(response.headers.get('content-type') || '');
            if (stream && response.body) {
                await readEvents(response, (payload, name) => {
                    if (payload.trim() === '[DONE]') { if (request.format === 'completions') completed = true; return; }
                    let event;
                    try { event = JSON.parse(payload); }
                    catch (_) { fail('无法解析流式回复。'); return; }
                    consume(event, name);
                });
            } else {
                const text = await response.text();
                bodyRead = true;
                let data;
                try { data = JSON.parse(text); }
                catch (_) { throw new Error(text.slice(0, 500) || '服务器没有返回有效回复。'); }
                for (const event of Array.isArray(data) ? data : [data]) consume(event, '', true);
            }
            bodyRead = true;
            if (failure) throw failure;
            if (!completed) throw Object.assign(new Error('回复流意外结束，未收到完成标记；已停止后续测试。'), {stopBatch: true});
            if (!result.text) throw new Error('渠道没有返回文本回复。');
        } catch (error) {
            // A broken connection may leave the upstream request running. Do not start another question.
            if (!bodyRead || error instanceof TypeError || error.name === 'AbortError') error.stopBatch = true;
            failure = error;
        }
        result.elapsed = Math.max(0, (now() - started) / 1000);
        result.tps = result.outputTokens !== null && result.elapsed > 0 ? result.outputTokens / result.elapsed : null;
        onUpdate({...result});
        if (failure) {
            failure.result = result;
            throw failure;
        }
        return result;
    }

    async function runSequential({providers, model, format, question, answer, switchProvider, askQuestion = ask,
        shouldStop = () => false, onResult = () => {}}) {
        const results = [];
        let stopped = false;
        for (const provider of providers) {
            if (shouldStop() || stopped) {
                onResult(provider, {status: 'skipped'});
                stopped = true;
                continue;
            }
            let phase = 'switching';
            onResult(provider, {status: phase});
            try {
                const account = await switchProvider(model, provider.id);
                if (account.selected_provider?.[model] !== provider.id) {
                    throw new Error('渠道切换未生效，未发送提问。');
                }
                if (typeof account.api_key !== 'string' || !account.api_key.startsWith('sk-')) {
                    throw new Error('没有取得有效的 API Key，未发送提问。');
                }
                if (shouldStop()) {
                    onResult(provider, {status: 'skipped'});
                    stopped = true;
                    continue;
                }
                phase = 'asking';
                onResult(provider, {status: phase});
                // Await the entire response, including EOF and usage, before switching the next provider.
                const result = await askQuestion({model, format, question, apiKey: account.api_key}, {
                    onUpdate: partial => onResult(provider, {...partial, status: phase})
                });
                const entry = {...result, status: 'done', matched: answer.length > 0 && result.text.includes(answer)};
                results.push({provider: provider.id, ...entry});
                onResult(provider, entry);
            } catch (error) {
                const entry = {...error.result, status: 'error', error: error.message || '请求失败', matched: null};
                results.push({provider: provider.id, ...entry});
                onResult(provider, entry);
                // A failed switch may still be applied by the server; stop rather than race another switch.
                if (phase === 'switching' || error.stopBatch || error.auth) stopped = true;
            }
        }
        return {results, stopped: stopped || shouldStop()};
    }

    const api = {providersForModel, formatFor, buildRequest, requestJson, readEvents, ask, runSequential};
    if (typeof module !== 'undefined' && module.exports) module.exports = api;
    else globalThis.StableChannelTest = api;
})();
