#ifndef cppJSON__h
#define cppJSON__h
/*
Note: the code relies on the internal implementation of cJSON
// relies project version
#define CJSON_VERSION_MAJOR 1
#define CJSON_VERSION_MINOR 7
#define CJSON_VERSION_PATCH 19



*/
#include"cJSON.h"
#define cJSON_strdup strdup
#include<cstring>
#include<string>
void clear_a_item(cJSON*item){
    if(item==0)return;
    if (!(item->type & cJSON_IsReference) && (item->child != NULL)){
        cJSON_Delete(item->child);
        item->child=0;
    }
    if (!(item->type & cJSON_IsReference) && (item->valuestring != NULL)){
        cJSON_free(item->valuestring);
        item->valuestring=0;
    }
    item->type=0;
    item->valueint=0;
    item->valuedouble=0;
}
struct cppJSON{
    cJSON*a;
    bool isroot;
    cppJSON():a(0),isroot(0){}
    cppJSON(cJSON* x,bool isroot):a(x),isroot(isroot){}
    ~cppJSON(){
        if(isroot&&a)cJSON_Delete(a);
    }
    cppJSON(const cppJSON&)=delete;
    cppJSON& operator=(const cppJSON&)=delete;
    cppJSON(cppJSON&& other)noexcept:a(other.a),isroot(other.isroot) {
        other.a=0;
        other.isroot=0;
    }
    cppJSON& operator=(cppJSON&& other)noexcept {
        if(this!=&other) {
            if(isroot&&a)cJSON_Delete(a);
            a=other.a;
            isroot=other.isroot;
            other.a=0;
            other.isroot=0;
        }
        return *this;
    }
    cppJSON clone() const {
        cppJSON newObj;
        if(a) {
            newObj.a=cJSON_Duplicate(a,1);
            newObj.isroot=true;
        }
        return newObj;
    }
    cppJSON parse(const char* x) const {
        cppJSON newa(cJSON_Parse(x),1);
        return newa;
    }
    std::string stringify(cppJSON &a) const {
        if(a.a==0)return "null";
        char* jsonString = cJSON_Print(a.a);
        std::string result(jsonString);
        cJSON_free(jsonString);
        return result;
    }
    std::string stringify_Unformatted(cppJSON &a) const {
        if(a.a==0)return "null";
        char* jsonString = cJSON_PrintUnformatted(a.a);
        std::string result(jsonString);
        cJSON_free(jsonString);
        return result;
    }
    cppJSON operator[](const char* key) const {
        if (a && cJSON_IsObject(a))return cppJSON(cJSON_GetObjectItem(a, key), false);
        return cppJSON();
    }
    cppJSON operator[](string key) const {
        if (a && cJSON_IsObject(a))return cppJSON(cJSON_GetObjectItem(a, key.c_str()), false);
        return cppJSON();
    }
    cppJSON operator[](int id) const {
        if (!a || !cJSON_IsArray(a))return cppJSON();
        cJSON* item = a->child;
        for (int i = 0; item&& i < id; i++)item = item->next;
        return cppJSON(item, false);
    }
    cppJSON& operator=(const char* value) {
        if(!a)return *this;
        clear_a_item(a);
        a->type=cJSON_String;
        a->valuestring=(char*)cJSON_strdup(value);
        return *this;
    }
    bool operator==(const char* value)const {
        if(!a)return 0;
        if(!(a->type&cJSON_String))return 0;
        if(!a->valuestring||!value)return 0;
        return strcmp(value,a->valuestring)==0;
    }
    bool operator==(string& value)const {
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
    void insert(const char*name,const char*content){
        if (a&&cJSON_IsObject(a)) {
            cJSON_DeleteItemFromObject(a, name);
            cJSON_AddStringToObject(a, name, content);
        }
    }
    void erase(const char*x){
        if (a)cJSON_DeleteItemFromObject(a,x);
    }
    bool has(const char* key){
        return cJSON_GetObjectItem(a, key);
    }
    cppJSON child()const{
        return cppJSON(a?a->child:0,0);
    }
    cppJSON next()const{
        return cppJSON(a?a->next:0,0);
    }
    double valuedouble(){
        return a?a->valuedouble:0;
    }
    std::string valuestring(){
        return a&&a->valuestring?(std::string)a->valuestring:"";
    }
    bool operator!() const {
        return a==0;
    }
    void debug(){
        printf("--------%llx ",(long long)a);
        if(a->type&cJSON_Invalid)printf("cJSON_Invalid ");
        if(a->type&cJSON_False)printf("cJSON_False ");
        if(a->type&cJSON_True)printf("cJSON_True ");
        if(a->type&cJSON_NULL)printf("cJSON_NULL ");
        if(a->type&cJSON_Number)printf("cJSON_Number ");
        if(a->type&cJSON_String)printf("cJSON_String ");
        if(a->type&cJSON_Array)printf("cJSON_Array ");
        if(a->type&cJSON_Object)printf("cJSON_Object ");
        if(a->type&cJSON_Raw)printf("cJSON_Raw ");
        if(a->type&cJSON_IsReference)printf("cJSON_IsReference ");
        if(a->type&cJSON_StringIsConst)printf("cJSON_StringIsConst ");
        if(a->type>1023)printf("????type%d",a->type);
        printf("\n");
        printf("%llx %llx %llx\n%s:",(long long)a->child,(long long)a->prev,
        (long long)a->next,(a->string?a->string:"NULL"));
        if(a->type&cJSON_Number)printf("%lf",a->valuedouble);
        if(a->type&cJSON_String)printf("%s",(a->valuestring?a->valuestring:"NULL"));
        printf("\n");
    }
}JSON;

#endif