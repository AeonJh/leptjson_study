/* Created by AeonJh on 2023/5/27. */

#include "leptjson.h"
#include <stdlib.h>  /* NULL, malloc(), realloc(), free(), strtod() */
#include <assert.h>  /* assert() */
#include <errno.h>   /* errno, ERANGE */
#include <math.h>    /* HUGE_VAL */
#include <stdio.h>   /* sprintf() */
#include <string.h>  /* memcpy() */

/**********************************************
JSON-text: ws value ws
    ws = *(%x20 / %x09 / %x0A / %x0D)
    value = null / false / true
        null  = "null"
        false = "false"
        true  = "true"
    value = number
        number = [ "-" ] int [ frac ] [ exp ]
        int = "0" / digit1-9 *digit
        frac = "." 1*digit
        exp = ("e" / "E") ["-" / "+"] 1*digit
**********************************************/

/* The initial allocated stack size */
#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

/* Determines that characters are equal, if so, move the pointer back one bit */
#define EXCEPT(c,ch) do { assert(*c->json == (ch)); c->json++; } while(0)
/* Determine if it‘s a number */
#define ISDIGIT(ch) ((ch) >= '0' && (ch) <= '9')
/* Determine if it‘s a number except 0 */
#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')
/* push single character onto the stack */
#define PUTC(c, ch) do { *(char*)lept_context_get(c, sizeof(char)) = (ch); } while(0)

/* string error */
#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

/* Context to be parsed */
typedef struct {
    const char* json;
    /* input output buffers (dynamic stack) */
    char* stack;
    size_t size, top;
} lept_context;

/* push characters onto the stack */
/* It is possible that lept_parse_value could cause memory reallocation on the stack,
which could invalidate pointers to the stack, To avoid this, change the return pointer
to return the index of the location pointed to on the stack. */
static size_t lept_context_push(lept_context* c, size_t size) {
    assert(size > 0);
    /* if the stack doesn't fit or is full, double the size */
    if (c->top + size >= c->size) {
        if (c->size == 0) {
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        }
        /* c->size * 1.5 */
        /* why not c->size * 2 ?
        When allocating a 1.5x increase, it is possible to reuse existing memory space,for more
        information, please refer to: https://www.zhihu.com/question/25079705/answer/30030883 */
        while (c->top + size >= c->size) {
            /* equivalent to c->size += c->size * 0.5, but shift operations are faster */
            c->size += c->size >> 1;
        }
        /* allocate memory */
        c->stack = (char*)realloc(c->stack, c->size);
        /* todo */
    }
    /* update the top of the stack */
    c->top += size;
    /* return index into the stack */
    return c->top - size;
}

/* return the memory address of the index */
static void* lept_context_get(lept_context* c, size_t size) {
    size_t index = lept_context_push(c, size);
    assert(index + size <= c->top);
    return c->stack + index;
}

/* pop characters from the stack */
static void* lept_context_pop(lept_context* c, size_t size) {
    assert(c->top >= size);
    /* returns the memory address of the pop-up value and update the top of the stack */
    return c->stack + (c->top -= size);
}

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

/* parse raw string */
static int lept_parse_string_raw(lept_context* c, char** str, size_t* len) {
    /* set the head of the string */
    size_t head = c->top;
    /* temporary storage of surrogates */
    /* unsigned u, u2; */
    const char* p;
    /* validate the first character */
    EXCEPT(c, '\"');
    p = c->json;
    /* loop to find the end of the string */
    for(;;) {
        /* When this statement is executed,
        ch is assigned the value p,and p = p+1 after execution */
        char ch = *p++;
        switch (ch) {
            case '\"':
                *len = c->top - head;
                /* copy the string to the stack */
                *str = lept_context_pop(c, *len);
                /* update the json string */
                c->json = p;
                return LEPT_PARSE_OK;
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
                    case 'u':
                        /* validate the four hexadecimal digits */
                        /* todo */
                        break;
                    default:
                        STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
                }
                break;
            case '\0':
                STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
            default:
                /* check the character */
                if ((unsigned char)ch < 0x20)
                    STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
                /* push the character to the stack */
                PUTC(c, ch);
        }
    }
}

/* parse string */
static int lept_parse_string(lept_context* c, lept_value* v) {
    int ret;
    char* s;
    size_t len;
    /* parse string */
    if ((ret = lept_parse_string_raw(c, &s, &len)) == LEPT_PARSE_OK) {
        lept_set_string(v, s, len);
    }
    return ret;
}

/* parse single literal */
static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case '"':  return lept_parse_string(c, v);
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
    /* initialize lept_context */
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
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
    switch (v->type) {
        case LEPT_STRING:
            free(v->s.s);
            break;
        default: break;
    }
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

const char* lept_get_string(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    /* return the string */
    return v->s.s;
}

size_t lept_get_string_length(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    /* return the length of the string */
    return v->s.len;
}

void lept_set_string(lept_value* v, const char* s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    /* free the memory */
    lept_free(v);
    /* set the value's type */
    /* set the string */
    v->s.s = (char*)malloc(len + 1);
    memcpy(v->s.s, s, len);
    v->s.s[len] = '\0';
    /* set the length of the string */
    v->s.len = len;
    v->type = LEPT_STRING;
}
