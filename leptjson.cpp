#define _CRT_SECURE_NO_WARNINGS

#define _WINDOWS
#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif  //����ڴ�й©

#include "leptjson.h"
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#ifndef LEPT_PARSE_STRINGIFY_INIT_SIZE
#define LEPT_PARSE_STRINGIFY_INIT_SIZE 256
#endif

//ȷ�ϵ�һ���ַ�Ϊ�ص����͵Ŀ�ͷ�ַ���ָ������ƫ��һλ
#define EXPECT(c, ch)       do{ assert(*c->json==(ch)); c->json++;}while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')   //�Ƿ�Ϊ����
#define ISDIGIT1TO9(ch)     ((ch) >= '0' && (ch) <= '9')   //�Ƿ�Ϊ��0����
//�������ַ�ѹ���ջ
#define PUTC(c, ch)         do{ *(char*)lept_context_push(c, sizeof(char)) = (ch);}while(0)
#define PUTS(c, s, len)     memcpy(lept_context_push(c, len), s, len)
#define STRING_ERROR(ret)   do{ c->top = head; return ret; }while(0)//���ش���

//��������
static int lept_parse_value(lept_context * c, lept_value * v);   //���ò�ͬ���ͽ�����������ֵ

static void lept_parse_whitespace(lept_context * c);             //�����ո�
//�����NULL,TRUE,FALSE
static int lept_parse_literal(lept_context * c, lept_value * v, const char* literal, lept_type type);
static int lept_parse_number(lept_context * c, lept_value * v);  //�����������
static int lept_parse_string(lept_context * c, lept_value * v);  //������ַ�����(д��)
static int lept_parse_string_raw(lept_context * c, char** str, size_t * len); //������ַ�����(����)
static int lept_parse_array(lept_context * c, lept_value * v);   //�����������
static int lept_parse_object(lept_context * c, lept_value * v);  //�����object��
static const char* lept_parse_hex4(const char* p, unsigned* u);  //����4λ16�������֣��洢Ϊ���u
static void lept_encode_utf8(lept_context * c, unsigned u);      //��������ΪUTF-8
//static int lept_parse_null(lept_context * c, lept_value * v);    //�����NULL����
//static int lept_parse_true(lept_context * c, lept_value * v);	 //�����TRUE����
//static int lept_parse_false(lept_context * c, lept_value * v);   //�����FALSE����
static void* lept_context_push(lept_context * c, size_t size);   //��ջѹ��
static void* lept_context_pop(lept_context * c, size_t size);    //��ջ����

static int lept_stringify_value(lept_context * c, const lept_value * v);//����
static void lept_stringify_string(lept_context * c, const char* s, size_t len);//�����ַ���


//������
char* lept_stringify(const lept_value * v, size_t * length) {
	lept_context c;
	assert(v != NULL);
	c.stack = (char*)malloc(c.size = LEPT_PARSE_STRINGIFY_INIT_SIZE);
	c.top = 0;
	lept_stringify_value(&c, v);
	if (length)
		*length = c.top;
	PUTC(&c, '\0');
	return c.stack;
}
/*int lept_stringify(const lept_value * v, char** json, size_t * length) {
	lept_context c;
	int ret;
	assert(v != NULL);
	assert(json != NULL);
	c.stack = (char*)malloc(c.size = LEPT_PARSE_STRINGIFY_INIT_SIZE);
	c.top = 0;
	if ((ret = lept_stringify_value(&c, v)) != LEPT_STRINGIFY_OK) {
		free(c.stack);
		*json = NULL;
		return ret;
	}
	if (length)
		*length = c.top;
	PUTC(&c, '\0');
	*json = c.stack;
	return LEPT_STRINGIFY_OK;
} */
static int lept_stringify_value(lept_context* c, const lept_value* v) {
	size_t i;
	switch (v->type){
		case LEPT_NULL: PUTS(c, "null", 4); break;
		case LEPT_FALSE: PUTS(c, "false", 5); break;
		case LEPT_TRUE: PUTS(c, "true", 4); break;
		case LEPT_NUMBER: 
			c->top -= 32 - sprintf((char*)lept_context_push(c, 32), "%.17g", v->u.n);
			break;
		case LEPT_STRING: lept_stringify_string(c, v->u.s.s, v->u.s.len); break;
		case LEPT_ARRAY:
			PUTC(c, '[');
			for (i = 0; i < v->u.a.size; i++) {
				if (i > 0)
					PUTC(c, ',');
				lept_stringify_value(c, &v->u.a.e[i]);
			}
			PUTC(c, ']');
			break;
		case LEPT_OBJECT:
			PUTC(c, '{');
			for (i = 0; i < v->u.o.size; i++) {
				if (i > 0)
					PUTC(c, ',');
				lept_stringify_string(c, v->u.o.m[i].k, v->u.o.m[i].klen);
				PUTC(c, ':');
				lept_stringify_value(c, &v->u.o.m[i].v);
			}
			PUTC(c, '}');
			break;
	}
	return LEPT_STRINGIFY_OK;
}
//�����ַ���
static void lept_stringify_string(lept_context* c, const char* s, size_t len) {
	static const char hex_digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
	size_t i, size;
	char* head, * p;
	assert(s != NULL);
	p = head = (char*)lept_context_push(c, size = len * 6 + 2);  //ÿ���ַ�6λ��2��˫����
	*p++ = '"';
	for (i = 0; i < len; i++) {
		unsigned char ch = (unsigned char)s[i];
		switch (ch)
		{
		case '\"': *p++ = '\\'; *p++ = '\"'; break;
		case '\\': *p++ = '\\'; *p++ = '\\'; break;
		case '\b': *p++ = '\\'; *p++ = 'b';  break;
		case '\f': *p++ = '\\'; *p++ = 'f';  break;
		case '\n': *p++ = '\\'; *p++ = 'n';  break;
		case '\r': *p++ = '\\'; *p++ = 'r';  break;
		case '\t': *p++ = '\\'; *p++ = 't';  break;
		default:
			if (ch < 0x20) {
				*p++ = '\\'; *p++ = 'u'; *p++ = '0'; *p++ = '0';
				*p++ = hex_digits[ch >> 4];
				*p++ = hex_digits[ch & 15];
			}
			else
				*p++ = s[i];
		}
	}
	*p++ = '"';
	c->top -= size - (p - head);
}
/*static void lept_stringify_string(lept_context* c, const char* s, size_t len) {
	size_t i;
	assert(s != NULL);
	PUTC(c, '"');
	for (i = 0; i < len; i++) {
		unsigned char ch = (unsigned char)s[i];
		switch (ch)
		{
		case '\"': PUTS(c, "\\\"", 2); break;
		case '\\': PUTS(c, "\\\\", 2); break;
		case '\b': PUTS(c, "\\b", 2); break;
		case '\f': PUTS(c, "\\f", 2); break;
		case '\n': PUTS(c, "\\n", 2); break;
		case '\r': PUTS(c, "\\r", 2); break;
		case '\t': PUTS(c, "\\t", 2); break;
		default:
			if (ch < 0x20) {
				char buffer[7];
				sprintf(buffer, "\\u%04X", ch);
				PUTS(c, buffer, 6);
			}
			else
				PUTC(c, s[i]);
		}
	}
	PUTC(c, '"');
}*/


//������
int lept_parse(lept_value * v, const char* json) {
	lept_context c;
	int ret;
	assert(v != NULL);
	c.json = json;
	c.stack = NULL;
	c.size = c.top = 0;
	lept_init(v);
	lept_parse_whitespace(&c);
	if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
		lept_parse_whitespace(&c);
		if (*c.json != '\0')
			ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
	}
	assert(c.top == 0);
	free(c.stack);
	return ret;
}

//���ò�ͬ���ͽ�����������ֵ
static int lept_parse_value(lept_context * c, lept_value * v) {
	switch (*c->json)
	{
	//case 'n': return lept_parse_null(c, v);
	//case 't': return lept_parse_true(c, v);
	//case 'f': return lept_parse_false(c, v);
	case 'n': return lept_parse_literal(c, v, "null", LEPT_NULL);
	case 't': return lept_parse_literal(c, v, "true", LEPT_TRUE);
	case 'f': return lept_parse_literal(c, v, "false", LEPT_FALSE);
	case '\0': return LEPT_PARSE_EXPECT_VALUE;
	case '"': return lept_parse_string(c, v);
	case '[': return lept_parse_array(c, v);
	case '{': return lept_parse_object(c, v);
	default: return lept_parse_number(c, v);
	}
}

//�����ո�
static void lept_parse_whitespace(lept_context* c) {
	const char* p = c->json;
	while (*p==' '||*p=='\t'||*p=='\n'||*p=='\r')  p++;
	c->json = p;
}

//�����NULL,TRUE,FALSE
static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
	size_t i;
	EXPECT(c, literal[0]);
	for ( i = 0; literal[i+1]; i++)
		if (c->json[i] != literal[i+1])  return LEPT_PARSE_INVALID_VALUE;
	c->json += i;
	v->type = type;
	return LEPT_PARSE_OK;
}

////�����NULL����
//static int lept_parse_null(lept_context* c, lept_value* v) {
//	EXPECT(c, 'n');
//	if (c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')
//		return LEPT_PARSE_INVALID_VALUE;
//	c->json += 3;
//	v->type = LEPT_NULL;
//	return LEPT_PARSE_OK;
//}
//
////�����TRUE����
//static int lept_parse_true(lept_context* c, lept_value* v) {
//	EXPECT(c, 't');
//	if (c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e')
//		return LEPT_PARSE_INVALID_VALUE;
//	c->json += 3;
//	v->type = LEPT_TRUE;
//	return LEPT_PARSE_OK;
//}
//
////�����FALSE����
//static int lept_parse_false(lept_context* c, lept_value* v) {
//	EXPECT(c, 'f');
//	if (c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's' || c->json[3] != 'e')
//		return LEPT_PARSE_INVALID_VALUE;
//	c->json += 4;
//	v->type = LEPT_FALSE;
//	return LEPT_PARSE_OK;
//}

//�����������
//static int lept_parse_number(lept_context* c, lept_value* v) {
//	char* end;
//	v->n = strtod(c->json, &end);   //ʮ����ת�����ƣ�endָ���β��һλ��ַ
//	if (c->json == end)
//		return LEPT_PARSE_INVALID_VALUE;
//	c->json = end;    //c.jsonָ����һ��
//	v->type = LEPT_NUMBER;
//	return LEPT_PARSE_OK;
//}
//�����������
static int lept_parse_number(lept_context* c, lept_value* v) {
	const char* p = c->json;
	if (*p == '-') p++;  //����
	if (*p == '0') p++;  
	else {
		if(!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;
		for (p++; ISDIGIT(*p); p++); //����
	}
	if (*p == '.') { //С��
		p++;
		if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
		for (p++; ISDIGIT(*p); p++);
	}
	if (*p == 'E' || *p == 'e') {
		p++;
		if (*p == '+' || *p == '-') p++;
		if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
		for (p++; ISDIGIT(*p); p++);
	}
	errno = 0;
	v->u.n = strtod(c->json, NULL);
	if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
		return LEPT_PARSE_NUMBER_TOO_BIG;
	c->json = p;
	v->type = LEPT_NUMBER;
	return LEPT_PARSE_OK;
}

//������ַ�����
static int lept_parse_string(lept_context* c, lept_value* v) {
	int ret;
	char* s;
	size_t len;
	if ((ret = lept_parse_string_raw(c, &s, &len)) == LEPT_PARSE_OK)
		lept_set_string(v, s, len);
	return ret;
}
static int lept_parse_string_raw(lept_context* c, char** str, size_t* len) {
	unsigned u, u2;
	size_t head = c->top;
	const char* p;
	EXPECT(c, '\"');
	p = c->json;
	for (;;) {
		char ch = *p++;
		switch (ch) {
		case '\"':
			*len = c->top - head;
			*str = (char* ) lept_context_pop(c, *len);
			c->json = p;
			return LEPT_PARSE_OK;
		case '\\':    //����ת������
			switch (*p++) {
			case '\"': PUTC(c, '\"'); break;
			case '\\': PUTC(c, '\\'); break;
			case '/':  PUTC(c, '/'); break;
			case 'b':  PUTC(c, '\b'); break;
			case 'f':  PUTC(c, '\f'); break;
			case 'n':  PUTC(c, '\n'); break;
			case 'r':  PUTC(c, '\r'); break;
			case 't':  PUTC(c, '\t'); break;
			case 'u':
				if (!(p = lept_parse_hex4(p, &u)))
					STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX); //���Ϸ���16��λ��
				if (u >= 0xD800 && u <= 0xDBFF) { //�Ǹߴ�����
					if (*p++ != '\\')
						STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
					if (*p++ != 'u')
						STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE); //ȱ�ٵʹ�����
					if (!(p = lept_parse_hex4(p, &u2)))
						STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX); //�ʹ�����Ϸ�
					if (u2 < 0xDC00 || u2 > 0xDFFF)
						STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
					u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
				}
				lept_encode_utf8(c, u);
				break;
			default:
				//c->top = head;
				STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
			}
			break;
		case '\0':
			STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
		default:
			//���Ϸ��ַ���Χ��˫���ŷ�б��(�Ѵ���)��%x00��%x1F
			if ((unsigned char)ch < 0x20) {
				STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
			}
			PUTC(c, ch);  //ָ��δ����βʱ��ѹ������
		}
	}
}
/* static int lept_parse_string(lept_context* c, lept_value* v) {
	unsigned u, u2;
	size_t head = c->top, len;
	const char* p;
	EXPECT(c, '\"');
	p = c->json;
	for (;;) {
		char ch = *p++;
		switch (ch) {
		case '\"':
			len = c->top - head;
			lept_set_string(v, (const char*)lept_context_pop(c, len), len); //ָ��ָ���βʱ��������ջ��ͬʱ��������д��v
			c->json = p;
			return LEPT_PARSE_OK;
		case '\0':
			c->top = head;
			return LEPT_PARSE_MISS_QUOTATION_MARK;
		case '\\':    //����ת������
			switch (*p++) {  
			case '\"': PUTC(c, '\"'); break;
			case '\\': PUTC(c, '\\'); break;
			case '/':  PUTC(c, '/'); break;
			case 'b':  PUTC(c, '\b'); break;
			case 'f':  PUTC(c, '\f'); break;
			case 'n':  PUTC(c, '\n'); break;
			case 'r':  PUTC(c, '\r'); break;
			case 't':  PUTC(c, '\t'); break;
			case 'u':  
				if (!(p = lept_parse_hex4(p, &u)))
					STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX); //���Ϸ���16��λ��
				if (u >= 0xD800 && u <= 0xDBFF) { //�Ǹߴ�����
					if (*p++ != '\\')
						STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
					if (*p++ != 'u')
						STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE); //ȱ�ٵʹ�����
					if (!(p = lept_parse_hex4(p, &u2)))
						STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX); //�ʹ�����Ϸ�
					if (u2 < 0xDC00 || u2 > 0xDFFF)
						STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
					u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
				}
				lept_encode_utf8(c, u);
				break;
			default:
				c->top = head;
				return LEPT_PARSE_INVALID_STRING_ESCAPE;
			}
			break;
		default:
			//���Ϸ��ַ���Χ��˫���ŷ�б��(�Ѵ���)��%x00��%x1F
			if ((unsigned char)ch < 0x20) {    
				c->top = head;
				return LEPT_PARSE_INVALID_STRING_CHAR;
			}
			PUTC(c, ch);  //ָ��δ����βʱ��ѹ������
		}
	}
} */
//��ջѹ��
static void* lept_context_push(lept_context* c, size_t size) {
	void* ret;
	assert(size > 0);
	if (c->top + size >= c->size) {
		if (c->size == 0)
			c->size = LEPT_PARSE_STACK_INIT_SIZE;
		while (c->top + size >= c->size)
			c->size += c->size >> 1;  //�ռ䲻��ʱ����Ϊ1.5��
		c->stack = (char*)realloc(c->stack, c->size);
	}
	ret = c->stack + c->top;
	c->top += size;
	return ret;  //���ص�ָ��ָ����ѹ��Ķ�ջ�ĳ�ʼλ��
}
//��ջ����
static void* lept_context_pop(lept_context* c, size_t size) {
	assert(c->top >= size);
	return c->stack + (c->top -= size);
}
//����4λ16�������֣��洢Ϊ���u
static const char* lept_parse_hex4(const char* p, unsigned* u) {
	int i;
	*u = 0;
	for (i = 0; i < 4; i++) {
		char ch = *p++;
		*u <<= 4;
		if (ch >= '0' && ch <= '9') *u |= ch - '0';
		else if (ch >= 'A' && ch <= 'F') *u |= ch - 'A' + 10;
		else if (ch >= 'a' && ch <= 'f') *u |= ch - 'a' + 10;
		else return NULL;
	}
	return p;
}
//��������ΪUTF-8
static void lept_encode_utf8(lept_context* c, unsigned u) {
	if (u <= 0x7F)
		PUTC(c, u & 0xFF);   //����0xFF��ֹ��������
	else if (u <= 0x7FF) {
		PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
		PUTC(c, 0x80 | ( u       & 0x3F));
	}
	else if (u <= 0xFFFF) {
		PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
		PUTC(c, 0x80 | ((u >> 6)  & 0x3F));
		PUTC(c, 0x80 | ( u        & 0x3F));
	}
	else {
		assert(u <= 0x10FFFF);
		PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
		PUTC(c, 0x80 | ((u >> 12) & 0x3F));
		PUTC(c, 0x80 | ((u >> 6)  & 0x3F));
		PUTC(c, 0x80 | ( u        & 0x3F));
	}
}

//�����array��
static int lept_parse_array(lept_context* c, lept_value* v) {
	size_t i,size = 0;
	int ret;
	EXPECT(c, '[');
	lept_parse_whitespace(c);
	if (*c->json == ']') {
		c->json++;
		v->type = LEPT_ARRAY;
		v->u.a.size = 0;
		v->u.a.e = NULL;
		return LEPT_PARSE_OK;
	}
	for (;;) {
		lept_value e;
		lept_init(&e);
		if((ret=lept_parse_value(c,&e))!=LEPT_PARSE_OK)
			break;
		memcpy(lept_context_push(c, sizeof(lept_value)), &e, sizeof(lept_value));
		size++;
		lept_parse_whitespace(c);
		if (*c->json == ',') {
			c->json++;
			lept_parse_whitespace(c);
		}
		else if (*c->json == ']')
		{
			c->json++;
			v->type = LEPT_ARRAY;
			v->u.a.size = size;
			size *= sizeof(lept_value);
			memcpy(v->u.a.e = (lept_value*)malloc(size), lept_context_pop(c, size), size);
			return LEPT_PARSE_OK;
		}
		else {
			ret = LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
			break;
		}	
	}
	for ( i = 0; i < size; i++)
		lept_free((lept_value*)lept_context_pop(c, sizeof(lept_value)));
	return ret;
}

//�����object��
static int lept_parse_object(lept_context* c, lept_value* v) {
	size_t size, i;
	lept_member m;
	int ret;
	EXPECT(c, '{');
	lept_parse_whitespace(c);
	if (*c->json == '}') {
		c->json++;
		v->type = LEPT_OBJECT;
		v->u.o.m = 0;
		v->u.o.size = 0;
		return LEPT_PARSE_OK;
	}
	m.k = NULL;
	size = 0;
	for (;;) {
		char* str;
		lept_init(&m.v);
		if (*c->json != '"') {
			ret = LEPT_PARSE_MISS_KEY;
			break;
		}
		if ((ret = lept_parse_string_raw(c, &str, &m.klen)) != LEPT_PARSE_OK)   //key
			break;
		memcpy(m.k = (char*)malloc(m.klen + 1), str, m.klen);
		m.k[m.klen]='\0';
		lept_parse_whitespace(c);
		if (*c->json != ':') {          //ð��
			ret = LEPT_PARSE_MISS_COLON;
			break;
		}
		c->json++;
		lept_parse_whitespace(c);
		if ((ret = lept_parse_value(c, &m.v)) != LEPT_PARSE_OK)  //value
			break;
		memcpy(lept_context_push(c, sizeof(lept_member)), &m, sizeof(lept_member));
		size++;
		m.k = NULL;
		lept_parse_whitespace(c);
		if (*c->json == ',') {
			c->json++;
			lept_parse_whitespace(c);
		}
		else if(*c->json == '}'){
			size_t s = sizeof(lept_member) * size;
			c->json++;
			v->type = LEPT_OBJECT;
			v->u.o.size = size;
			memcpy(v->u.o.m = (lept_member*)malloc(s), lept_context_pop(c, s), s);
			return LEPT_PARSE_OK;
		}
		else {
			ret = LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
			break;
		}
	}
	free(m.k);
	for ( i = 0; i < size; i++) {
		lept_member* m = (lept_member*)lept_context_pop(c, sizeof(lept_member));
		free(m->k);
		lept_free(&m->v);
	}
	v->type = LEPT_NULL;
	return ret;
}

//��������
lept_type lept_get_type(const lept_value* v) {
	assert(v != NULL);
	return v->type;
}

//��ȡboolֵ
int lept_get_boolean(const lept_value* v) {
	assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
	return v->type == LEPT_TRUE;
}

//����boolֵ
void lept_set_boolean(lept_value* v, int b) {
	lept_free(v);
	v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

//��ȡstring��������
const char* lept_get_string(const lept_value* v) {
	assert(v != NULL && v->type == LEPT_STRING);
	return v->u.s.s;
}

//��ȡstring�������ݳ���
size_t lept_get_string_length(const lept_value* v) {
	assert(v != NULL && v->type == LEPT_STRING);
	return v->u.s.len;
}

//��ȡ������
double lept_get_number(const lept_value* v) {
	assert(v != NULL && v->type == LEPT_NUMBER);
	return v->u.n;
}

//��ȡarray��С
size_t lept_get_array_size(const lept_value* v) {
	assert(v != NULL && v->type == LEPT_ARRAY);
	return v->u.a.size;
}

//��ȡarray��Ԫ
lept_value* lept_get_array_element(const lept_value* v, size_t index) {
	assert(v != NULL && v->type == LEPT_ARRAY);
	assert(index < v->u.a.size);
	return &v->u.a.e[index];
}

//��������
void lept_set_number(lept_value* v, double n) {
	lept_free(v);
	v->u.n = n;
	v->type = LEPT_NUMBER;
}

//��ȡobject��С
size_t lept_get_object_size(const lept_value* v) {
	assert(v != NULL && v->type == LEPT_OBJECT);
	return v->u.o.size;
}

//��ȡobject��keyֵ
const char* lept_get_object_key(const lept_value* v, size_t index) {
	assert(v != NULL && v->type == LEPT_OBJECT);
	assert(index < v->u.o.size);
	return v->u.o.m[index].k;
}

//��ȡobject��key�ĳ���
size_t lept_get_object_key_length(const lept_value* v, size_t index) {
	assert(v != NULL && v->type == LEPT_OBJECT);
	assert(index < v->u.o.size);
	return v->u.o.m[index].klen;
}

//��ȡobject��value
lept_value* lept_get_object_value(const lept_value* v, size_t index) {
	assert(v != NULL && v->type == LEPT_OBJECT);
	assert(index < v->u.o.size);
	return &v->u.o.m[index].v;
}

//��ѯarray�еļ�ֵ(��������)
size_t lept_find_object_index(const lept_value* v, const char* key, size_t klen) {
	size_t i;
	assert(v != NULL && v->type == LEPT_OBJECT && key != NULL);
	for (i = 0; i < v->u.o.size; i++)
		if (v->u.o.m[i].klen == klen && memcmp(v->u.o.m[i].k, key, klen) == 0)
			return i;
	return LEPT_KEY_NOT_EXIST;
}

//��ѯarray�еļ�ֵ(����ֵ)
lept_value* lept_find_object_value(lept_value* v, const char* key, size_t klen) {
	size_t index = lept_find_object_index(v, key, klen);
	return index != LEPT_KEY_NOT_EXIST ? &v->u.o.m[index].v : NULL;
}

//�Ƚ��Ƿ����
int lept_is_equal(const lept_value* lhs, const lept_value* rhs) {
	size_t i;
	assert(lhs != NULL && rhs != NULL);
	if (lhs->type != rhs->type)
		return 0;
	switch (lhs->type)
	{
	case LEPT_STRING:
		return lhs->u.s.len == rhs->u.s.len && memcmp(lhs->u.s.s, rhs->u.s.s, lhs->u.s.len) == 0;
	case LEPT_NUMBER:
		return lhs->u.n == rhs->u.n;
	case LEPT_ARRAY:
		if (lhs->u.a.size != rhs->u.a.size)
			return 0;
		for (i = 0; i < lhs->u.a.size; i++)
			if (!lept_is_equal(&lhs->u.a.e[i], &rhs->u.a.e[i]))
				return 0;
		return 1;
	case LEPT_OBJECT:
		if (lhs->u.o.size != rhs->u.o.size)
			return 0;
		for (i = 0; i < lhs->u.a.size; i++) {
			if (lhs->u.o.m[i].klen != rhs->u.o.m[i].klen && memcmp(lhs->u.o.m[i].k, rhs->u.o.m[i].k, lhs->u.o.m[i].klen))
				return 0;
			if (!lept_is_equal(&lhs->u.o.m[i].v, &rhs->u.o.m[i].v))
				return 0;
		}
		return 1;
	default:
		return 1;
	}
}

//��ȸ���
void lept_copy(lept_value* dst, const lept_value* src) {
	size_t i;
	assert(src != NULL && dst != NULL && src != dst);
	switch (src->type) {
	case LEPT_STRING:
		lept_set_string(dst, src->u.s.s, src->u.s.len);
		break;
	case LEPT_ARRAY:
		lept_set_array(dst, src->u.a.size);
		for ( i = 0; i < src->u.a.size; i++)
		{
			lept_copy(&dst->u.a.e[i], &src->u.a.e[i]);
		}
		dst->u.a.size = src->u.a.size;
		break;
	case LEPT_OBJECT:
		lept_set_object(dst, src->u.o.size);
		for (i = 0; i < src->u.o.size; i++) {
			lept_value* val = lept_set_object_value(dst, src->u.o.m[i].k, src->u.o.m[i].klen);
			lept_copy(val, &src->u.o.m[i].v);
		}
		dst->u.o.size = src->u.o.size;
		break;
	default:
		lept_free(dst);
		memcpy(dst, src, sizeof(lept_value));
		break;
	}
}

//�ƶ�
void lept_move(lept_value* dst, lept_value* src) {
	assert(dst != NULL && src != NULL && src != dst);
	lept_free(dst);
	memcpy(dst, src, sizeof(lept_value));
	lept_init(src);
}

//����
void lept_swap(lept_value* lhs, lept_value* rhs) {
	assert(lhs != NULL && rhs != NULL);
	if (lhs != rhs) {
		lept_value temp;
		memcpy(&temp, lhs, sizeof(lept_value));
		memcpy(lhs, rhs, sizeof(lept_value));
		memcpy(rhs, &temp, sizeof(lept_value));
	}
}

//�������飬��������
void lept_set_array(lept_value* v, size_t capacity) {
	assert(v != NULL);
	lept_free(v);
	v->type = LEPT_ARRAY;
	v->u.a.size = 0;
	v->u.a.capacity = capacity;
	v->u.a.e = capacity > 0 ? (lept_value*)malloc(capacity * sizeof(lept_value)) : NULL;
}
//��ȡ��������
size_t lept_get_array_capacity(const lept_value* v) {
	assert(v != NULL && v->type == LEPT_ARRAY);
	return v->u.a.capacity;
}
//������������
void lept_reserve_array(lept_value* v, size_t capacity) {
	assert(v != NULL && v->type == LEPT_ARRAY);
	if (v->u.a.capacity < capacity) {
		v->u.a.capacity = capacity;
		v->u.a.e = (lept_value*)realloc(v->u.a.e, capacity * sizeof(lept_value)); //realloc�������·���ռ䲢�������ݵ��¿ռ�
	}
}
//��С�����������������ݴ�С
void lept_shrink_array(lept_value* v) {
	assert(v != NULL && v->type == LEPT_ARRAY);
	if (v->u.a.capacity > v->u.a.size) {
		v->u.a.capacity = v->u.a.size;
		v->u.a.e = (lept_value*)realloc(v->u.a.e, v->u.a.capacity * sizeof(lept_value));
	}
}
//ĩ������
lept_value* lept_pushback_array_element(lept_value* v) {
	assert(v != NULL && v->type == LEPT_ARRAY);
	if (v->u.a.size == v->u.a.capacity)
		lept_reserve_array(v, v->u.a.capacity == 0 ? 1 : v->u.a.capacity * 2);
	lept_init(&v->u.a.e[v->u.a.size]);
	return &v->u.a.e[v->u.a.size++];
}
//ɾȥĩ��Ԫ��
void lept_popback_array_element(lept_value* v) {
	assert(v != NULL && v->type == LEPT_ARRAY && v->u.a.size > 0);
	lept_free(&v->u.a.e[--v->u.a.size]);
}

//����Object����������
void lept_set_object(lept_value* v, size_t capacity) {
	assert(v != NULL);
	lept_free(v);
	v->type = LEPT_OBJECT;
	v->u.o.size = 0;
	v->u.o.capacity = capacity;
	v->u.o.m = capacity > 0 ? (lept_member*)malloc(capacity * sizeof(lept_member)) : NULL;
}
//����object����
void lept_reserve_object(lept_value* v, size_t capacity) {
	assert(v != NULL && v->type == LEPT_OBJECT);
	if (v->u.o.capacity < capacity) {
		v->u.o.capacity = capacity;
		v->u.o.m = (lept_member*)realloc(v->u.o.m, capacity * sizeof(lept_member));
	}
}
//������object�����key������ָ��ָ���Ӧvalue
lept_value* lept_set_object_value(lept_value* v, const char* key, size_t klen) {
	assert(v != NULL && v->type == LEPT_OBJECT && key != NULL);
	size_t i, index;
	index = lept_find_object_index(v, key, klen);
	if (index != LEPT_KEY_NOT_EXIST)
		return &v->u.o.m[index].v;
	if (v->u.o.size == v->u.o.capacity) {
		lept_reserve_object(v, v->u.o.capacity == 0 ? 1 : (v->u.o.capacity << 1));
	}
	i = v->u.o.size;
	v->u.o.m[i].k = (char*)malloc(klen + 1);
	memcpy(v->u.o.m[i].k, key, klen);
	v->u.o.m[i].k[klen] = '\0';
	v->u.o.m[i].klen = klen;
	lept_init(&v->u.o.m[i].v);
	v->u.o.size++;
	return &v->u.o.m[i].v;
}

//��̬�����ڴ�ռ䣬�����ַ���
void lept_set_string(lept_value* v, const char* s, size_t len) {
	assert(v != NULL && (s != NULL || len == 0));
	lept_free(v);
	v->u.s.s = (char*)malloc(len + 1);
	memcpy(v->u.s.s, s, len);
	v->u.s.s[len] = '\0';    //�������ӿ��ַ���Ϊ������
	v->u.s.len = len;
	v->type = LEPT_STRING;
}

//�����������ͷ��ڴ�
void lept_free(lept_value* v) {
	size_t i;
	assert(v != NULL);
	switch (v->type) {
	case LEPT_STRING:
		free(v->u.s.s);
		break;
	case LEPT_ARRAY:
		for (i = 0; i < v->u.a.size; i++)
			lept_free(&v->u.a.e[i]);
		free(v->u.a.e);
		break;
	case LEPT_OBJECT:
		for (i = 0; i < v->u.o.size; i++) {
			free(v->u.o.m[i].k);
			lept_free(&v->u.o.m[i].v);
		}
		free(v->u.o.m);
		break;
	default: break;
	}
	v->type = LEPT_NULL;
}

