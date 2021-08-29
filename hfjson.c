#include "hfjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL */
#include <errno.h>   /* errno, ERANGE */
#include <math.h>    /* HUGE_VAL */
#include <string.h>  /* memcpy() */


#ifndef HF_PARSE_STACK_INIT_SIZE
#define HF_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch) do{assert(*c->json==(ch));c->json++;} while(0)
#define ISDIGIT1TO9(ch) ((ch)<= '9' && (ch) >= '1')   
#define ISDIGIT(ch) ((ch)>='0'&&(ch)<='9')
#define PUTC(c, ch)         do { *(char*)hf_context_push(c, sizeof(char)) = (ch); } while(0)

// void PUTC1(hf_context* c, char ch) {
//     *(char*)hf_context_push(c, sizeof(ch)) = ch;
// }

typedef struct {
    const char* json;
    // string buffer stack
    char* stack;
    size_t size, top;
}hf_context;

// push string stack in hf_context 
static void* hf_context_push(hf_context* c, size_t size) {
    void* ret;
    assert(size > 0);
    if (c->top + size > c->size) {
        if (c->size == 0)
            c->size = HF_PARSE_STACK_INIT_SIZE;
        while (c->top + size >= c->size) {
            c->size += c->size >> 1;
        }
        c->stack = (char*)realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

// pop the string buffer stack
static void* hf_context_pop(hf_context* c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

// parse whitespace
static void hf_parse_whitespace(hf_context* c) {
    const char* p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    c->json = p;
}

static int hf_parse_literal(hf_context* c, hf_value* v, const char* literal, hf_type type) {
    int i;
    EXPECT(c, literal[0]);
    // literal[-1] = '\0' = 0 结束循环 妙
    for (i = 0; literal[i + 1]; i++) {
        if (c->json[i] != literal[i + 1])
            return HF_PARSE_INVALID_VALUE;
    }
    c->json += i;
    v->type = type;
    return HF_PARSE_OK;
}

static int hf_parse_number(hf_context* c, hf_value* v) {
    const char* p = c->json;
    if (*p == '-') p++;
    if (*p == '0')p++;
    else {
        if (!ISDIGIT1TO9(*p)) return HF_PARSE_INVALID_VALUE;
        for (p++;ISDIGIT(*p);p++);
    }
    if (*p == '.') {
        p++;
        if (!ISDIGIT(*p)) return HF_PARSE_INVALID_VALUE;
        for (p++;ISDIGIT(*p);p++);
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!ISDIGIT(*p)) return HF_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    // strtod <stdlib.h> 将 char* 转成 double
    // end 类型 *end
    // &end type: **end 
    // 当要函数改变传入的值时需要传出改变的值的指针
    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
        return HF_PARSE_NUMBER_TOO_BIG;
    v->type = HF_NUMBER;
    c->json = p;
    return HF_PARSE_OK;
}

static int hf_parse_string(hf_context* c, hf_value* v) {
    size_t head = c->top, len;
    const char* p;
    EXPECT(c, '"');
    p = c->json;
    for (;;) {
        char ch = *p++;
        switch (ch) {
            case '\"':
                len = c->top - head;
                hf_set_string(v, (const char*)hf_context_pop(c, len), len);
                c->json = p;
                return HF_PARSE_OK;
            case '\\':
                switch (*p++) {
                    case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/' ); break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    default:
                        c->top = head;
                        return HF_PARSE_INVALID_STRING_ESCAPE;
                }
                break;
            case '\0':
                c->top = head;
                return HF_PARSE_MISS_QUOTATION_MARK;
            default:
                if ((unsigned char)ch < 0x20) { 
                    c->top = head;
                    return HF_PARSE_INVALID_STRING_CHAR;
                }
                PUTC(c, ch);
        }
    }
}

static int hf_parse_value(hf_context* c, hf_value* v) {
    switch (*c->json) {
        case 'n':  return hf_parse_literal(c, v, "null", HF_NULL);
        case 't':  return hf_parse_literal(c, v, "true", HF_TRUE);
        case 'f':  return hf_parse_literal(c, v, "false", HF_FALSE);
        default:   return hf_parse_number(c, v);
        case '"':  return hf_parse_string(c, v);
        case '\0': return HF_PARSE_EXPECT_VALUE;
    }
}

int hf_parse(hf_value* v, const char* json) {
    hf_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    // 初始化c.stack
    c.stack = NULL;
    c.size = c.top = 0;
    hf_init(v);
    hf_parse_whitespace(&c);
    if ((ret = hf_parse_value(&c, v)) == HF_PARSE_OK) {
        hf_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = HF_NULL;
            ret = HF_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

void hf_free(hf_value* v) {
    assert(v != NULL);
    if (v->type == HF_STRING)
        free(v->u.s.s);
    v->type = HF_NULL;
}

hf_type hf_get_type(const hf_value* v) {
    assert(v != NULL);
    return v->type;
}

int hf_get_boolean(const hf_value* v) {
    assert(v != NULL && (v->type == HF_TRUE || v->type == HF_FALSE));
    return v->type == HF_TRUE;
}

void hf_set_boolean(hf_value* v, int b) {
    hf_free(v);
    v->type = b ? HF_TRUE : HF_FALSE;
}

double hf_get_number(const hf_value* v) {
    assert(v != NULL && v->type == HF_NUMBER);
    return v->u.n;
}

void hf_set_number(hf_value* v, double n) {
    hf_free(v);
    v->u.n = n;
    v->type = HF_NUMBER;
}

const char* hf_get_string(const hf_value* v) {
    assert(v != NULL && v->type == HF_STRING);
    return v->u.s.s;
}

size_t hf_get_string_length(const hf_value* v) {
    assert(v != NULL && v->type == HF_STRING);
    return v->u.s.len;
}

void hf_set_string(hf_value* v, const char* s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    hf_free(v);
    v->u.s.s = (char*)malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = HF_STRING;
}

