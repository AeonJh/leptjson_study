//
// Created by AeonJh on 2023/5/27.
//
#include "leptjson.h"
#include <stdlib.h>  /* NULL, malloc(), realloc(), free(), strtod() */
#include <assert.h>  /* assert() */
#include <errno.h>   /* errno, ERANGE */
#include <math.h>    /* HUGE_VAL */
#include <stdio.h>   /* sprintf() */
#include <string.h>  /* memcpy() */

/*****************************************
JSON-text: ws value ws
    ws = *(%x20 / %x09 / %x0A / %x0D)
    value = null / false / true
    null  = "null"
    false = "false"
    true  = "true"
*****************************************/

/* Determines that characters are equal, if so, move the pointer back one bit */
#define EXCEPT(c,ch) do { assert(*c->json == (ch)); c->json++; } while(0)
/* Determine if it‘s a number */
#define ISDIGIT(ch) ((ch) >= '0' && (ch) <= '9')
/* Determine if it‘s a number except 0 */
#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')

/* Context to be parsed */
typedef struct {
    const char* json;
} lept_context;

/* parse whitespace between the context */
static void lept_parse_whitespace(lept_context* c) {
    const char* p = c->json;
    /* skip whitespace(%x20 / %x09 / %x0A / %x0D) */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    /* update the json string */
    c->json = p;
}

/* parse literal */
static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
    size_t i;
    /* check the first character */
    EXCEPT(c, literal[0]);
    /* check the rest characters */
    for(i = 0; literal[i+1]; i++) {
        if (c->json[i] != literal[i+1]) {
            return LEPT_PARSE_INVALID_VALUE;
        }
    }
    /* update the json string */
    c->json += i;
    /* set the value's type */
    v->type = type;
    return LEPT_PARSE_OK;
}

/* parse number */
static int lept_parse_number(lept_context* c, lept_value* v) {
    const char* p = c->json;
    /* validate minus('-') */
    if (*p == '-') p++;
    /* ignore the number '0' if it is the first character */
    if (*p == '0') p++;
    /* validate the number '1'~'9' */
    else {
        if (!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;
        /* The loop determines whether it is a number or not */
        for (p++; ISDIGIT(*p); p++);
    }
    /* validate the fraction */
    if (*p == '.') {
        p++;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    /* validate the exponent */
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    errno = 0;
    /* convert string to double */
    v->n = strtod(c->json, NULL);
    /* check the range of the number */
    if (errno == ERANGE && (v->n == HUGE_VAL || v->n == -HUGE_VAL)) {
        return LEPT_PARSE_NUMBER_TOO_BIG;
    }
    /* set the value's type */
    v->type = LEPT_NUMBER;
    /* update the json string */
    c->json = p;
    return LEPT_PARSE_OK;
}

/* parse single literal */
static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
        default:   return lept_parse_number(c, v);
    }
}

/* parse complete literal */
int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    /* can be used to check the parse result */
    int ret;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    /* parse whitespace */
    lept_parse_whitespace(&c);
    /* parse the first character */
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        /* parse the next character */
        lept_parse_whitespace(&c);
        /* if the next character is not '\0', then the json string is not over */
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

void lept_free(lept_value* v) {
    assert(v != NULL);
    /* free the memory */
    v->type = LEPT_NULL;
}

int lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

int lept_get_boolean(const lept_value* v) {
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
    /* return 1 if the value is true, otherwise return 0 */
    return v->type == LEPT_TRUE ? 1 : 0;
}

void lept_set_boolean(lept_value* v, int boolean) {
    assert(v != NULL);
    /* set the value's type */
    v->type = boolean ? LEPT_TRUE : LEPT_FALSE;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    /* return the number */
    return v->n;
}
void lept_set_number(lept_value* v, double number) {
    assert(v != NULL);
    /* set the value's type */
    v->type = LEPT_NUMBER;
    /* set the number */
    v->n = number;
}