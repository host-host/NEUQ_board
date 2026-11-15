#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""模型字母指纹 (modeltrace): 根据"随机 400 字母"任务的输出判断最像哪个模型。

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

模型文件默认是 <项目根>/build/modeltrace/model.pkl, 可用 -o / --model 指定。

打分方法(三路, 各自在候选模型间做 z 标准化后按 3:1:2 加权, 再 softmax 成百分比):
  a. 约 140 个统计特征到各模型"平均画像"的马氏距离(收缩协方差 LDA)
  b. 全文字符 n-gram 似然
  c. 开头两行的字符 n-gram 似然
"""
import sys, os, json, math, pickle, argparse, collections, re, time, random

try:
    import numpy as np
except ImportError:
    _msg = '缺少 numpy, 请先安装:  pip3 install numpy   (或 apt install -y python3-numpy)'
    if sys.argv[1:2] == ['api']:
        print(json.dumps({'error': {'message': _msg, 'code': 'not_ready'}})); sys.exit(0)
    sys.exit(_msg)

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_MODEL = os.path.join(HERE, '..', '..', 'build', 'modeltrace', 'model.pkl')

W_LDA, W_NG, W_PRE = 3.0, 1.0, 2.0   # 三路权重
SHRINK = 0.2                         # LDA 协方差向对角收缩的比例
PRE_LINES = 2                        # "开头"取前几行
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
    return [x for x in (letters(l) for l in s.split('\n')) if x]


def cs(L):
    return '^' + '\n'.join(L)


def cpat(x):
    return ''.join('U' if c.isupper() else 'L' if c.islower() else c for c in x)


# ---------------------------------------------------------------- 统计特征
def ent(cnt):
    n = sum(cnt)
    return -sum(c / n * math.log(c / n) for c in cnt if c) if n else 0


def features(s):
    L = lines(s)
    t = ''.join(L); n = len(t); F = {}
    if n < 10:
        t = (t + 'aB') * 5; n = len(t); L = L or [t]
    lo = t.lower(); up = [c.isupper() for c in t]
    cnt = collections.Counter(lo)
    for c in AL: F['f_' + c] = math.sqrt(cnt[c] / n)
    lc = collections.Counter(c for c in t if c.islower())
    for c in AL:
        a = lc[c]; b = cnt[c] - a
        F['c_' + c] = math.log((a + 1) / (b + 1))
    F['low'] = sum(not u for u in up) / n
    F['alt'] = sum(up[i] != up[i + 1] for i in range(n - 1)) / (n - 1)
    runs = []; r = 1
    for i in range(1, n):
        if up[i] == up[i - 1]: r += 1
        else: runs.append((up[i - 1], r)); r = 1
    runs.append((up[-1], r))
    for k in (1, 2, 3):
        F['runL%d' % k] = sum(1 for u, x in runs if not u and (x == k if k < 3 else x >= 3)) / len(runs)
        F['runU%d' % k] = sum(1 for u, x in runs if u and (x == k if k < 3 else x >= 3)) / len(runs)
    tri = collections.Counter(tuple(up[i:i + 3]) for i in range(n - 2))
    for a in (0, 1):
        for b in (0, 1):
            for c in (0, 1): F['ct%d%d%d' % (a, b, c)] = tri[(bool(a), bool(b), bool(c))] / max(1, n - 2)
    nv = sum(cnt[c] for c in VOW); nvl = sum(lc[c] for c in VOW)
    F['vow'] = nv / n
    F['vlow'] = math.log((nvl + 1) / (nv - nvl + 1))
    F['vv'] = sum(lo[i] in VOW and lo[i + 1] in VOW for i in range(n - 1)) / (n - 1)
    F['rare'] = sum(cnt[c] for c in 'qxzjkv') / n
    F['fent'] = ent([cnt[c] for c in AL])
    F['fmax'] = max(cnt.values()) / n
    F['fmin'] = min(cnt[c] for c in AL) / n
    kb = [KB.find(c) for c in lo]; al = [AL.find(c) for c in lo]

    def step(pos, d):
        return sum(pos[i + 1] - pos[i] == d for i in range(n - 1)) / (n - 1)
    F['kb+1'] = step(kb, 1); F['kb-1'] = step(kb, -1); F['al+1'] = step(al, 1); F['al-1'] = step(al, -1)
    F['al+2'] = step(al, 2); F['al-2'] = step(al, -2)
    F['dbl'] = sum(lo[i] == lo[i + 1] for i in range(n - 1)) / (n - 1)
    F['dbl2'] = sum(lo[i] == lo[i + 2] for i in range(n - 2)) / max(1, n - 2)
    F['sameT'] = sum(t[i] == t[i + 1] for i in range(n - 1)) / (n - 1)
    sr = sh = dist = 0
    for i in range(n - 1):
        a = KPOS[lo[i]]; b = KPOS[lo[i + 1]]
        sr += a[0] == b[0]; sh += (lo[i] in LEFT) == (lo[i + 1] in LEFT); dist += abs(a[0] - b[0]) + abs(a[1] - b[1])
    F['samerow'] = sr / (n - 1); F['samehand'] = sh / (n - 1); F['kdist'] = dist / (n - 1)
    for r, row in enumerate(ROWS): F['row%d' % r] = sum(cnt[c] for c in row) / n
    dh = collections.Counter(min(12, abs(al[i] - al[i + 1])) for i in range(n - 1))
    for d in range(13): F['ad%d' % d] = dh[d] / (n - 1)
    F['adsign'] = sum(lo[i + 1] > lo[i] for i in range(n - 1)) / (n - 1)
    last = {}; g = []
    for i, c in enumerate(lo):
        if c in last: g.append(i - last[c])
        last[c] = i
    ng = max(1, len(g))
    for a, b in ((1, 5), (6, 10), (11, 15), (16, 20), (21, 26), (27, 35), (36, 50), (51, 9999)):
        F['gap%d' % a] = sum(a <= x <= b for x in g) / ng
    F['gap20'] = sum(x == 20 for x in g) / ng; F['gap26'] = sum(x == 26 for x in g) / ng
    F['gapmean'] = math.log(sum(g) / ng + 1)
    for w in (10, 20, 26):
        if n >= w: F['win%d' % w] = sum(len(set(lo[i:i + w])) for i in range(0, n - w + 1)) / (n - w + 1) / w
        else: F['win%d' % w] = 1
    for k in (2, 3, 5, 8):
        gr = collections.Counter(lo[i:i + k] for i in range(n - k + 1))
        F['rep%d' % k] = sum(v for v in gr.values() if v > 1) / max(1, n - k + 1)
        F['dist%d' % k] = len(gr) / max(1, n - k + 1)
    gr = collections.Counter(t[i:i + 2] for i in range(n - 1))
    F['bient'] = ent(list(gr.values())) / math.log(max(2, n - 1))
    nl = len(L)
    F['dline'] = sum(len(set(l.lower())) for l in L) / nl / (sum(len(l) for l in L) / nl)
    F['duplines'] = (nl - len(set(L))) / nl
    F['lstart_up'] = sum(l[0].isupper() for l in L) / nl
    F['lend_up'] = sum(l[-1].isupper() for l in L) / nl
    F['lstart_alt'] = sum(L[i][0].isupper() != L[i + 1][0].isupper() for i in range(nl - 1)) / max(1, nl - 1)
    F['lstart_dist'] = len(set(l[0].lower() for l in L)) / nl
    F['lstart_vow'] = sum(l[0].lower() in VOW for l in L) / nl
    F['lstart_ent'] = ent(list(collections.Counter(l[0].lower() for l in L).values()))
    F['cross'] = sum(L[i][-1].isupper() != L[i + 1][0].isupper() for i in range(nl - 1)) / max(1, nl - 1)
    F['vert'] = sum(sum(a.lower() == b.lower() for a, b in zip(L[i], L[i + 1])) for i in range(nl - 1)) / max(1, n - len(L[0]))
    for j in range(3):
        col = [l[j] for l in L if len(l) > j]
        F['colup%d' % j] = sum(c.isupper() for c in col) / max(1, len(col))
    h = n // 2
    kadj = [abs(kb[i] - kb[i + 1]) == 1 for i in range(n - 1)]
    calt = [up[i] != up[i + 1] for i in range(n - 1)]
    F['d_alt'] = sum(calt[h:]) / max(1, n - 1 - h) - sum(calt[:h - 1]) / max(1, h - 1)
    F['d_kb'] = sum(kadj[h:]) / max(1, n - 1 - h) - sum(kadj[:h - 1]) / max(1, h - 1)
    return F


# ---------------------------------------------------------------- n-gram (Witten-Bell 回退)
# 通道: (名字, 取哪种串, 阶数, 词表大小, 权重)
CHANNELS = (('cs4', 'cs', 4, 54, 1.0), ('ci4', 'ci', 4, 28, 1.0), ('cp6', 'cp', 6, 4, 0.5),
            ('cs2', 'cs', 2, 54, 0.5), ('ci2', 'ci', 2, 28, 0.5))
PRE_CHANNELS = (('cs4', 'pcs', 4, 54), ('ci4', 'pci', 4, 28))


def ng_new(n):
    return ([{} for _ in range(n)], [{} for _ in range(n)])


def ng_add(g, x):
    c, tot = g; n = len(c)
    for i in range(1, len(x)):
        ch = x[i]
        for k in range(n):
            if i - k < 0: break
            ctx = x[i - k:i]
            d = c[k].get(ctx)
            if d is None: d = c[k][ctx] = {}
            d[ch] = d.get(ch, 0) + 1
            tot[k][ctx] = tot[k].get(ctx, 0) + 1


def ng_ll(g, V, x):
    c, tot = g; n = len(c); total = 0.0
    for i in range(1, len(x)):
        pr = 1.0 / V; ch = x[i]
        for k in range(n):
            if i - k < 0: break
            ctx = x[i - k:i]
            t = tot[k].get(ctx)
            if not t: break
            d = c[k][ctx]; T = len(d)
            pr = (d.get(ch, 0) + T * pr) / (t + T)
        total += math.log(pr)
    return total / max(1, len(x) - 1)


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
def prepare(text, names=None):
    L = lines(text); F = features(text)
    a = cs(L); b = cs(L[:PRE_LINES])
    return {'F': [F[k] for k in (names or sorted(F))], 'cs': a, 'ci': a.lower(), 'cp': cpat(a),
            'pcs': b, 'pci': b.lower()}


def fit(samples, y, classes):
    K = len(classes)
    ng = {name: [ng_new(n) for _ in range(K)] for name, _, n, _, _ in CHANNELS}
    pre = {name: [ng_new(n) for _ in range(K)] for name, _, n, _ in PRE_CHANNELS}
    for s, k in zip(samples, y):
        for name, key, n, V, w in CHANNELS: ng_add(ng[name][k], s[key])
        for name, key, n, V in PRE_CHANNELS: ng_add(pre[name][k], s[key])
    return {'version': 2, 'classes': classes, 'names': sorted(features('aB' * 20)), 'ng': ng, 'pre': pre,
            'lda': lda_fit([s['F'] for s in samples], y, K), 'T': 1.0}


def raw_scores(M, s):
    out = W_LDA * zrow(lda_score(M['lda'], s['F']))
    for name, key, n, V, w in CHANNELS:
        out = out + W_NG * w * zrow([ng_ll(g, V, s[key]) for g in M['ng'][name]])
    p = np.zeros(len(M['classes']))
    for name, key, n, V in PRE_CHANNELS:
        p += [ng_ll(g, V, s[key]) for g in M['pre'][name]]
    return out + W_PRE * zrow(p)


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
        del M                                  # 及时释放, 避免同时占两份模型的内存
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
    if M.get('version') != 2:
        raise SystemExit('模型文件 %s 是旧版本生成的, 请重新运行 prep' % os.path.abspath(path))
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
