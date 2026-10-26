// 保留每个用户最后 500 条历史。
// g++ -O2 -Wall -Wextra code/unused/dbclean4.cpp build/ndb2.o -o build/dbclean4
#include "../lib/ndb2.h"
#include "../lib/gptapi5.h"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>
struct history{
    int n;
    char user_id[10];
    char con_id[][32];
};
struct history_entry{
    char con_id[32];
    content* con;
    long long updatetime;
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
        std::vector<history_entry> entries(n);
        for(int i=0;i<n;i++){
            memcpy(entries[i].con_id,h->con_id[i],sizeof(entries[i].con_id));
            content* p=(content*)ndb2_got(content_db,h->con_id[i],0);
            entries[i].con=p;
            entries[i].updatetime=p?p->updatetime:0;
        }
        std::sort(entries.begin(),entries.end(),[](const history_entry& a,const history_entry& b){
            return a.updatetime<b.updatetime;
        });
        for(int i=0;i<n-500;i++){
            content* p=entries[i].con;
            if(!p)puts("DB ERROR\n");
            else p->deleted=std::max((int)p->deleted,1);
        }
        for(int i=0;i<500;i++)memcpy(h->con_id[i],entries[n-500+i].con_id,sizeof(h->con_id[i]));
        h->n = 500;
        trimmed += n - 500;
    }
    printf("%d\n",trimmed);
    return 0;
}
