//
// Created by AeonJh on 2023/5/27.
//
#include"leptjson.h"
#include<assert.h>  /* assert() */
#include<stdlib.h>  /* NULL */
#include<string.h>  /* memcpy() */

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

/* parse single literal */
static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
        default:   return LEPT_PARSE_INVALID_VALUE;
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
    return v->type;
}

void lept_set_boolean(lept_value* v, int boolean) {
    assert(v != NULL);
    /* set the value's type */
    v->type = boolean ? LEPT_TRUE : LEPT_FALSE;
}
