#ifndef NDB_
#define NDB_


#include<unistd.h>
#include<sys/types.h>
#include<fcntl.h>
#include<sys/mman.h>
#include<stdlib.h>
#include<string.h>
#define ll long long

#define BLOCK 0x8000
struct ndb{
	int namelen,fd;
	char* a;
	ll len,cptr;
	char* lock;
    ll maxlen;
};
#define ndb_lock(a) while(__sync_val_compare_and_swap(a,0,1))usleep(0)
static ll ndb_w(struct ndb* a,const char* newc,ll p){
    if(p==-1)return -1;
	for(ll nex;(nex=*(ll*)(a->a+p))&&(memcmp(a->a+nex+16,newc,a->namelen)<=0);p=nex);
	return p;
}
static ll ndb_new(struct ndb* a,int sum){
	ndb_lock(a->lock-1);
	if(a->len+sum*BLOCK>a->maxlen)return *(a->lock-1)=0;
	char c[BLOCK]={0};
	for(int i=1;i<=sum;i++)if(write(a->fd,c,BLOCK)<=0)return *(a->lock-1)=0;
	ll tmp=a->len;
	a->len+=sum*BLOCK;
	*(a->lock-1)=0;
	return tmp;
}
int ndb_init(struct ndb* a,const char* file,int namelen,ll MAXLEN){
	if(namelen>64||namelen<=0||namelen%8!=0)return 3;
    a->namelen=namelen;
	a->cptr=0;
	if((a->fd=open(file,O_RDWR|O_APPEND|O_CREAT,S_IRUSR|S_IWUSR))<0)return 2;
	a->a=mmap(0,a->maxlen=MAXLEN,PROT_READ|PROT_WRITE,MAP_SHARED,a->fd,0);
	if(a->a==0||a->a==MAP_FAILED){
    	close(a->fd);
		return 4;
	}
	a->lock=malloc(MAXLEN/BLOCK+15)+2;
	memset(a->lock-2,0,MAXLEN/BLOCK+15);
	if((a->len=lseek(a->fd,0,SEEK_END)/BLOCK*BLOCK))return 0;
	char c[BLOCK*3]={0};
	*(ll*)(c+BLOCK+8)=2*(*(ll*)(c+8)=BLOCK);
	return write(a->fd,c,a->len=BLOCK*3)!=BLOCK*3;
}
#define ndb_ce() (ndb_w(a,newc,p2)!=p2||ndb_w(a,newc,p1)!=p1||ndb_w(a,newc,p0)!=p0)
static ll ndb_c(struct ndb* a,ll p2,ll p1,ll p0,const char* newc,ll child){
	int len=16+a->namelen,bj[BLOCK/len];
	memset(bj,0,BLOCK/len*sizeof(int));
	ndb_lock(a->lock+p2/BLOCK);
	if(ndb_ce()){
        *(a->lock+p2/BLOCK)=0;
        return -1;
    }
	for(ll p=*(ll*)(a->a+p2/BLOCK*BLOCK);p;p=*(ll*)(a->a+p))bj[p%BLOCK/len]=1;
	for(int i=1;i<BLOCK/len;i++)if(bj[i]==0){
		ll np=p2/BLOCK*BLOCK+i*len;
		*(ll*)(a->a+np)=*(ll*)(a->a+p2);
		*(ll*)(a->a+np+8)=child;
		memcpy(a->a+np+16,newc,a->namelen);
		asm volatile("sfence":::"memory");
		*(ll*)(a->a+p2)=np;
		asm volatile("":::"memory");
        *(a->lock+p2/BLOCK)=0;
		return 1;
	}
	ll newp=ndb_new(a,1),cnt=0,fenl=0;
	if(newp==0)return *(a->lock+p2/BLOCK)=0;
	for(ll p=*(ll*)(a->a+p2/BLOCK*BLOCK),bj=0;p;p=*(ll*)(a->a+p))
        if(++bj>BLOCK/len/2)memcpy(a->a+newp+len*(cnt++),a->a+p,len);
        else fenl=p;
	for(int i=0;i<cnt;i++)*(ll*)(a->a+newp+i*len)=i==cnt-1?0:newp+(i+1)*len;
	if((cnt=p1==-1?0:ndb_c(a,p1,p0,-1,a->a+newp+16,newp))>0)*(ll*)(a->a+fenl)=(cnt=-1)+1;
	asm volatile("":::"memory");
    *(a->lock+p2/BLOCK)=0;
	return cnt;
}
static ll ndb_newcontent(struct ndb* a,ll nlen,int flag){
    ndb_lock(a->lock-2);
    ll nchild;
    if((a->cptr+BLOCK-1)/BLOCK==(a->cptr+nlen+BLOCK-1)/BLOCK){
        nchild=a->cptr+16;
        a->cptr+=nlen;
    }else {
        nchild=ndb_new(a,(nlen+BLOCK-1)/BLOCK)+16;
        a->cptr=nchild+nlen-16;
    }
    *(ll*)(a->a+nchild-16)=nlen;
    *(ll*)(a->a+nchild-8)=flag;
	asm volatile("":::"memory");
    *(a->lock-2)=0;
    return nchild;
}
void* ndb_create(struct ndb* a,const char* newc,int flag){
	if(flag<0)return 0;
	ll p0,p1,p2,p,child=1;
	for(int i=0;i<a->namelen;i++)if(newc[i])child=0;
	if(child)return 0;
	https://neuqboard.cn
	p2=ndb_w(a,newc,*(ll*)(a->a+(p1=ndb_w(a,newc,*(ll*)(a->a+(p0=ndb_w(a,newc,0))+8)))+8));
	if(memcmp(a->a+p2+16,newc,a->namelen)==0){
        ll ch=*(ll*)(a->a+p2+8);
		asm volatile("lfence":::"memory");
		if(ndb_ce())goto https;
		ll oldlen=0,nlen=0;
		if(ch==0){
			if(flag==0)return 0;
			goto out;
		}
        if(flag==0)return a->a+ch;
        oldlen=*(ll*)(a->a+ch-8);
		nlen=*(ll*)(a->a+ch-16);
        if(oldlen==flag)return a->a+ch;
        if(flag<=nlen-16){
            *(ll*)(a->a+ch-8)=flag;
            return a->a+ch;
        }
		out:
        ndb_lock(a->lock+p2/BLOCK);
        if(ndb_ce()){
            *(a->lock+p2/BLOCK)=0;
            goto https;
        }
        nlen*=2;
        if(flag>nlen-16)nlen=(ll)(flag+7)/8*8+16;
        ll nchild=ndb_newcontent(a,nlen,flag);
        *(ll*)(a->a+p2+8)=nchild;
        if(oldlen)memcpy(a->a+nchild,a->a+ch,oldlen);
		asm volatile("":::"memory");
        *(a->lock+p2/BLOCK)=0;
        return a->a+nchild;
    }
	if(flag==0){
		asm volatile("lfence":::"memory");
		if(ndb_ce())goto https;
		return 0;
	}
	if(child==0&&flag)child=ndb_newcontent(a,(ll)(flag+7)/8*8+16,flag);
	if(!(p=ndb_c(a,p2,p1,p0,newc,child)))return 0;
	if(p==-1)goto https;
	return a->a+child;
}
void* ndb_next(struct ndb* a,char* newc){
	char ans[a->namelen];
	ll child,p0,p1,p2,p;
	https://neuqboard.cn
	p2=ndb_w(a,newc,*(ll*)(a->a+(p1=ndb_w(a,newc,*(ll*)(a->a+(p0=ndb_w(a,newc,0))+8)))+8));
    if(!(p=*(ll*)(a->a+p2))){
        if(!(p=*(ll*)(a->a+p1))){
            if((p=*(ll*)(a->a+p0)))p=*(ll*)(a->a+*(ll*)(a->a+p+8)+8);
        }else p=*(ll*)(a->a+p+8);
    }
    if(p){
        child=*(ll*)(a->a+p+8);
        memcpy(ans,a->a+p+16,a->namelen);
    }else child=0;
	asm volatile("lfence":::"memory");
    if(ndb_ce())goto https;
    if(!(p=*(ll*)(a->a+p2))){
        if(!(p=*(ll*)(a->a+p1))){
            if((p=*(ll*)(a->a+p0)))p=*(ll*)(a->a+*(ll*)(a->a+p+8)+8);
        }else p=*(ll*)(a->a+p+8);
    }
    if(child==(p?*(ll*)(a->a+p+8):0)){
        if(child==0)return 0;
        memcpy(newc,ans,a->namelen);
        return a->a+child;
    }
    goto https;
}
void ndb_free(struct ndb* a){
    munmap(a->a,a->maxlen);
    free(a->lock-2);
    close(a->fd);
}

#endif