// 临时工具：先移除不存在的对话引用，再按原顺序保留每个用户最后 1000 条历史。
// g++ -O2 -Wall -Wextra code/unused/dbclean4.cpp build/ndb2.o -o build/dbclean4
#include "../lib/ndb2.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
struct history { int n; char user_id[10]; char con_id[][32]; };

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "/web/res/pri";
    std::string hp = dir + "/gpt5userhistory.ndb2", cp = dir + "/gpt5content.ndb2";
    if (access(hp.c_str(), F_OK) || access(cp.c_str(), F_OK)) return perror("数据库"), 1;
    ndb2 conversations = ndb2_init_readonly(cp.c_str()), histories = ndb2_init(hp.c_str());
    if (!conversations || !histories) return fputs("无法打开数据库\n", stderr), 1;
    char key[48] = {};
    long long users = 0, missing = 0, trimmed = 0;
    for (auto* h = (history*)ndb2_next(histories, key); h; h = (history*)ndb2_next(histories, key)) {
        long long len = ndb2_gotmaxlen(h);
        int n = len >= (long long)sizeof(history) ? h->n : -1;
        if (n < 0 || n > (len - (long long)offsetof(history, con_id)) / 32)
            return fprintf(stderr, "历史节点异常：%s，已停止\n", key), 1;
        int kept = 0;
        for (int i = 0; i < n; ++i) {
            char id[33] = {};
            memcpy(id, h->con_id[i], 32);
            if (!ndb2_got(conversations, id, 0)) continue;
            if (kept != i) memcpy(h->con_id[kept], h->con_id[i], 32);
            ++kept;
        }
        memset(h->con_id + kept, 0, (size_t)(n - kept) * 32);
        h->n = kept;
        missing += n - kept;
        ++users;
    }
    memset(key, 0, sizeof(key));
    for (auto* h = (history*)ndb2_next(histories, key); h; h = (history*)ndb2_next(histories, key)) {
        int n = h->n;
        if (n <= 1000) continue;
        memmove(h->con_id, h->con_id + n - 1000, 1000 * 32);
        memset(h->con_id + 1000, 0, (size_t)(n - 1000) * 32);
        h->n = 1000;
        trimmed += n - 1000;
    }
    ndb2_free(histories);
    ndb2_free(conversations);
    printf("已整理 %lld 个用户，移除失效引用 %lld 条，裁剪旧历史 %lld 条\n", users, missing, trimmed);
    return 0;
}
