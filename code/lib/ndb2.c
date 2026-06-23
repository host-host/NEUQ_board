#include"ndb2.h"
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<sys/mman.h>
#include<stdio.h>
#define BLOCK 0x1000
#define LIMIT 1024//128GB限制
#define ll long long
#define SEG (1LL<<27)//128MB
#define MAXLEN (SEG-16)
#define wmb() asm volatile("sfence":::"memory")
#define LOCK(a) while(__sync_val_compare_and_swap(a,0,1))usleep(0)
#define p2p(p) ((point*)(a->a[(p)>>27]+((p)&(SEG-1))))
#define gotlock(p) (a->lock[(p)>>27]+((p)&(SEG-1))/BLOCK)
typedef struct{
    char *a[LIMIT];
    ll filelen,cptr;
    int fd;
    char lenlock,ptrlock,*lock[LIMIT];
}ndb;
typedef struct{
    ll child,next;
    char name[48];
}point;
ndb2 ndb2_init(const char* file){
    ndb *a=malloc(sizeof(ndb));
    if(!a)return 0;
    memset(a,0,sizeof(ndb));
    if((a->fd=open(file,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR))<0)goto out;
    if((a->filelen=lseek(a->fd,0,SEEK_END)/BLOCK*BLOCK)==0){
        ll c[BLOCK/8]={BLOCK-64};
        if(write(a->fd,(char*)c,a->filelen=BLOCK)!=BLOCK)goto out;
    }
    for(int i=0;i<(a->filelen+SEG-1)/SEG;i++){
        a->a[i]=mmap(0,SEG,PROT_READ|PROT_WRITE,MAP_SHARED,a->fd,(ll)i*SEG);
        if(a->a[i]==0||a->a[i]==MAP_FAILED)goto out;
        if((a->lock[i]=malloc(SEG/BLOCK))==0)goto out;
        memset(a->lock[i],0,SEG/BLOCK);
    }
    return a;
    out:
    ndb2_free(a);
    return 0;
}
static ll ndb_new(ndb* a,int sum){
    if(sum>SEG/BLOCK||sum<=0)return 0;
    LOCK(&a->lenlock);
    ll need=(ll)sum*BLOCK,l=a->filelen;
    if(l/SEG!=(l+need-1)/SEG)l=(l/SEG+1)*SEG;
    ll end=l+need,i=l/SEG;
    if(end>(ll)LIMIT*SEG)return a->lenlock=0;
    if(posix_fallocate(a->fd,a->filelen,end-a->filelen))return a->lenlock=0;
    if(a->a[i]==0){
        a->a[i]=mmap(0,SEG,PROT_READ|PROT_WRITE,MAP_SHARED,a->fd,(ll)i*SEG);
        if(a->a[i]==0||a->a[i]==MAP_FAILED)return a->lenlock=0;
        if((a->lock[i]=malloc(SEG/BLOCK))==0)return a->lenlock=0;
        memset(a->lock[i],0,SEG/BLOCK);
    }
    a->filelen=end;
    wmb();
    a->lenlock=0;
    return l;
}
static ll ndb_newcontent(ndb* a,ll len){
    if(len<=0)return 0;
    LOCK(&a->ptrlock);
    len=(len+15)/16*16;
    ll p=a->cptr;
    if((a->cptr+BLOCK-1)/BLOCK==(a->cptr+len+16+BLOCK-1)/BLOCK)a->cptr+=len+16;
    else{
        ll q=ndb_new(a,(len+16+BLOCK-1)/BLOCK);
        if(q==0)return a->ptrlock=0;
        a->cptr=(p=q)+len+16;
    }
    p2p(p)->next=len;
    a->ptrlock=0;
    return p;
}
static ll ndb_w(ndb* a,const char* name,ll p){
	for(ll nex;(nex=p2p(p)->next)&&memcmp(p2p(nex)->name,name,48)<=0;p=nex);
	return p;
}
static int check(ndb*a,ll*p,const char*name){
    asm volatile("lfence":::"memory");
    if(p2p(0)->child!=p[9])return 1;
    for(int i=1;i<=p[0];i++)if(ndb_w(a,name,p[i])!=p[i])return 1;
    return 0;
}
int ndb_c(ndb *a,ll *p,const char* name,ll child){
    ll p2=p[p[0]];
    char* tmplock=gotlock(p2);
    LOCK(tmplock);
    if(check(a,p,name)){
        *tmplock=0;
        return -1;
    }
    point*l=p2p(p2&(~(BLOCK-1)));
    int used[BLOCK/64]={1};
    if(p[0]==1)used[BLOCK/64-1]=1;
    for(point* now=l;now->next;)used[(now=p2p(now->next))-l]=1;
    for(int i=1;i<BLOCK/64;i++)if(!used[i]) {
            point* new=l+i,*curr=p2p(p2);
            new->child=child;
            new->next=curr->next;
            memcpy(new->name,name,48);
            wmb();
            curr->next=(p2&(~(BLOCK-1)))+i*sizeof(point);
            wmb();
            *tmplock=0;
            return 1;
        }
	ll newp=ndb_new(a,1),cnt=0;
	if(newp==0)return *tmplock=0;
    point*pp=p2p(newp);
    if(p[0]==1){
        memcpy(pp,l,BLOCK);
        for(int i=0;i<BLOCK/64;i++)if(pp[i].next)pp[i].next+=newp;
        wmb();
        l->child=newp;
        l->next=0;
        wmb();
        *tmplock=0;
        return -1;
    }
    point* fenl=0;
    for(point* now=l;now!=(point*)a->a[0];now=p2p(now->next))if(++cnt<BLOCK/128)fenl=now;
        else memcpy(pp+cnt-BLOCK/128,now,sizeof(point));
    for(int i=0;i<cnt-BLOCK/128;i++)pp[i].next=newp+(i+1)*sizeof(point);
    pp[cnt-BLOCK/128].next=0;
    p[0]--;
    if((cnt=ndb_c(a,p,pp->name,newp))>0) {
        fenl->next=0;
        wmb();
        *tmplock=0;
        return -1;
    }
    *tmplock=0;
    return cnt;
}
void* ndb2_got(ndb2 handle,const char* key,int flag){
    char name[48]={0};
    if(!key)return 0;
    int n=strlen(key);
    if(n<=0||n>47||flag<0||flag>100*1024*1024)return 0;
    memcpy(name,key,n);
    ndb* a=(ndb*)handle;
    ll p[10],child=0,ret;
	https://neuqboard.cn
    p[9]=p2p(0)->child;
    for(p[p[1]=0]=1;p2p(p[p[0]])->child;p[0]++){
        p[p[0]]=ndb_w(a,name,p[p[0]]);
        p[p[0]+1]=p2p(p[p[0]])->child;
    }
    p[0]--;
    if(memcmp(p2p(p[p[0]])->name,name,48)==0){
        child=p2p(p[p[0]])->child;
        point*cont=p2p(child);
        if(check(a,p,name))goto https;
        if(flag<=cont->next)return cont->name;
        int nlen=cont->next*2>MAXLEN?MAXLEN:cont->next*2;
        if(flag>nlen)nlen=flag;
        char*tmplock=gotlock(p[p[0]]);
        LOCK(tmplock);
        if(check(a,p,name)){
            *tmplock=0;
            goto https;
        }
        if(!(child=ndb_newcontent(a,nlen)))return (void*)(ll)(*tmplock=0);
        if(cont->next)memcpy(p2p(child)->name,cont->name,cont->next);
        wmb();
        p2p(p[p[0]])->child=child;
        wmb();
        *tmplock=0;
        return p2p(child)->name;
    }
    if(flag==0){
        if(check(a,p,name))goto https;
        return 0;
    }
    if(!child)child=ndb_newcontent(a,flag);
    if(!child||!(ret=ndb_c(a,p,name,child)))return 0;
    if(ret==-1)goto https;
    return p2p(child)->name;
}
void ndb2_free(ndb2 handle){
    ndb* a=(ndb*)handle;
    if(!a)return;
    for(int i=0;i<LIMIT;i++){
        if(a->a[i]&&a->a[i]!=MAP_FAILED)munmap(a->a[i],SEG);
        if(a->lock[i])free(a->lock[i]);
    }
    if(a->fd>=0)close(a->fd);
    free(a);
}
void* ndb2_next(ndb2 handle,char* key){
    ndb* a=(ndb*)handle;
    ll p[10],succ,content=0;
    char outname[48];
    out:
    p[9]=p2p(0)->child;
    for(p[p[1]=0]=1;p2p(p[p[0]])->child;p[0]++){
        p[p[0]]=ndb_w(a,key,p[p[0]]);
        p[p[0]+1]=p2p(p[p[0]])->child;
    }
    p[0]--;
    succ=0;
    if(p2p(p[p[0]])->next)succ=p2p(p[p[0]])->next;
    else for(int L=p[0]-1;L>=1;L--)if(p2p(p[L])->next){
        for(succ=p2p(p[L])->next;p2p(p2p(succ)->child)->child;succ=p2p(succ)->child);
        break;
    }
    if(succ){
        content=p2p(succ)->child;
        memcpy(outname,p2p(succ)->name,48);
    }
    if(check(a,p,key))goto out;
    if(!succ)return 0;
    memcpy(key,outname,48);
    return p2p(content)->name;
}
/*以上代码由nit保证正确性，以下代码由AI生成 */
/* ============ 完整性检查与修复: ndb2_fix ============ */
#define FX_NODE 1
#define FX_CONT 2
typedef struct{
    ndb* a; char* role; ll nblk;
    ndb2_fix_return* r; ndb2 nb;
    int op, leafdepth; ll lossy;
}fixctx;
static int fx_voff(ndb*a,ll p){ return p>=0 && p<a->filelen && a->a[p>>27]; }
static int fx_content(ndb*a,ll p){
    if(!fx_voff(a,p)||(p&15))return 0;
    if(p2p(p)->child!=0)return 0;
    ll len=p2p(p)->next;
    if(len<=0||len>100LL*1024*1024)return 0;
    return fx_voff(a,p+16+len-1);
}
static int fx_name(const char*nm){
    int i; for(i=0;i<48;i++)if(!nm[i])break;
    return i>=1 && i<=47;
}
static void fx_emit(fixctx*c,const char*nm,ll cont){
    ndb*a=c->a;
    if(!c->nb)return;
    if(ndb2_got(c->nb,nm,0))return;            /* 去重: 已存在 */
    ll len=p2p(cont)->next;
    void*v=ndb2_got(c->nb,nm,(int)len);
    if(v){ memcpy(v,((char*)p2p(cont))+16,len); c->r->fixed_keys++; }
}
static void fx_crole(fixctx*c,ll cont){
    ndb*a=c->a;
    if(!c->role)return;
    ll len=p2p(cont)->next, b0=cont/BLOCK, b1=(cont+16+len-1)/BLOCK, b;
    for(b=b0;b<=b1 && b<c->nblk;b++){
        if(c->role[b]==FX_NODE){ c->r->error++; c->lossy++; }
        c->role[b]=FX_CONT;
    }
}
/* 遍历一个节点(一个BLOCK内的next链). depth用于叶深一致性检查 */
static void fx_node(fixctx*c,ll head,int depth){
    ndb*a=c->a; ll blk=head/BLOCK;
    if(!fx_voff(a,head)||(head&63)){ c->r->error++; c->lossy++; return; }
    if(depth>64){ c->r->error++; c->lossy++; return; }      /* 兜底防环 */
    if(c->role && blk<c->nblk){
        if(c->role[blk]==FX_NODE || c->role[blk]==FX_CONT){ c->r->error++; c->lossy++; return; }
        c->role[blk]=FX_NODE;
    }
    char prev[48]; int hasprev=0, hop=0;
    ll e=head;
    for(;;){
        point* pe=p2p(e);
        ll child=pe->child, nx=pe->next;
        int empty=!fx_name(pe->name);
        if(!empty){
            if(hasprev && memcmp(prev,pe->name,48)>=0)c->r->error++;   /* 顺序乱,非致命 */
            memcpy(prev,pe->name,48); hasprev=1;
        }
        /* child 分类与空名无关: 节点头(child!=0)→下钻; content→叶值; 否则终止符 */
        if(child && !(child&63) && fx_voff(a,child) && p2p(child)->child!=0){
            fx_node(c,child,depth+1);
        }else if(fx_content(a,child)){
            if(c->leafdepth<0)c->leafdepth=depth;
            else if(c->leafdepth!=depth)c->r->error++;
            if(!empty){
                ll vl=p2p(child)->next;
                c->r->keys++;
                { int kl=strlen(pe->name); if(kl>c->r->keys_max_len)c->r->keys_max_len=kl; }
                if(vl>c->r->value_max_len)c->r->value_max_len=vl;
                fx_crole(c,child);
                fx_emit(c,pe->name,child);
            }
        }else if(!empty && child){                                    /* 真key但child损坏:丢失 */
            c->r->error++; c->lossy++;
        }
        if(!nx)break;
        if(nx==e || (nx>>12)!=blk || (nx&63) || !fx_voff(a,nx)){ c->r->error++; c->lossy++; break; }
        if(++hop>=BLOCK/64){ c->r->error++; c->lossy++; break; } /* 块内最多64跳,防环 */
        e=nx;
    }
}
/* op=2: 全盘扫描所有 point, 锚定到合法 content 的当作叶 entry 抢救.
   从高地址向低地址扫, 配合 fx_emit 去重 => 同名保留偏移最大(最新)那份 */
static void fx_scan(fixctx*c){
    ndb*a=c->a;
    for(ll p=(a->filelen/64)*64-64; p>=0; p-=64){
        if(!fx_voff(a,p))continue;
        point* pe=p2p(p);
        if(!fx_name(pe->name))continue;
        ll child=pe->child;
        if(!fx_content(a,child))continue;        /* 必须精确锚定到 content 头 */
        fx_emit(c,pe->name,child);
    }
}
ndb2_fix_return* ndb2_fix(ndb2 handle,int op,const char* new_file_path){
    ndb* a=(ndb*)handle;
    ndb2_fix_return* r=malloc(sizeof(ndb2_fix_return));
    if(!r)return 0;
    memset(r,0,sizeof(*r));
    if(!a)return r;
    r->filelen=a->filelen;
    fixctx c; memset(&c,0,sizeof(c));
    c.a=a; c.r=r; c.op=op; c.leafdepth=-1;
    c.nblk=(a->filelen+BLOCK-1)/BLOCK;
    c.role=malloc(c.nblk);                        /* 失败则降级为仅靠深度/跳数防环 */
    if(c.role)memset(c.role,0,c.nblk);
    /* op>0 先建新库; 建不了 => fixed*=0, 但统计照填 */
    if(op==1||op==2){
        if(new_file_path && access(new_file_path,F_OK)!=0)
            c.nb=ndb2_init(new_file_path);
    }
    if(op==2 && c.nb){
        /* 强力修复: 不信拓扑, 但仍先跑一遍检测填 error/统计(不写入) */
        ndb2 save=c.nb; c.nb=0;
        fx_node(&c,0,1);
        c.nb=save;
        c.r->fixed_keys=0;
        fx_scan(&c);
    }else{
        /* op=0 检测; op=1 检测同时写入(c.nb 非空则写) */
        fx_node(&c,0,1);
    }
    if(c.nb){
        r->fixed=r->error - c.lossy;             /* B口径: 仅未致数据丢失的缺陷计入 */
        if(r->fixed<0)r->fixed=0;
        r->fixed_filelen=((ndb*)c.nb)->filelen;
        ndb2_free(c.nb);
    }
    if(c.role)free(c.role);
    return r;
}