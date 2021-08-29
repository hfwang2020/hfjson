#ifndef HFJSON_H__
#define HFJSON_H__

#include <stddef.h>

typedef enum {
    HF_NULL,
    HF_FALSE,
    HF_TRUE,
    HF_NUMBER,
    HF_STRING,
    HF_ARRAY,
    HF_OBJECT
} hf_type;

// double n 解析数字类型
typedef struct {
    union {
        struct { char* s;size_t len; }s;
        double n;
    }u;
    hf_type type;
} hf_value;

enum {
    HF_PARSE_OK = 0,
    HF_PARSE_EXPECT_VALUE,
    HF_PARSE_INVALID_VALUE,
    HF_PARSE_ROOT_NOT_SINGULAR,
    HF_PARSE_NUMBER_TOO_BIG,
    HF_PARSE_MISS_QUOTATION_MARK,
    HF_PARSE_INVALID_STRING_ESCAPE,
    HF_PARSE_INVALID_STRING_CHAR
};

// set v->type null
#define hf_init(v) do { (v)->type = HF_NULL; } while(0)

int hf_parse(hf_value* v, const char* json);

// free memery of v
void hf_free(hf_value* v);

// return v->type
hf_type hf_get_type(const hf_value* v);


// clear v and set v.type null
#define hf_set_null(v) hf_free(v)

// get the true or false of v(type bool)
int hf_get_boolean(const hf_value* v);
// set the bool of v
void hf_set_boolean(hf_value* v, int b);

// get the number of v(type number)
double hf_get_number(const hf_value* v);
// set the number of v
void hf_set_number(hf_value* v, double n);

// get the string of v string type
const char* hf_get_string(const hf_value* v);
// get the length of v(string)
size_t hf_get_string_length(const hf_value* v);
// set the char* s to v->u.s.s
void hf_set_string(hf_value* v, const char* s, size_t len);

#endif /* HFJSON_H__ */

