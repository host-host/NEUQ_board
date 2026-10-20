// 保留每个用户最后 500 条历史。
// g++ -O2 -Wall -Wextra code/unused/dbclean4.cpp build/ndb2.o -o build/dbclean4
#include "../lib/ndb2.h"
#include "../lib/gptapi5.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
struct history{
    int n;
    char user_id[10];
    char con_id[][32];
};
int main() {
    ndb2 content_db;//con_id -> content
    ndb2 history_db;//user_id -> history
    content_db=ndb2_init("/web/res/pri/gpt5content.ndb2");
    history_db=ndb2_init("/web/res/pri/gpt5userhistory.ndb2");
    char key[48]={0};
    int trimmed=0;
    for (history* h = (history*)ndb2_next(history_db, key); h; h = (history*)ndb2_next(history_db, key)) {
        int n = h->n;
        if (n <= 500) continue;
        for(int i=0;i<n-500;i++){
            content* p=(content*)ndb2_got(content_db,h->con_id[i],0);
            if(!p)puts("DB ERROR\n");
            else p->deleted=std::max((int)p->deleted,1);
        }
        memmove(h->con_id, h->con_id + n - 500, 500 * 32);
        memset(h->con_id + 500, 0, (size_t)(n - 500) * 32);
        h->n = 500;
        trimmed += n - 500;
    }
    printf("%d\n",trimmed);
    return 0;
}
