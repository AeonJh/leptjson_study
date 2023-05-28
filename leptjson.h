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
    LEPT_PARSE_OK = 0, /* parse success */
    /* parse error */
    LEPT_PARSE_EXPECT_VALUE, /* JSON string is empty or contains only whitespace characters. */
    LEPT_PARSE_INVALID_VALUE, /* usually due to invalid format. */
    LEPT_PARSE_ROOT_NOT_SINGULAR, /* JSON text contains multiple values. */
    /* number error */
    LEPT_PARSE_NUMBER_TOO_BIG, /* number too big. */
    /* string error*/
    LEPT_PARSE_MISS_QUOTATION_MARK, /* miss quotation mark('\"'). */
    LEPT_PARSE_INVALID_STRING_ESCAPE, /* invalid escape character. */
    LEPT_PARSE_INVALID_STRING_CHAR, /* invalid string character(char < 0x20). */
    LEPT_PARSE_INVALID_UNICODE_HEX, /* invalid unicode hexadecimal number. */
    LEPT_PARSE_INVALID_UNICODE_SURROGATE, /* invalid unicode surrogate pair. */
    /* array error */
    LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, /* miss comma(',') or square bracket('[' or ']') */
};

/* 3.json value struct */
typedef struct lept_value lept_value; /* forward declaration */

struct lept_value {
/* union: a variable that can be used to store different types of data at
different times Anonymous unions are a C11 extension. */
    union {
        /* string */ /* s: string */ /* len: string's length */
        struct { char* s; size_t len; }s;
        /* array */ /* e: element */ /* size: array's size */ /* capacity: array's capacity */
        /* array: dynamic array */
        /* The capacity is added here to better support operations on arrays, such as: add or delete elements */
        /* The capacity is the size of the array, and the size is the number of elements in the array. */
        struct { lept_value* e; size_t size, capacity; }a;
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

/* get type */
int lept_get_type(const lept_value* v);

/* boolean */
int lept_get_boolean(const lept_value* v);          /* get boolean */
void lept_set_boolean(lept_value* v, int boolean);  /* set boolean */

/* number */
double lept_get_number(const lept_value* v);        /* get number */
void lept_set_number(lept_value* v, double number); /* set number */

/* string */
const char* lept_get_string(const lept_value* v);                /* get string */
size_t lept_get_string_length(const lept_value* v);              /* get string's length */
void lept_set_string(lept_value* v, const char* s, size_t len);  /* set string */

/* array */
void lept_set_array(lept_value* v, size_t capacity);                        /* set array */
size_t lept_get_array_size(const lept_value* v);                            /* get array's size */
size_t lept_get_array_capacity(const lept_value* v);                        /* get array's capacity */
void lept_reserve_array(lept_value* v, size_t capacity);                    /* reserve array's capacity */
void lept_shrink_array(lept_value* v);                                      /* shrink array's capacity */
void lept_clear_array(lept_value* v);                                       /* clear array */
lept_value* lept_get_array_element(lept_value* v, size_t index);            /* get array's element */
lept_value* lept_pushback_array_element(lept_value* v);                     /* pushback array's element */
void lept_popback_array_element(lept_value* v);                             /* popback array's element */
lept_value* lept_insert_array_element(lept_value* v, size_t index);         /* insert array's element */
void lept_erase_array_element(lept_value* v, size_t index, size_t count);   /* erase array's element */


#endif