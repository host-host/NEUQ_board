#ifndef CPPJSON_H
#define CPPJSON_H

#include"cJSON.h"
#include<cstring>
#include<string>
#include<utility>
#include<climits>
#include<cstdio>
#include<cstdlib>
inline static void clear_a_item(cJSON*item){
    if(item==0)return;
    if(!(item->type&cJSON_IsReference)){
        if(item->child)cJSON_Delete(item->child);
        if(item->valuestring)cJSON_free(item->valuestring);
    }
    item->child=0;
    item->valuestring=0;
    item->type&=cJSON_StringIsConst;
    item->valueint=0;
    item->valuedouble=0;
}
inline static char* my_cJSON_strdup(const char* str) {
    if(!str)return 0;
    size_t len=strlen(str)+1;
    char* copy=(char*)cJSON_malloc(len);
    if(copy)memcpy(copy,str,len);
    return copy;
}
inline static void move_a_item_content(cJSON* target,cJSON* source){
    clear_a_item(target);
    if(source){
        target->child=source->child;
        target->type|=source->type&~cJSON_StringIsConst;
        target->valuestring=source->valuestring;
        target->valueint=source->valueint;
        target->valuedouble=source->valuedouble;
        source->child=0;
        source->valuestring=0;
        cJSON_Delete(source);
    }else target->type|=cJSON_NULL;
}
struct cppJSON{
    cJSON*a;
    bool isroot;
    struct iterator{
        cJSON* item;
        cppJSON operator*()const{return cppJSON(item,0);}
        iterator& operator++(){
            if(item)item=item->next;
            return *this;
        }
        bool operator!=(const iterator& other)const{return item!=other.item;}
    };
    cppJSON():a(0),isroot(0){}
    cppJSON(cJSON* x,bool root):a(x),isroot(root){}
    cppJSON(const char* x):a(cJSON_Parse(x)),isroot(1){}
    cppJSON(const char* x,int len):a(x&&len>=0?cJSON_ParseWithLength(x,(size_t)len):0),isroot(1){}
    ~cppJSON(){
        if(isroot&&a)cJSON_Delete(a);
    }
    cppJSON(const cppJSON& other) {
        a=other.isroot?other.a?cJSON_Duplicate(other.a,1):0:other.a;
        isroot=other.isroot;
    }
    cppJSON& operator=(const cppJSON& other){
        if(this==&other)return *this;
        cJSON* replacement=other.isroot&&other.a?cJSON_Duplicate(other.a,1):other.a;
        if(other.isroot&&other.a&&!replacement)return *this;
        if(isroot&&a)cJSON_Delete(a);
        a=replacement;
        isroot=other.isroot;
        return *this;
    }
    cppJSON(cppJSON&& other) noexcept: a(other.a), isroot(other.isroot) {
        other.a = nullptr;
        other.isroot = false;
    }
    cppJSON& operator=(cppJSON&& other) noexcept {
        if(this==&other)return *this;
        if(isroot&&a)cJSON_Delete(a);
        a=other.a;
        isroot=other.isroot;
        other.a=0;
        other.isroot=0;
        return *this;
    }
    bool replace(const cppJSON& other){
        if(!a)return false;
        cJSON* replacement=other.a?cJSON_Duplicate(other.a,1):0;
        if(other.a&&!replacement)return false;
        move_a_item_content(a,replacement);
        return true;
    }
    cppJSON clone() const {
        cppJSON newObj;
        if(a) {
            newObj.a=cJSON_Duplicate(a,1);
            newObj.isroot=true;
        }
        return newObj;
    }
    std::string stringify() const {
        if(a==0)return "null";
        char* jsonString=cJSON_Print(a);
        std::string result(jsonString?jsonString:"");
        cJSON_free(jsonString);
        return result;
    }
    std::string stringify_Unformatted() const {
        if(a==0)return "null";
        char* jsonString=cJSON_PrintUnformatted(a);
        std::string result(jsonString?jsonString:"");
        cJSON_free(jsonString);
        return result;
    }
    cppJSON operator[](const char* key) const {
        if (a && cJSON_IsObject(a))return cppJSON(cJSON_GetObjectItemCaseSensitive(a, key), false);
        return cppJSON();
    }
    cppJSON operator[](std::string key) const {
        if (a && cJSON_IsObject(a))return cppJSON(cJSON_GetObjectItemCaseSensitive(a, key.c_str()), false);
        return cppJSON();
    }
    cppJSON operator[](int id) const {
        if (!a || !cJSON_IsArray(a))return cppJSON();
        return cppJSON(cJSON_GetArrayItem(a, id), false);
    }
    cppJSON& operator=(const char* value) {
        if(!a)return *this;
        clear_a_item(a);
        a->type|=cJSON_String;
        a->valuestring=(char*)my_cJSON_strdup(value);
        if(!a->valuestring)a->type^=cJSON_String|cJSON_NULL;
        return *this;
    }
    cppJSON& operator=(const std::string& value) {
        if(!a)return *this;
        clear_a_item(a);
        a->type|=cJSON_String;
        a->valuestring=(char*)my_cJSON_strdup(value.c_str());
        if(!a->valuestring)a->type^=cJSON_String|cJSON_NULL;
        return *this;
    }
    cppJSON& operator=(double value) {
        if(!a)return *this;
        clear_a_item(a);
        a->type|=cJSON_Number;
        a->valueint=value>INT_MAX?INT_MAX:value<INT_MIN?INT_MIN:(int)value;
        a->valuedouble=value;
        return *this;
    }
    cppJSON& operator=(bool value) {
        if(!a)return *this;
        clear_a_item(a);
        a->type|= value ? cJSON_True : cJSON_False;
        return *this;
    }
    bool operator==(const char* value)const {
        if(!a)return 0;
        if(!(a->type&cJSON_String))return 0;
        if(!a->valuestring||!value)return 0;
        return strcmp(value,a->valuestring)==0;
    }
    bool operator==(const std::string& value)const {
        if(!a)return 0;
        if(!(a->type&cJSON_String))return 0;
        if(!a->valuestring)return 0;
        return strcmp(value.c_str(),a->valuestring)==0;
    }
    bool operator==(bool value)const {
        if(!a)return 0;
        if(a->type&cJSON_False)return value==false;
        if(a->type&cJSON_True)return value==true;
        return 0;
    }
    bool operator==(const cppJSON& other)const {
        if(a==other.a)return true;
        return a&&other.a&&cJSON_Compare(a,other.a,true);
    }
    void insert(const char* name, const char* content) {
        if (a && cJSON_IsObject(a)) {
            cJSON_DeleteItemFromObjectCaseSensitive(a, name);
            if (content) cJSON_AddStringToObject(a, name, content);
            else cJSON_AddNullToObject(a, name);
        }
    }
    void insert(const char* name, const std::string& content) {
        if (a && cJSON_IsObject(a)) {
            cJSON_DeleteItemFromObjectCaseSensitive(a, name);
            cJSON_AddStringToObject(a, name, content.c_str());
        }
    }
    void insert(const char* name, double content) {
        if (a && cJSON_IsObject(a)) {
            cJSON_DeleteItemFromObjectCaseSensitive(a, name);
            cJSON_AddNumberToObject(a, name, content);
        }
    }
    void insert(const char* name, bool content) {
        if (a && cJSON_IsObject(a)) {
            cJSON_DeleteItemFromObjectCaseSensitive(a, name);
            cJSON_AddBoolToObject(a, name, content);
        }
    }
    void insert(const char* name, const cppJSON& childNode) {
        if (a && cJSON_IsObject(a)) {
            cJSON* item=cJSON_Duplicate(childNode.a, 1);
            cJSON_DeleteItemFromObjectCaseSensitive(a, name);
            if(childNode.a)cJSON_AddItemToObject(a, name, item);
            else cJSON_AddNullToObject(a,name);
        }
    }
    void insert(const char* name, cppJSON&& childNode) {
        if (a && cJSON_IsObject(a)) {
            cJSON* item=childNode.a?childNode.isroot?childNode.a:cJSON_Duplicate(childNode.a,1):0;
            cJSON_DeleteItemFromObjectCaseSensitive(a, name);
            if(item)cJSON_AddItemToObject(a,name,item);
            else cJSON_AddNullToObject(a,name);
            if(childNode.isroot)childNode.a=0;
        }
    }
    void push_back(const char* content) {
        if(a&&cJSON_IsArray(a)) {
            if (content) cJSON_AddItemToArray(a, cJSON_CreateString(content));
            else cJSON_AddItemToArray(a, cJSON_CreateNull());
        }
    }
    void push_back(const std::string& content) {
        if(a&&cJSON_IsArray(a))cJSON_AddItemToArray(a,cJSON_CreateString(content.c_str()));
    }
    void push_back(double content) {
        if(a&&cJSON_IsArray(a))cJSON_AddItemToArray(a, cJSON_CreateNumber(content));
    }
    void push_back(bool content) {
        if(a&&cJSON_IsArray(a))cJSON_AddItemToArray(a, cJSON_CreateBool(content));
    }
    void push_back(const cppJSON& childNode) {
        if(a&&cJSON_IsArray(a)){
            if(childNode.a)cJSON_AddItemToArray(a,cJSON_Duplicate(childNode.a,1));
            else cJSON_AddItemToArray(a,cJSON_CreateNull());
        }
    }
    void push_back(cppJSON&& childNode) {
        if(a&&cJSON_IsArray(a)){
            cJSON* item=childNode.a?childNode.isroot?childNode.a:cJSON_Duplicate(childNode.a,1):0;
            if(item)cJSON_AddItemToArray(a,item);
            else cJSON_AddItemToArray(a,cJSON_CreateNull());
            if(childNode.isroot)childNode.a=0;
        }
    }
    void erase(const char*x){
        if(a&&cJSON_IsObject(a))cJSON_DeleteItemFromObjectCaseSensitive(a,x);
    }
    void erase(int id){
        if(a&&cJSON_IsArray(a)&&id>=0)cJSON_DeleteItemFromArray(a,id);
    }
    bool has(const char* key) const {
        if(!a||!key)return 0;
        if(cJSON_IsObject(a))return cJSON_GetObjectItemCaseSensitive(a,key)!=0;
        if(cJSON_IsArray(a))for(cJSON* entry=a->child; entry; entry=entry->next)if(entry->valuestring&&strcmp(entry->valuestring,key)==0)return 1;
        return 0;
    }
    bool IsArray()const{
        return a&&cJSON_IsArray(a);
    }
    bool IsObject()const{
        return a&&cJSON_IsObject(a);
    }
    bool IsString()const{
        return a&&cJSON_IsString(a);
    }
    bool IsNumber()const{
        return a&&cJSON_IsNumber(a);
    }
    bool IsBool()const{
        return a&&cJSON_IsBool(a);
    }
    bool IsNull()const{
        return a&&cJSON_IsNull(a);
    }
    int size()const{
        return a&&(cJSON_IsArray(a)||cJSON_IsObject(a))?cJSON_GetArraySize(a):0;
    }
    iterator begin()const{return iterator{a?a->child:0};}
    iterator end()const{return iterator{0};}
    void clear(){
        if(a&&isroot)cJSON_Delete(a);
        a=0;
        isroot=0;
    }
    cppJSON child()const{
        return cppJSON(a?a->child:0,0);
    }
    cppJSON next()const{
        return cppJSON(a?a->next:0,0);
    }
    double valuedouble() const {
        return a?a->valuedouble:0;
    }
    std::string valuestring() const {
        return a&&a->valuestring?(std::string)a->valuestring:"";
    }
    operator std::string() const {
        return valuestring();
    }
    bool operator!() const {
        return a==0;
    }
    explicit operator bool() const noexcept {
        return a != nullptr;
    }
    void init_from_file(const char* file_path){
        if(a&&isroot)cJSON_Delete(a);
        isroot=1;
        a=0;
        FILE* file=fopen(file_path,"rb");
        if(!file)return;
        if(fseek(file,0,SEEK_END)!=0) {
            fclose(file);
            return;
        }
        long size=ftell(file);
        if(size<0||fseek(file,0,SEEK_SET)!=0) {
            fclose(file);
            return;
        }
        char* buffer=(char*)malloc((size_t)size+1);
        if(!buffer) {
            fclose(file);
            return;
        }
        size_t read_size=fread(buffer,1,(size_t)size,file);
        fclose(file);
        if (read_size != (size_t)size) {
            free(buffer);
            return;
        }
        buffer[size] = '\0';
        a=cJSON_Parse(buffer);
        free(buffer);
    }
    static cppJSON from_file(const char* path) {
        cppJSON result;
        result.init_from_file(path);
        return result;
    }
    // void debug(){
    //     printf("--------%llx ",(long long)a);
    //     if(a->type&cJSON_Invalid)printf("cJSON_Invalid ");
    //     if(a->type&cJSON_False)printf("cJSON_False ");
    //     if(a->type&cJSON_True)printf("cJSON_True ");
    //     if(a->type&cJSON_NULL)printf("cJSON_NULL ");
    //     if(a->type&cJSON_Number)printf("cJSON_Number ");
    //     if(a->type&cJSON_String)printf("cJSON_String ");
    //     if(a->type&cJSON_Array)printf("cJSON_Array ");
    //     if(a->type&cJSON_Object)printf("cJSON_Object ");
    //     if(a->type&cJSON_Raw)printf("cJSON_Raw ");
    //     if(a->type&cJSON_IsReference)printf("cJSON_IsReference ");
    //     if(a->type&cJSON_StringIsConst)printf("cJSON_StringIsConst ");
    //     if(a->type>1023)printf("????type%d",a->type);
    //     printf("\n");
    //     printf("%llx %llx %llx\n%s:",(long long)a->child,(long long)a->prev,
    //     (long long)a->next,(a->string?a->string:"NULL"));
    //     if(a->type&cJSON_Number)printf("%lf",a->valuedouble);
    //     if(a->type&cJSON_String)printf("%s",(a->valuestring?a->valuestring:"NULL"));
    //     printf("\n");
    // }
};

#endif
