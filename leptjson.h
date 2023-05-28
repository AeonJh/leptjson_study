/* Created by AeonJh on 2023/5/27.*/

#ifndef LEPTJSON_STUDY_LEPTJSON_H
#define LEPTJSON_STUDY_LEPTJSON_H

#include <stddef.h> /* size_t */

/* 1.json value type */
typedef enum {
    LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT
} lept_type;

/* 2.json parse result */
enum {
    LEPT_PARSE_OK = 0,
    /* The reasons for this error are: JSON string is empty or contains only whitespace characters. */
    LEPT_PARSE_EXPECT_VALUE,
    /* generic error, usually due to invalid format. */
    LEPT_PARSE_INVALID_VALUE,
    LEPT_PARSE_ROOT_NOT_SINGULAR,
    /* number error */
    LEPT_PARSE_NUMBER_TOO_BIG,
    /* string error*/
    LEPT_PARSE_MISS_QUOTATION_MARK,
    LEPT_PARSE_INVALID_STRING_ESCAPE, /* invalid escape character */
    LEPT_PARSE_INVALID_STRING_CHAR,
    LEPT_PARSE_INVALID_UNICODE_HEX, /* invalid unicode hexadecimal number */
    LEPT_PARSE_INVALID_UNICODE_SURROGATE /* invalid unicode surrogate pair */
};

/* 3.json value struct */
typedef struct lept_value lept_value; /* forward declaration */

struct lept_value {
/* union: a variable that can be used to store different types of data at
different times Anonymous unions are a C11 extension. */
    union {
        struct { char* s; size_t len; }s; /* string */
        double n; /* number */
    };
    lept_type type; /* value type */
};

/* init */
#define lept_init(v) do { (v)->type = LEPT_NULL; } while(0)
/* set null */
#define lept_set_null(v) lept_free(v)

/* 4.API */
/* parse json string to json value */
int lept_parse(lept_value* v, const char* json);

/* free json value */
void lept_free(lept_value* v);

/* get value's type */
int lept_get_type(const lept_value* v);

/* get or set boolean's value */
int lept_get_boolean(const lept_value* v);
void lept_set_boolean(lept_value* v, int boolean);

/* get or set number's value */
double lept_get_number(const lept_value* v);
void lept_set_number(lept_value* v, double number);

/* get or set string's value and get string's length */
const char* lept_get_string(const lept_value* v);
size_t lept_get_string_length(const lept_value* v);
void lept_set_string(lept_value* v, const char* s, size_t len);


#endif