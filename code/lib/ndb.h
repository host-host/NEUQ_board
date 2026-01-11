/**
 * ndb数据库使用方法
 * 不同于一般数据库，本数据库只会返回一个指针
 * 你在该指针地址下的读写会自动持久化
 */
#ifndef NDB_H
#define NDB_H

#ifdef __cplusplus
extern "C"{
#endif

struct ndb{
	int namelen,fd;
	char* a;
	long long len,cptr;
	char* lock;
    long long maxlen;
};
int ndb_init(struct ndb* a,const char* file,int namelen,long long MAXLEN);
/**
 * file表示数据库保存到哪个文件
 * namelen表示名字索引的长度，规定namelen为8的倍数且不大于64
 * MAXLEN表示文件最大长度，返回0表示正常初始化
 */
void* ndb_create(struct ndb* a,const char* name,int flag);
/**
 * flag=0表示不创建新节点
 * flag>0表示创建长度为flag的节点，或将原内容长度截断/扩充到flag
 * 返回指针，为0表示创建失败或者未找到
 * 注意：若已有内容且内容原长度不等于flag，可能会迁移数据地址导致之前该内容返回的地址指针失效
 */
void* ndb_next(struct ndb* a,char* name);
/**
 * 用该函数可以遍历数据库或者查找下一个节点，遍历数据库使用方法如下
 *  char tmp[namelen]={0};
 *  while(1){
 *      void* a=ndb_next(&user,tmp);
 *      if(a==0)break;
 *	    //do something
 *  }
 */
void ndb_free(struct ndb* a);
/**
 * 使用完成后用该函数清空
 */

#ifdef __cplusplus
}
#endif
#endif