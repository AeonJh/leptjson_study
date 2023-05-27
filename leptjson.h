//
// Created by AeonJh on 2023/5/27.
//

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
    /* JSON string is empty or contains only whitespace characters. */
    LEPT_PARSE_EXPECT_VALUE,
    LEPT_PARSE_INVALID_VALUE,
    LEPT_PARSE_ROOT_NOT_SINGULAR
};

/* 3.json value struct */
typedef struct lept_value lept_value;

struct lept_value {
    lept_type type;
};

/* init */
#define lept_init(v) do { (v)->type = LEPT_NULL; } while(0)

/* 4.api */
    /* parse json string to json value */
int lept_parse(lept_value* v, const char* json);

    /* free json value */
void lept_free(lept_value* v);

    /* get value's type */
int lept_get_type(const lept_value* v);

    /* get or set boolean's value */
int lept_get_boolean(const lept_value* v);
void lept_set_boolean(lept_value* v, int boolean);

#endif //LEPTJSON_STUDY_LEPTJSON_H
