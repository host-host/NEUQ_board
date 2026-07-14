#ifndef cppJSON__h
#define cppJSON__h


#include"cJSON.h"
#include<cstring>
#include<string>
inline void clear_a_item(cJSON*item){
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
inline char* safe_cJSON_strdup(const char* str) {
    if (str == nullptr) return nullptr;
    size_t len = strlen(str) + 1;
    char* copy = (char*)cJSON_malloc(len);
    if (copy != nullptr)memcpy(copy, str, len);
    return copy;
}
struct cppJSON{
    cJSON*a;
    bool isroot;
    cppJSON():a(0),isroot(0){}
    cppJSON(cJSON* x,bool isroot):a(x),isroot(isroot){}
    cppJSON(const char* x):a(cJSON_Parse(x)),isroot(1){}
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
    std::string stringify() const {
        if(a==0)return "null";
        char* jsonString = cJSON_Print(a);
        std::string result(jsonString);
        cJSON_free(jsonString);
        return result;
    }
    std::string stringify_Unformatted() const {
        if(a==0)return "null";
        char* jsonString = cJSON_PrintUnformatted(a);
        std::string result(jsonString);
        cJSON_free(jsonString);
        return result;
    }
    cppJSON operator[](const char* key) const {
        if (a && cJSON_IsObject(a))return cppJSON(cJSON_GetObjectItem(a, key), false);
        return cppJSON();
    }
    cppJSON operator[](std::string key) const {
        if (a && cJSON_IsObject(a))return cppJSON(cJSON_GetObjectItem(a, key.c_str()), false);
        return cppJSON();
    }
    cppJSON operator[](int id) const {
        if (!a || !cJSON_IsArray(a))return cppJSON();
        return cppJSON(cJSON_GetArrayItem(a, id), false);
    }
    cppJSON& operator=(const char* value) {
        if(!a)return *this;
        clear_a_item(a);
        a->type=cJSON_String;
        a->valuestring=(char*)safe_cJSON_strdup(value);
        return *this;
    }
    cppJSON& operator=(const std::string& value) {
        if(!a)return *this;
        clear_a_item(a);
        a->type=cJSON_String;
        a->valuestring=(char*)safe_cJSON_strdup(value.c_str());
        return *this;
    }
    cppJSON& operator=(double value) {
        if(!a)return *this;
        clear_a_item(a);
        a->type=cJSON_Number;
        a->valueint = (int)value;
        a->valuedouble = value;
        return *this;
    }
    cppJSON& operator=(bool value) {
        if(!a)return *this;
        clear_a_item(a);
        a->type = value ? cJSON_True : cJSON_False;
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
    void insert(const char* name, const char* content) {
        if (a && cJSON_IsObject(a)) {
            cJSON_DeleteItemFromObject(a, name);
            if (content) cJSON_AddStringToObject(a, name, content);
            else cJSON_AddNullToObject(a, name);
        }
    }
    void insert(const char* name, const std::string& content) {
        if (a && cJSON_IsObject(a)) {
            cJSON_DeleteItemFromObject(a, name);
            cJSON_AddStringToObject(a, name, content.c_str());
        }
    }
    void insert(const char* name, double content) {
        if (a && cJSON_IsObject(a)) {
            cJSON_DeleteItemFromObject(a, name);
            cJSON_AddNumberToObject(a, name, content);
        }
    }
    void insert(const char* name, bool content) {
        if (a && cJSON_IsObject(a)) {
            cJSON_DeleteItemFromObject(a, name);
            cJSON_AddBoolToObject(a, name, content);
        }
    }
    void insert(const char* name, const cppJSON& childNode) {
        if (a && cJSON_IsObject(a) && childNode.a) {
            cJSON_DeleteItemFromObject(a, name);
            cJSON_AddItemToObject(a, name, cJSON_Duplicate(childNode.a, 1));
        }
    }
    void insert(const char* name, cppJSON&& childNode) {
        if (a && cJSON_IsObject(a) && childNode.a) {
            cJSON_DeleteItemFromObject(a, name);
            cJSON_AddItemToObject(a, name, childNode.a);
            childNode.isroot = false; 
            childNode.a = 0;
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
        if(a&&cJSON_IsArray(a)&&childNode.a)cJSON_AddItemToArray(a, cJSON_Duplicate(childNode.a, 1));
    }
    void push_back(cppJSON&& childNode) {
        if (a && cJSON_IsArray(a) && childNode.a) {
            cJSON_AddItemToArray(a, childNode.a);
            childNode.isroot = false; 
            childNode.a = 0;
        }
    }
    void erase(const char*x){
        if (a)cJSON_DeleteItemFromObject(a,x);
    }
    bool has(const char* key) const {
        if(!a)return 0;
        return cJSON_GetObjectItem(a, key)!=0;
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
