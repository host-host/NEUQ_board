/**
 * ndb数据库为key-value数据库，支持不同key的并发读写，以及相同key的并发读
 * key为\0结尾的非空字符串，不超过47B
 * value不超过100MB
 * 数据库总大小不超过128GB
 */
#ifndef NDB2_H
#define NDB2_H

#ifdef __cplusplus
extern "C"{
#endif

typedef void* ndb2;
ndb2 ndb2_init(const char* file);

/**
 * valuelen==0表示不创建新节点
 * valuelen>0表示创建长度为valuelen的节点，支持扩容已有节点
 * 返回指针，为0表示创建失败或者未找到
 */
void* ndb2_got(ndb2 a,const char* key,int valuelen);
#define ndb2_gotmaxlen(p) (*(((long long*)(p))-1))

/**
 * 用该函数可以遍历数据库或者查找下一个节点，遍历数据库使用方法如下
 *  char key[48]={0};
 *  for(void*p=ndb2_next(a,key);p;p=ndb2_next(a,key)){
 *	    //do something
 *  }
 * key必须是至少 48 字节的可写缓冲区
 */
void* ndb2_next(ndb2 a,char* key);

typedef struct{
    long long keys;//key总数
    long long keys_max_len;//key的最大长度
    long long filelen;//文件大小
    long long value_max_len;//value的最大长度
    long long error;//错误总数量
    long long fixed;//已修复的错误总数量
    long long fixed_filelen;//修复后新库的文件大小
    long long fixed_keys;//修复后新库的key总数
}ndb2_fix_return;
ndb2_fix_return* ndb2_fix(ndb2 a,int op,const char*new_file_path);//修复数据库，op=[0/1/2]表示[不修复/普通修复/强力修复]
void ndb2_free(ndb2 a);
#ifdef __cplusplus
}
#endif
#endif