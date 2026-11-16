#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""模型字母指纹 (modeltrace): 根据"随机 400 字母"任务的输出判断最像哪个模型。

输入一律先转小写, 不使用任何大小写信息 (大小写受渠道/上下文影响太大); 只用统计特征。

依赖: Python3 (>=3.6) 和 numpy。生产环境安装 numpy (二选一, 装一次即可):
     pip3 install numpy                 # 推荐; 若提示没有 pip: apt install -y python3-pip
     apt install -y python3-numpy       # Ubuntu/Debian 也可以直接用系统包
   装完用  python3 -c "import numpy; print(numpy.__version__)"  确认。

两个功能:

1) 预处理: 读入样本库(格式同 zero.json: {"模型名": ["输出1", "输出2", ...], ...}), 生成模型文件
     python3 modeltrace.py prep zero.json [more.json ...] [-o 模型文件]
   输入路径以 / 结尾时，合并该目录直接包含的所有 .json 文件（不递归），可与文件路径混用。

2) 查询: 给一段输出, 返回最像的模型
     python3 modeltrace.py query --file out.txt        (不给 --file 则读 stdin)
     python3 modeltrace.py query --file many.txt --multi
         --multi: 文件里是同一个未知模型的多条输出, 条与条之间空一行(或单独一行 ---), 合并判断

3) 后端接口用 (由 code/lib/modeltrace.cpp 调用, stdin 读一个 JSON 请求, stdout 写一个 JSON 响应):
     python3 modeltrace.py api
   请求 {"action":"models"}
     -> {"model_count":28,"sample_count":700,"built_at":时间戳,"cv":{...},
         "models":[{"name":"gpt-6-astra","family":"gpt","samples":25},...]}
   请求 {"action":"query","top":5,"groups":[{"name":"未知A","samples":["输出1","输出2"]},...]}
     -> {"results":[{"name":"未知A","samples":2,"predicted":..,"top":[{"model":..,"score":..}],"family":{..},
                     "votes":{..},"per_sample":[..]},...]}
   出错 -> {"error":{"message":"...","code":"bad_request|not_ready|internal"}}

模型文件默认是 /web/res/model.pkl, 可用 -o / --model 指定。

打分方法: 约 107 个统计特征到各模型"平均画像"的马氏距离(收缩协方差 LDA),
在候选模型间做 z 标准化后 softmax 成百分比。
"""
import sys, os, json, math, pickle, argparse, collections, re, time, random

try:
    import numpy as np
except ImportError:
    _msg = '缺少 numpy, 请先安装:  pip3 install numpy   (或 apt install -y python3-numpy)'
    if sys.argv[1:2] == ['api']:
        print(json.dumps({'error': {'message': _msg, 'code': 'not_ready'}})); sys.exit(0)
    sys.exit(_msg)

DEFAULT_MODEL = '/web/res/model.pkl'

SCALE = 3.0                          # z 标准化后的放大倍数 (只影响温度的取值范围)
SHRINK = 0.2                         # LDA 协方差向对角收缩的比例
HEAD = 6                             # 开头取前几个字母做位置特征
CV_FOLDS = 5
MAX_GROUPS, MAX_SAMPLES, MAX_CHARS = 20, 60, 20000   # 接口单次请求的上限: 组数 / 样本总数 / 单条样本字符数

# ---------------------------------------------------------------- 文本切分
AL = 'abcdefghijklmnopqrstuvwxyz'
KB = 'qwertyuiopasdfghjklzxcvbnm'
ROWS = ['qwertyuiop', 'asdfghjkl', 'zxcvbnm']
KPOS = {c: (r, i) for r, row in enumerate(ROWS) for i, c in enumerate(row)}
LEFT = set('qwertasdfgzxcvb')
VOW = 'aeiou'


def letters(s):
    return ''.join(c for c in s if c.isascii() and c.isalpha())


def lines(s):
    """按行取出字母, 全部转小写"""
    return [x for x in (letters(l).lower() for l in s.split('\n')) if x]


# ---------------------------------------------------------------- 统计特征 (全小写)
def ent(cnt):
    n = sum(cnt)
    return -sum(c / n * math.log(c / n) for c in cnt if c) if n else 0


def features(s):
    L = lines(s)
    t = ''.join(L); n = len(t); F = {}
    if n < 10:
        t = (t + 'ab') * 5; n = len(t); L = L or [t]
    cnt = collections.Counter(t)
    # 字母频率
    for c in AL: F['f_' + c] = math.sqrt(cnt[c] / n)
    F['vow'] = sum(cnt[c] for c in VOW) / n
    F['vv'] = sum(t[i] in VOW and t[i + 1] in VOW for i in range(n - 1)) / (n - 1)
    F['rare'] = sum(cnt[c] for c in 'qxzjkv') / n
    F['fent'] = ent([cnt[c] for c in AL])
    F['fmax'] = max(cnt.values()) / n
    F['fmin'] = min(cnt[c] for c in AL) / n
    for r, row in enumerate(ROWS): F['row%d' % r] = sum(cnt[c] for c in row) / n
    # 相邻字母在键盘 / 字母表上怎么走
    kb = [KB.find(c) for c in t]; al = [AL.find(c) for c in t]

    def step(pos, d):
        return sum(pos[i + 1] - pos[i] == d for i in range(n - 1)) / (n - 1)
    F['kb+1'] = step(kb, 1); F['kb-1'] = step(kb, -1); F['al+1'] = step(al, 1); F['al-1'] = step(al, -1)
    F['al+2'] = step(al, 2); F['al-2'] = step(al, -2)
    sr = sh = dist = 0
    for i in range(n - 1):
        a = KPOS[t[i]]; b = KPOS[t[i + 1]]
        sr += a[0] == b[0]; sh += (t[i] in LEFT) == (t[i + 1] in LEFT); dist += abs(a[0] - b[0]) + abs(a[1] - b[1])
    F['samerow'] = sr / (n - 1); F['samehand'] = sh / (n - 1); F['kdist'] = dist / (n - 1)
    dh = collections.Counter(min(12, abs(al[i] - al[i + 1])) for i in range(n - 1))
    for d in range(1, 13): F['ad%d' % d] = dh[d] / (n - 1)
    F['adsign'] = sum(t[i + 1] > t[i] for i in range(n - 1)) / (n - 1)
    h = n // 2
    kadj = [abs(kb[i] - kb[i + 1]) == 1 for i in range(n - 1)]
    F['d_kb'] = sum(kadj[h:]) / max(1, n - 1 - h) - sum(kadj[:h - 1]) / max(1, h - 1)
    # 重复与重现距离
    F['dbl'] = sum(t[i] == t[i + 1] for i in range(n - 1)) / (n - 1)
    F['dbl2'] = sum(t[i] == t[i + 2] for i in range(n - 2)) / max(1, n - 2)
    last = {}; g = []
    for i, c in enumerate(t):
        if c in last: g.append(i - last[c])
        last[c] = i
    ng = max(1, len(g))
    for a, b in ((1, 5), (6, 10), (11, 15), (16, 20), (21, 26), (27, 35), (36, 50), (51, 9999)):
        F['gap%d' % a] = sum(a <= x <= b for x in g) / ng
    F['gap20'] = sum(x == 20 for x in g) / ng; F['gap26'] = sum(x == 26 for x in g) / ng
    F['gapmean'] = math.log(sum(g) / ng + 1)
    for w in (10, 20, 26):
        if n >= w: F['win%d' % w] = sum(len(set(t[i:i + w])) for i in range(0, n - w + 1)) / (n - w + 1) / w
        else: F['win%d' % w] = 1
    for k in (2, 3, 5, 8):
        gr = collections.Counter(t[i:i + k] for i in range(n - k + 1))
        F['rep%d' % k] = sum(v for v in gr.values() if v > 1) / max(1, n - k + 1)
        F['dist%d' % k] = len(gr) / max(1, n - k + 1)
    gr = collections.Counter(t[i:i + 2] for i in range(n - 1))
    F['bient'] = ent(list(gr.values())) / math.log(max(2, n - 1))
    # 行
    nl = len(L)
    F['dline'] = sum(len(set(l)) for l in L) / nl / (sum(len(l) for l in L) / nl)
    F['duplines'] = (nl - len(set(L))) / nl
    F['lstart_dist'] = len(set(l[0] for l in L)) / nl
    F['lstart_vow'] = sum(l[0] in VOW for l in L) / nl
    F['lstart_ent'] = ent(list(collections.Counter(l[0] for l in L).values()))
    F['vert'] = sum(sum(a == b for a, b in zip(L[i], L[i + 1])) for i in range(nl - 1)) / max(1, n - len(L[0]))
    # 开头怎么起手: 前 HEAD 个字母各自的 字母表位置 / 键盘行 / 键盘列
    for j in range(HEAD):
        c = t[j]
        F['h%d_al' % j] = AL.find(c) / 25; F['h%d_row' % j] = KPOS[c][0] / 2; F['h%d_col' % j] = KPOS[c][1] / 9
    return F


# ---------------------------------------------------------------- 线性判别 (numpy)
def zrow(v):
    v = np.asarray(v, dtype=float)
    return (v - v.mean()) / (v.std() + 1e-9)


def lda_fit(X, y, K):
    """X: 特征矩阵, y: 类别下标。返回打分所需的量。"""
    X = np.asarray(X, dtype=float); y = np.asarray(y)
    mu = X.mean(0); sd = X.std(0) + 1e-6
    Z = (X - mu) / sd
    means = np.array([Z[y == k].mean(0) for k in range(K)])
    W = Z - means[y]
    S = W.T @ W / len(Z)
    S = (1 - SHRINK) * S + SHRINK * np.diag(np.diag(S)) + 1e-3 * np.eye(len(S))
    Pm = means @ np.linalg.inv(S)          # P 对称, 每行是 P·mean_k
    return {'mu': mu, 'sd': sd, 'Pm': Pm, 'mPm': (means * Pm).sum(1)}


def lda_score(L, f):
    # 马氏距离里与类别无关的 z'Pz 项在按行 z 标准化时会被消掉, 因此省略
    z = np.clip((np.asarray(f, dtype=float) - L['mu']) / L['sd'], -6, 6)
    return -0.5 * (L['mPm'] - 2 * (L['Pm'] @ z)) / len(z)


# ---------------------------------------------------------------- 模型
NAMES = sorted(features('ab' * 20))


def prepare(text, names=None):
    F = features(text)
    return {'F': [F[k] for k in (names or NAMES)]}


def fit(samples, y, classes):
    return {'version': 3, 'classes': classes, 'names': NAMES,
            'lda': lda_fit([s['F'] for s in samples], y, len(classes)), 'T': 1.0}


def raw_scores(M, s):
    return SCALE * zrow(lda_score(M['lda'], s['F']))


def softmax(S, T):
    S = np.asarray(S, dtype=float); e = np.exp(T * (S - S.max()))
    return e / e.sum()


def family(name):
    return name.split('-')[0]


def family_scores(classes, p):
    fam = {}
    for c, v in zip(classes, p): fam[family(c)] = fam.get(family(c), 0) + float(v)
    return {k: round(v, 4) for k, v in sorted(fam.items(), key=lambda x: -x[1])}


NOTE = 'score 是候选模型之间的相对分数, 只在样本库内校准'


def query_one(M, text, top=5):
    classes = M['classes']
    p = softmax(raw_scores(M, prepare(text, M['names'])), M['T'])
    r = sorted(range(len(p)), key=lambda j: -p[j])
    tot = len(letters(text))
    return {'predicted': classes[r[0]],
            'top': [{'model': classes[j], 'score': round(float(p[j]), 4)} for j in r[:top]],
            'family': family_scores(classes, p),
            'letters': tot,
            'note': NOTE}, p


def query_multi(M, texts, top=5):
    classes = M['classes']; K = len(classes)
    per = [query_one(M, t) for t in texts]
    L = np.mean([np.log(p + 1e-12) for _, p in per], axis=0)
    q = softmax(L, 1.0)
    r = sorted(range(K), key=lambda j: -q[j])
    votes = {}
    for o, _ in per: votes[o['predicted']] = votes.get(o['predicted'], 0) + 1
    return {'samples': len(texts), 'predicted': classes[r[0]],
            'top': [{'model': classes[j], 'score': round(float(q[j]), 4)} for j in r[:top]],
            'votes': dict(sorted(votes.items(), key=lambda x: -x[1])),
            'family': family_scores(classes, q),
            'per_sample': [{'predicted': o['predicted'], 'score': o['top'][0]['score'], 'top': o['top'][:3],
                            'letters': o['letters']} for o, _ in per],
            'note': NOTE}


def split_samples(t):
    sep = r'(?m)^\s*-{3,}\s*$'
    parts = re.split(sep, t) if re.search(sep, t) else re.split(r'\n\s*\n', t)
    return [x.strip() for x in parts if letters(x)]


# ---------------------------------------------------------------- 预处理
def read_lib(paths):
    files = []
    for p in paths:
        if p.endswith('/'):
            if not os.path.isdir(p): raise SystemExit('%s: 不是目录' % p)
            found = [os.path.join(p, n) for n in sorted(os.listdir(p))
                     if n.endswith('.json') and os.path.isfile(os.path.join(p, n))]
            if not found: raise SystemExit('%s: 目录下没有 .json 样本文件' % p)
            files.extend(found)
        else: files.append(p)
    texts, labels, classes = [], [], []
    by_class = {}
    for p in files:
        with open(p, encoding='utf-8') as f: D = json.load(f)
        if not isinstance(D, dict): raise SystemExit('%s: 需要 {"模型名": ["输出", ...]} 格式' % p)
        for k, v in D.items():
            v = [t for t in v if isinstance(t, str) and letters(t)]
            if not v: continue
            if k not in by_class: classes.append(k); by_class[k] = []
            by_class[k].extend(v)
    for k, name in enumerate(classes):
        v = by_class[name]
        random.shuffle(v)                       # 同名模型跨文件合并后, 再随机打乱
        texts += v; labels += [k] * len(v)
    return texts, labels, classes


def calibrate(samples, y, classes, log):
    """交叉验证: 选 softmax 温度, 顺便给出库内准确率"""
    K = len(classes); n = len(y); seen = [0] * K; fold = []
    for k in y: fold.append(seen[k] % CV_FOLDS); seen[k] += 1
    S = [None] * n
    for f in range(CV_FOLDS):
        tr = [i for i in range(n) if fold[i] != f]; te = [i for i in range(n) if fold[i] == f]
        if not te or not tr: continue
        M = fit([samples[i] for i in tr], [y[i] for i in tr], classes)
        for i in te: S[i] = raw_scores(M, samples[i])
        log('交叉验证 %d/%d' % (f + 1, CV_FOLDS))
    idx = [i for i in range(n) if S[i] is not None]
    best = None
    for j in range(40):
        T = math.exp(math.log(0.05) + (math.log(3) - math.log(0.05)) * j / 39)
        ll = -sum(math.log(softmax(S[i], T)[y[i]] + 1e-300) for i in idx) / len(idx)
        if best is None or ll < best[0]: best = (ll, T)
    rank = [int((S[i] > S[i][y[i]]).sum()) + 1 for i in idx]
    fam = sum(family(classes[int(np.argmax(S[i]))]) == family(classes[y[i]]) for i in idx)
    m = float(len(idx))
    return float(best[1]), {'n': len(idx), 'top1': sum(r == 1 for r in rank) / m, 'top3': sum(r <= 3 for r in rank) / m,
                     'top5': sum(r <= 5 for r in rank) / m, 'family_top1': fam / m, 'log_loss': float(best[0])}


def cmd_prep(a):
    log = lambda s: sys.stderr.write(s + '\n') or sys.stderr.flush()
    texts, y, classes = read_lib(a.data)
    if len(classes) < 2: raise SystemExit('样本库里至少要有 2 个有样本的模型')
    log('样本 %d 条, 模型 %d 个, 提取特征...' % (len(texts), len(classes)))
    samples = [prepare(t) for t in texts]
    T, rep = (1.0, None) if a.no_cv else calibrate(samples, y, classes, log)
    log('训练最终模型...')
    M = fit(samples, y, classes); M['T'] = T; M['cv'] = rep
    M['counts'] = [y.count(k) for k in range(len(classes))]; M['built_at'] = int(time.time())
    out = os.path.abspath(a.model); d = os.path.dirname(out)
    if not os.path.isdir(d): os.makedirs(d)
    tmp = out + '.tmp'
    with open(tmp, 'wb') as f: pickle.dump(M, f, protocol=4)
    os.replace(tmp, out)
    print(json.dumps({'model': out, 'classes': len(classes), 'samples': len(texts), 'temperature': T, 'cv': rep},
                     ensure_ascii=False, indent=1))


def load_model(path):
    if not os.path.isfile(path):
        raise SystemExit('找不到模型文件 %s, 请先运行: python3 modeltrace.py prep zero.json' % os.path.abspath(path))
    with open(path, 'rb') as f: M = pickle.load(f)
    if M.get('version') != 3 or M.get('names') != NAMES:
        raise SystemExit('模型文件 %s 不是当前版本的 modeltrace.py 生成的, 请重新运行 prep' % os.path.abspath(path))
    return M


def cmd_query(a):
    M = load_model(a.model)
    if a.file:
        with open(a.file, encoding='utf-8') as f: t = f.read()
    else:
        t = sys.stdin.read()
    if a.multi:
        parts = split_samples(t)
        if not parts: raise SystemExit('输入里没有字母')
        out = query_multi(M, parts, a.top)
    else:
        out = query_one(M, t, a.top)[0]
    print(json.dumps(out, ensure_ascii=False, indent=1))


# ---------------------------------------------------------------- 后端接口
class ApiError(Exception):
    def __init__(self, message, code='bad_request'):
        Exception.__init__(self, message); self.code = code


def api(req, model_path):
    if not isinstance(req, dict): raise ApiError('请求体必须是 JSON 对象')
    action = req.get('action')
    if action not in ('models', 'query'): raise ApiError('action 只能是 models 或 query')
    try:
        M = load_model(model_path)
    except SystemExit as e:
        raise ApiError(str(e), 'not_ready')
    classes = M['classes']
    if action == 'models':
        counts = M.get('counts') or [None] * len(classes)
        return {'model_count': len(classes), 'sample_count': sum(c or 0 for c in counts) or None,
                'built_at': M.get('built_at'), 'cv': M.get('cv'),
                'models': [{'name': c, 'family': family(c), 'samples': n} for c, n in zip(classes, counts)]}
    groups = req.get('groups')
    if not isinstance(groups, list) or not groups: raise ApiError('groups 必须是非空数组')
    if len(groups) > MAX_GROUPS: raise ApiError('一次最多查询 %d 组' % MAX_GROUPS)
    top = req.get('top', 5)
    if not isinstance(top, int) or isinstance(top, bool) or not 1 <= top <= len(classes): top = 5
    total = 0; parsed = []
    for i, g in enumerate(groups):
        if not isinstance(g, dict) or not isinstance(g.get('samples'), list): raise ApiError('第 %d 组缺少 samples 数组' % (i + 1))
        name = g.get('name')
        name = name.strip()[:80] if isinstance(name, str) and name.strip() else '第 %d 组' % (i + 1)
        texts = [t[:MAX_CHARS] for t in g['samples'] if isinstance(t, str) and letters(t[:MAX_CHARS])]
        total += len(texts); parsed.append((name, texts))
    if total > MAX_SAMPLES: raise ApiError('一次最多查询 %d 条样本' % MAX_SAMPLES)
    results = []
    for name, texts in parsed:
        if not texts:
            results.append({'name': name, 'samples': 0, 'error': '这一组没有包含英文字母的样本'}); continue
        r = query_multi(M, texts, top); del r['note']; r['name'] = name
        results.append(r)
    return {'results': results, 'note': NOTE}


def cmd_api(a):
    try:
        out = api(json.loads(sys.stdin.buffer.read().decode('utf-8')), a.model)
    except ApiError as e:
        out = {'error': {'message': str(e), 'code': e.code}}
    except ValueError:
        out = {'error': {'message': '请求体不是合法的 UTF-8 JSON', 'code': 'bad_request'}}
    except Exception as e:
        out = {'error': {'message': '内部错误: %s' % e, 'code': 'internal'}}
    sys.stdout.write(json.dumps(out)); sys.stdout.flush()      # 默认 ensure_ascii, 不受终端编码影响


def main():
    ap = argparse.ArgumentParser(description='模型字母指纹: 预处理(prep) / 查询(query)')
    sub = ap.add_subparsers(dest='cmd'); sub.required = True
    p = sub.add_parser('prep', help='用样本库生成模型文件')
    p.add_argument('data', nargs='+', help='样本库 json 或以 / 结尾的目录（仅直接包含的 .json），可给多个')
    p.add_argument('-o', '--model', default=DEFAULT_MODEL, help='模型文件输出路径')
    p.add_argument('--no-cv', action='store_true', help='跳过交叉验证(更快, 但分数不做校准)')
    p.set_defaults(fn=cmd_prep)
    q = sub.add_parser('query', help='查询一段输出最像哪个模型')
    q.add_argument('--file', help='待查询的输出文件, 不给则读 stdin')
    q.add_argument('--model', default=DEFAULT_MODEL, help='模型文件路径')
    q.add_argument('--multi', action='store_true', help='输入是同一模型的多条输出(空行或 --- 分隔), 合并判断')
    q.add_argument('--top', type=int, default=5, help='输出前几名')
    q.set_defaults(fn=cmd_query)
    w = sub.add_parser('api', help='后端接口用: stdin 读 JSON 请求, stdout 写 JSON 响应')
    w.add_argument('--model', default=DEFAULT_MODEL, help='模型文件路径')
    w.set_defaults(fn=cmd_api)
    a = ap.parse_args(); a.fn(a)


if __name__ == '__main__':
    main()
