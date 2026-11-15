#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""采集样本 (responses / completions / claude 三种格式, 流式与非流式都支持), 只用标准库。

用法:
    python3 collect.py <baseurl> <apikey> <modelname> <amount> <p> <outputfile> <reqfile> [--timeout 秒]

    baseurl    接口地址, 例如 https://api.example.com/v1; 实际请求地址按格式自动拼 (见下)
    apikey     密钥
    modelname  模型名, 会替换请求体里的 model, 也是写入 outputfile 时的键
    amount     请求次数
    p          并发上限
    outputfile 输出文件, 格式同 zero.json: {"模型名": ["输出1", "输出2", ...]}; 已存在则合并追加
    reqfile    请求体 json, 只改 model, 其余原样发送; 里面的 stream 决定流式还是非流式

格式判断 (参考 code/lib/gptapi6.cpp):
    baseurl 已经以 /responses、/chat/completions、/completions、/messages 结尾 -> 直接用它, 格式对应
    请求体有 messages, 且有 system/anthropic_version/thinking/top_k/stop_sequences,
        或 tools 里用的是 input_schema                     -> claude      <baseurl>/v1/messages
    请求体有 messages                                      -> completions <baseurl>/v1/chat/completions
    请求体有 input                                         -> responses   <baseurl>/v1/responses
    baseurl 已经带版本段 (/v1、/v1beta 等)时不再重复加 /v1。
    claude 的请求体如果既没 system 也没其他特征, 请直接把 baseurl 给到 .../v1/messages 以免误判。

鉴权头: claude 用 x-api-key + anthropic-version: 2023-06-01, 其余用 Authorization: Bearer。
响应解析看实际内容: 以 { 开头按普通 json, 否则按 SSE 逐行解析; 流式必须收到结束事件才算成功。

每个请求只发一次, 任何失败都不重试; 失败或回复里没有字母的不写入 outputfile。每成功一条就落盘一次。
"""
import sys, os, re, json, argparse, threading, urllib.request, urllib.error
from concurrent.futures import ThreadPoolExecutor

lock = threading.Lock()
SHORT = {'responses': '/responses', 'completions': '/chat/completions', 'claude': '/messages'}
FULL = {'responses': ('/responses',), 'completions': ('/chat/completions', '/completions'), 'claude': ('/messages',)}


def endpoint(base, fmt):
    """拼出真正要 POST 的地址"""
    u = base.rstrip('/')
    if any(u.endswith(s) for s in FULL[fmt]): return u                 # 已经是完整接口地址
    if re.search(r'/v\d+[a-z0-9]*$', u, re.I): return u + SHORT[fmt]   # 自带版本段, 如 .../v1
    return u + '/v1' + SHORT[fmt]


def detect(base, body):
    """按 baseurl 结尾和请求体字段判断格式, 返回 (格式, 地址)"""
    u = base.rstrip('/').lower()
    for fmt in ('completions', 'claude', 'responses'):
        if any(u.endswith(s) for s in FULL[fmt]): return fmt, base.rstrip('/')
    if isinstance(body.get('messages'), list):
        claude = ('system', 'anthropic_version', 'thinking', 'top_k', 'stop_sequences')
        tools = body.get('tools') or []
        if any(k in body for k in claude) or any(isinstance(t, dict) and 'input_schema' in t for t in tools):
            return 'claude', endpoint(base, 'claude')
        return 'completions', endpoint(base, 'completions')
    if 'input' in body: return 'responses', endpoint(base, 'responses')
    raise SystemExit('认不出请求体格式: 需要 messages(completions/claude) 或 input(responses)')


def join_text(v):
    """把 content 拼成纯文本: 字符串, 或 [{"type":"text","text":...}, ...]"""
    if isinstance(v, str): return v
    if not isinstance(v, list): return ''
    return ''.join(x if isinstance(x, str) else x.get('text') or ''
                   for x in v if isinstance(x, str) or (isinstance(x, dict) and isinstance(x.get('text'), str)))


def err_text(e):
    if isinstance(e, dict): return e.get('message') or json.dumps(e, ensure_ascii=False)
    return str(e)


def responses_text(d):
    """responses 的 output 里 type=message 的文本"""
    return ''.join(join_text(item.get('content')) for item in d.get('output') or []
                   if isinstance(item, dict) and item.get('type') == 'message')


def completions_text(d):
    ch = (d.get('choices') or [None])[0]
    m = ch.get('message') if isinstance(ch, dict) else None
    return join_text(m.get('content')) if isinstance(m, dict) else ''


def claude_text(d):
    return join_text([b for b in d.get('content') or [] if isinstance(b, dict) and b.get('type') == 'text'])


def parse_json(raw, fmt):
    """非流式: 整个响应就是一个 json"""
    try: d = json.loads(raw)
    except ValueError: return None, '响应不是合法 JSON'
    if not isinstance(d, dict): return None, '响应不是 JSON 对象'
    if d.get('error'): return None, '接口报错 ' + err_text(d['error'])[:300]
    if fmt == 'claude':
        if d.get('type') == 'error': return None, '接口报错 ' + err_text(d.get('error'))[:300]
        return claude_text(d), None
    if fmt == 'completions': return completions_text(d), None
    if d.get('status') in ('failed', 'incomplete'):
        return None, '响应 %s %s' % (d.get('status'), err_text(d.get('error') or d.get('incomplete_details'))[:300])
    return responses_text(d), None


def parse_sse(raw, fmt):
    """流式: 逐行解析 SSE, 返回 (文本, 错误信息)"""
    text, alt, err, done = [], [], None, False
    for line in raw.splitlines():
        line = line.rstrip('\r')
        if not line.startswith('data:'): continue
        data = line[5:].strip()
        if not data: continue
        if data == '[DONE]':
            if fmt == 'completions': done = True
            continue
        try: ev = json.loads(data)
        except ValueError: continue
        if not isinstance(ev, dict): continue
        if fmt == 'responses':
            t = ev.get('type')
            if t == 'response.output_text.delta': text.append(ev.get('delta') or '')
            elif t == 'response.output_text.done' and not text: text.append(ev.get('text') or '')
            elif t == 'response.completed':
                done = True; alt = [responses_text(ev.get('response') or {})]
            elif t in ('response.failed', 'response.incomplete', 'error'):
                r = ev if t == 'error' else ev.get('response') or {}
                err = '%s %s' % (t, err_text(r.get('error') or r.get('incomplete_details') or
                                              ev.get('message'))[:300])
        elif fmt == 'completions':
            if ev.get('error'): err = '接口报错 ' + err_text(ev['error'])[:300]; continue
            for ch in ev.get('choices') or []:
                if not isinstance(ch, dict) or ch.get('index') not in (None, 0): continue
                dl = ch.get('delta')
                if isinstance(dl, dict): text.append(join_text(dl.get('content')))
                m = ch.get('message')                       # 少数渠道流式里直接给完整 message
                if isinstance(m, dict): alt.append(join_text(m.get('content')))
                if ch.get('finish_reason'): done = True
        else:
            t = ev.get('type')
            if t == 'content_block_start':
                b = ev.get('content_block') or {}
                if b.get('type') == 'text': text.append(b.get('text') or '')
            elif t == 'content_block_delta':
                dl = ev.get('delta') or {}
                if isinstance(dl.get('text'), str): text.append(dl['text'])
            elif t == 'message_stop': done = True
            elif t == 'error': err = '接口报错 ' + err_text(ev.get('error') or ev)[:300]
    if err: return None, err
    if not done:
        return None, '流提前结束, 没有收到 %s' % {
            'responses': 'response.completed', 'completions': 'finish_reason 或 [DONE]',
            'claude': 'message_stop'}[fmt]
    return ''.join(text) or ''.join(alt), None


def request_once(url, key, fmt, body, timeout, stream):
    """发一次请求, 返回 (文本, 错误信息)"""
    h = {'Content-Type': 'application/json', 'User-Agent': 'modeltrace-collect/2.0',
         'Accept': 'text/event-stream' if stream else 'application/json'}
    if fmt == 'claude':
        h['x-api-key'] = key; h['anthropic-version'] = '2023-06-01'
    else:
        h['Authorization'] = 'Bearer ' + key
    req = urllib.request.Request(url, data=body, method='POST', headers=h)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            raw = r.read().decode('utf-8', 'replace')
    except urllib.error.HTTPError as e:
        try: detail = e.read()[:400].decode('utf-8', 'replace')
        except Exception: detail = ''
        return None, 'HTTP %d %s' % (e.code, detail)
    except Exception as e:
        return None, '%s: %s' % (type(e).__name__, e)
    if not raw.strip(): return None, '响应为空'
    if raw.lstrip().startswith('{'): return parse_json(raw, fmt)
    return parse_sse(raw, fmt)


def append(path, model, text):
    with lock:
        D = {}
        if os.path.isfile(path):
            with open(path, encoding='utf-8') as f: D = json.load(f)
        D.setdefault(model, []).append(text)
        tmp = path + '.tmp'
        with open(tmp, 'w', encoding='utf-8') as f: json.dump(D, f, ensure_ascii=False, indent=1)
        os.replace(tmp, path)
        return len(D[model])


def main():
    ap = argparse.ArgumentParser(description='采集样本 (responses/completions/claude, 流式与非流式), 不重试')
    ap.add_argument('baseurl'); ap.add_argument('apikey'); ap.add_argument('modelname')
    ap.add_argument('amount', type=int); ap.add_argument('p', type=int)
    ap.add_argument('outputfile'); ap.add_argument('reqfile')
    ap.add_argument('--timeout', type=float, default=600, help='单个请求的超时秒数, 默认 600')
    a = ap.parse_args()
    if a.amount < 1 or a.p < 1: sys.exit('amount 和 p 必须是正整数')
    with open(a.reqfile, encoding='utf-8') as f: R = json.load(f)
    if not isinstance(R, dict): sys.exit('%s 里必须是 JSON 对象' % a.reqfile)
    R['model'] = a.modelname
    fmt, url = detect(a.baseurl, R)
    stream = bool(R.get('stream'))
    body = json.dumps(R, ensure_ascii=False).encode('utf-8')
    path = os.path.abspath(a.outputfile)
    if os.path.isfile(path):
        with open(path, encoding='utf-8') as f:
            if not isinstance(json.load(f), dict): sys.exit('%s 不是 {"模型名": [...]} 格式' % path)
    done = [0, 0]

    def work(i):
        text, err = request_once(url, a.apikey, fmt, body, a.timeout, stream)
        if err is None and not any(c.isascii() and c.isalpha() for c in text): err = '回复里没有字母'
        if err is None: n = append(path, a.modelname, text)
        with lock:
            done[0 if err is None else 1] += 1
            if err is None: print('[%d/%d] 成功, %d 个字母, %s 现有 %d 条' % (i + 1, a.amount, sum(c.isascii() and c.isalpha() for c in text), a.modelname, n), flush=True)
            else: print('[%d/%d] 失败: %s' % (i + 1, a.amount, err), flush=True)

    print('格式 %s, %s, POST %s  model=%s  共 %d 次, 并发 %d' % (fmt, '流式' if stream else '非流式', url, a.modelname, a.amount, a.p), flush=True)
    with ThreadPoolExecutor(max_workers=a.p) as ex: list(ex.map(work, range(a.amount)))
    print('完成: 成功 %d, 失败 %d, 写入 %s' % (done[0], done[1], path), flush=True)


if __name__ == '__main__':
    main()
