// compact_gpt.cpp — 重构 gpt_content.dat，删除不再被任何用户引用的对话
// 用法：./compact_gpt
// 会生成新的 gpt_content.dat.new，确认无误后替换原文件
#include "lib/ndb.h"
#include "lib/gptapi2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_set>
#include <string>

extern ndb gpt_con;
extern ndb gpt_user;
static ndb gpt_con_new;

int main() {
    if (ndb_init(&gpt_con_new, "/web/res/pri/gpt_content.dat.new", 32, 0x1000000000LL) != 0) {
        fprintf(stderr, "Failed to create gpt_content.dat.new\n");
        return 1;
    }
    std::unordered_set<std::string> valid_ids;
    char uname[24] = {0};
    int user_count = 0, id_count = 0;
    while (1) {
        gpt_userhistory* uh = (gpt_userhistory*)ndb_next(&gpt_user, uname);
        if (!uh) break;
        user_count++;
        for (int i = 0; i < uh->n; i++) {
            valid_ids.insert(std::string(uh->content[i].a));
            id_count++;
        }
    }
    printf("扫描到 %d 个用户，共 %d 条有效对话引用\n", user_count, id_count);

    // 遍历 gpt_content.dat，只复制被引用的条目
    char cid[32] = {0};
    int copied = 0, skipped = 0;
    while (1) {
        gpt_content* con = (gpt_content*)ndb_next(&gpt_con, cid);
        if (!con) break;
        std::string sid(cid, 31); // ID 最长 31 字节（generate_new_id 生成 31 字符）
        // 也尝试 null-terminated 形式
        sid = std::string(cid);

        if (valid_ids.count(sid) == 0) {
            skipped++;
            continue;
        }

        // 计算 content 长度
        size_t content_len = strlen(con->content);
        size_t total_size = sizeof(gpt_content) + content_len + 1;

        gpt_content* dst = (gpt_content*)ndb_create(&gpt_con_new, cid, (int)total_size);
        if (!dst) {
            fprintf(stderr, "写入失败，id=%s\n", cid);
            continue;
        }
        dst->publish   = con->publish;
        dst->isusing   = false; // 重构后重置 isusing
        memcpy(dst->owner,  con->owner,  sizeof(dst->owner));
        dst->createtime = con->createtime;
        memcpy(dst->name,   con->name,   sizeof(dst->name));
        memcpy(dst->other,  con->other,  sizeof(dst->other));
        memcpy(dst->content, con->content, content_len + 1);
        copied++;
    }

    ndb_free(&gpt_con);
    ndb_free(&gpt_user);
    ndb_free(&gpt_con_new);

    printf("完成：保留 %d 条，跳过 %d 条（已删除）\n", copied, skipped);
    printf("新文件：/web/res/pri/gpt_content.dat.new\n");
    printf("确认无误后执行：\n");
    printf("  cp /web/res/pri/gpt_content.dat /web/res/pri/gpt_content.dat.bak\n");
    printf("  mv /web/res/pri/gpt_content.dat.new /web/res/pri/gpt_content.dat\n");
    return 0;
}
