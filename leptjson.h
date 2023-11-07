#pragma once
#include<stdio.h>

#ifndef LEPTJSON_H__
#define LEPTJSON_H__


//区分数据类型
typedef enum { LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT } lept_type;

//json值结构体
typedef struct lept_value lept_value;
//object结构体
typedef struct lept_member lept_member;

struct lept_value {
    union{
        struct { lept_member* m; size_t size, capacity; }o;  //object
        struct { lept_value* e; size_t size, capacity; }a;   //array(动态数组，capacity表示容量)
        struct { char* s; size_t len; }s;  //字符串
        double n;
    }u;
	lept_type type;
};

struct lept_member {
    char* k; size_t klen;    //key值字符串和长度
    lept_value v;            //值
};

//减少传递参数，二级结构
typedef struct {
    const char* json;
    char* stack;    //新建堆栈，解析结果缓冲区
    size_t size, top;   //size为堆栈大小，top为栈顶位置
}lept_context;

//初始化类型为NULL
#define lept_init(v) do{ (v)->type = LEPT_NULL;} while(0)
#define lept_set_null(v) lept_init(v);

//解析json
int lept_parse(lept_value* v, const char* json);

//获取类型
lept_type lept_get_type(const lept_value* v);
//获取bool值
int lept_get_boolean(const lept_value* v);
//设置bool值
void lept_set_boolean(lept_value* v, int b);
//确保类型为数字型
double lept_get_number(const lept_value* v); 
//设置数字
void lept_set_number(lept_value* v, double n);
//获取string类型数据
const char* lept_get_string(const lept_value* v);
//获取string类型数据长度
size_t lept_get_string_length(const lept_value* v);
//获取array大小
size_t lept_get_array_size(const lept_value* v);
//获取array的元
lept_value* lept_get_array_element(const lept_value* v, size_t index);
//设置数组，赋予容量
void lept_set_array(lept_value* v, size_t capacity);
//获取数组容量
size_t lept_get_array_capacity(const lept_value* v);
//扩大数组容量
void lept_reserve_array(lept_value* v, size_t capacity);
//缩小数字容量到现有数据大小
void lept_shrink_array(lept_value* v);
//末端扩容
lept_value* lept_pushback_array_element(lept_value* v);
//获取object大小
size_t lept_get_object_size(const lept_value* v);
//获取object的key值
const char* lept_get_object_key(const lept_value* v, size_t index);
//获取object的key的长度
size_t lept_get_object_key_length(const lept_value* v, size_t index);
//获取object的value
lept_value* lept_get_object_value(const lept_value* v, size_t index);
//查询array中的键值(返回索引)
size_t lept_find_object_index(const lept_value* v, const char* key, size_t klen);
//查询array中的键值(返回值)
lept_value* lept_find_object_value(lept_value* v, const char* key, size_t klen);
//设置Object，赋予容量
void lept_set_object(lept_value* v, size_t capacity);
//扩大object容量
void lept_reserve_object(lept_value* v, size_t capacity);
//设置新object，添加key，返回指针指向对应value
lept_value* lept_set_object_value(lept_value* v, const char* key, size_t klen);
//动态分配内存空间，复制字符串
void lept_set_string(lept_value* v, const char* s, size_t len);
//按类型释放内存
void lept_free(lept_value* v);
//生成器
char* lept_stringify(const lept_value* v, size_t* length);
//比较是否相等
int lept_is_equal(const lept_value* lhs, const lept_value* rhs);
//深度复制
void lept_copy(lept_value* dst, const lept_value* src);
//移动
void lept_move(lept_value* dst, lept_value* src);
//交换
void lept_swap(lept_value* lhs, lept_value* rhs);

//解析返回值，无错误返回 LEPT_PARSE_OK
enum {
    LEPT_PARSE_OK = 0,
    LEPT_PARSE_EXPECT_VALUE,
    LEPT_PARSE_INVALID_VALUE,
    LEPT_PARSE_ROOT_NOT_SINGULAR,
    LEPT_PARSE_NUMBER_TOO_BIG,
    LEPT_PARSE_MISS_QUOTATION_MARK,
    LEPT_PARSE_INVALID_STRING_ESCAPE,
    LEPT_PARSE_INVALID_STRING_CHAR,
    LEPT_PARSE_INVALID_UNICODE_HEX,
    LEPT_PARSE_INVALID_UNICODE_SURROGATE,
    LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET,
    LEPT_PARSE_MISS_KEY,
    LEPT_PARSE_MISS_COLON,
    LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET,
    LEPT_STRINGIFY_OK,
    LEPT_KEY_NOT_EXIST
};





#endif // !LEPTJSON_H__