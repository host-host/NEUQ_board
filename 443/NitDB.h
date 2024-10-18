#ifndef __NitDB
#define __NitDB
#ifdef __cplusplus
extern"C"{
#endif

struct NitDB{
	int namelen,fd;
	void* a;
	long long len,cptr;
	char* lock;
    long long maxlen;
};
int NitDB_init(struct NitDB* a,const char* file,int namelen,long long MAXLEN);
void* NitDB_create(struct NitDB* a,const void* b,int flag);
void* NitDB_next(struct NitDB* a,void* b);
void NitDB_free(struct NitDB* a);


#ifdef __cplusplus
}
#endif
#endif