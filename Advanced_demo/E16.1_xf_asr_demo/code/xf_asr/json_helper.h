#ifndef JSON_HELPER_H
#define JSON_HELPER_H

#include "zf_common_headfile.h"

// 简单的JSON对象结构
typedef struct {
    char buffer[4096];  // 存储JSON字符串
    int length;         // 当前长度
} json_object_t;

// 创建一个新的JSON对象
static inline void json_object_create(json_object_t* obj) {
    if (!obj) return;
    memset(obj->buffer, 0, sizeof(obj->buffer));
    strcpy(obj->buffer, "{}");
    obj->length = 2;
}

// 销毁JSON对象
static inline void json_object_destroy(json_object_t* obj) {
    // 简单实现，不需要额外清理
    if (!obj) return;
    memset(obj->buffer, 0, sizeof(obj->buffer));
    obj->length = 0;
}

// 设置字符串字段
static inline void json_object_set_string(json_object_t* obj, const char* key, const char* value) {
    if (!obj || !key || !value) return;
    
    char temp[4096] = {0};
    
    // 如果是空对象，直接添加第一个键值对
    if (strcmp(obj->buffer, "{}") == 0) {
        snprintf(temp, sizeof(temp), "{\"%s\":%s}", key, value);
    } else {
        // 移除结尾的 }
        obj->buffer[obj->length - 1] = '\0';
        // 添加新键值对
        snprintf(temp, sizeof(temp), "%s,\"%s\":%s}", obj->buffer, key, value);
    }
    
    // 复制回对象
    strncpy(obj->buffer, temp, sizeof(obj->buffer));
    obj->length = strlen(obj->buffer);
}

// 转换JSON对象到字符串
static inline void json_object_toString(json_object_t* obj, char* buffer) {
    if (!obj || !buffer) return;
    strcpy(buffer, obj->buffer);
}

#endif // JSON_HELPER_H 