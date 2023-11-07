#pragma once
#include<stdio.h>

#ifndef LEPTJSON_H__
#define LEPTJSON_H__


//������������
typedef enum { LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT } lept_type;

//jsonֵ�ṹ��
typedef struct lept_value lept_value;
//object�ṹ��
typedef struct lept_member lept_member;

struct lept_value {
    union{
        struct { lept_member* m; size_t size, capacity; }o;  //object
        struct { lept_value* e; size_t size, capacity; }a;   //array(��̬���飬capacity��ʾ����)
        struct { char* s; size_t len; }s;  //�ַ���
        double n;
    }u;
	lept_type type;
};

struct lept_member {
    char* k; size_t klen;    //keyֵ�ַ����ͳ���
    lept_value v;            //ֵ
};

//���ٴ��ݲ����������ṹ
typedef struct {
    const char* json;
    char* stack;    //�½���ջ���������������
    size_t size, top;   //sizeΪ��ջ��С��topΪջ��λ��
}lept_context;

//��ʼ������ΪNULL
#define lept_init(v) do{ (v)->type = LEPT_NULL;} while(0)
#define lept_set_null(v) lept_init(v);

//����json
int lept_parse(lept_value* v, const char* json);

//��ȡ����
lept_type lept_get_type(const lept_value* v);
//��ȡboolֵ
int lept_get_boolean(const lept_value* v);
//����boolֵ
void lept_set_boolean(lept_value* v, int b);
//ȷ������Ϊ������
double lept_get_number(const lept_value* v); 
//��������
void lept_set_number(lept_value* v, double n);
//��ȡstring��������
const char* lept_get_string(const lept_value* v);
//��ȡstring�������ݳ���
size_t lept_get_string_length(const lept_value* v);
//��ȡarray��С
size_t lept_get_array_size(const lept_value* v);
//��ȡarray��Ԫ
lept_value* lept_get_array_element(const lept_value* v, size_t index);
//�������飬��������
void lept_set_array(lept_value* v, size_t capacity);
//��ȡ��������
size_t lept_get_array_capacity(const lept_value* v);
//������������
void lept_reserve_array(lept_value* v, size_t capacity);
//��С�����������������ݴ�С
void lept_shrink_array(lept_value* v);
//ĩ������
lept_value* lept_pushback_array_element(lept_value* v);
//��ȡobject��С
size_t lept_get_object_size(const lept_value* v);
//��ȡobject��keyֵ
const char* lept_get_object_key(const lept_value* v, size_t index);
//��ȡobject��key�ĳ���
size_t lept_get_object_key_length(const lept_value* v, size_t index);
//��ȡobject��value
lept_value* lept_get_object_value(const lept_value* v, size_t index);
//��ѯarray�еļ�ֵ(��������)
size_t lept_find_object_index(const lept_value* v, const char* key, size_t klen);
//��ѯarray�еļ�ֵ(����ֵ)
lept_value* lept_find_object_value(lept_value* v, const char* key, size_t klen);
//����Object����������
void lept_set_object(lept_value* v, size_t capacity);
//����object����
void lept_reserve_object(lept_value* v, size_t capacity);
//������object�����key������ָ��ָ���Ӧvalue
lept_value* lept_set_object_value(lept_value* v, const char* key, size_t klen);
//��̬�����ڴ�ռ䣬�����ַ���
void lept_set_string(lept_value* v, const char* s, size_t len);
//�������ͷ��ڴ�
void lept_free(lept_value* v);
//������
char* lept_stringify(const lept_value* v, size_t* length);
//�Ƚ��Ƿ����
int lept_is_equal(const lept_value* lhs, const lept_value* rhs);
//��ȸ���
void lept_copy(lept_value* dst, const lept_value* src);
//�ƶ�
void lept_move(lept_value* dst, lept_value* src);
//����
void lept_swap(lept_value* lhs, lept_value* rhs);

//��������ֵ���޴��󷵻� LEPT_PARSE_OK
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