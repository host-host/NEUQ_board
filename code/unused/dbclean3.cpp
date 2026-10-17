// 临时工具：按扫描到的全部用户历史，将无引用对话标为 deleted=3。
//g++ -O2 -Wall -Wextra code/unused/dbclean3.cpp build/ndb2.o -o build/dbclean3
#include "../lib/gptapi5.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>
#include <unistd.h>
struct history { int n; char user_id[10]; char con_id[][32]; };

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "/web/res/pri";
    std::string hp = dir + "/gpt5userhistory.ndb2", cp = dir + "/gpt5content.ndb2";
    if (access(hp.c_str(), F_OK) || access(cp.c_str(), F_OK)) return perror("数据库"), 1;
    ndb2 histories = ndb2_init_readonly(hp.c_str());
    if (!histories) return fputs("无法打开历史库\n", stderr), 1;
    std::unordered_set<std::string> used;
    char key[48] = {};
    for (auto* h = (history*)ndb2_next(histories, key); h; h = (history*)ndb2_next(histories, key)) {
        long long len = ndb2_gotmaxlen(h);
        if (len < (long long)sizeof(history) || h->n < 0 || h->n > (len - (long long)offsetof(history, con_id)) / 32)
            return fprintf(stderr, "历史节点异常：%s，未修改对话\n", key), 1;
        for (int i = 0, n = h->n; i < n; ++i) {
            size_t len = strnlen(h->con_id[i], 32);
            if (!len || len == 32) return fputs("历史对话 ID 异常，未修改对话\n", stderr), 1;
            used.emplace(h->con_id[i], len);
        }
    }
    ndb2_free(histories);
    ndb2 conversations = ndb2_init(cp.c_str());
    if (!conversations) return fputs("无法打开对话库\n", stderr), 1;
    memset(key, 0, sizeof(key));
    long long count = 0;
    for (auto* p = (content*)ndb2_next(conversations, key); p; p = (content*)ndb2_next(conversations, key)) {
        if (ndb2_gotmaxlen(p) < (long long)sizeof(content)) continue;
        if (!used.count(key) && p->deleted != 3) { p->deleted = 3; ++count; }
    }
    ndb2_free(conversations);
    printf("已将 %lld 个无历史引用的对话标记为 deleted=3\n", count);
    return 0;
}
