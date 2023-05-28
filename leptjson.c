/* Created by AeonJh on 2023/5/27. */

#include "leptjson.h"
#include <stdlib.h>  /* NULL, malloc(), realloc(), free(), strtod() */
#include <assert.h>  /* assert() */
#include <errno.h>   /* errno, ERANGE */
#include <math.h>    /* HUGE_VAL */
#include <stdio.h>   /* sprintf() */
#include <string.h>  /* memcpy() */

/**************************************************************
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
    value = string
        string = quotation-mark *char quotation-mark
        char = unescaped /
            escape (
                %x22 /          ; "    quotation mark  U+0022
                %x5C /          ; \    reverse solidus U+005C
                %x2F /          ; /    solidus         U+002F
                %x62 /          ; b    backspace       U+0008
                %x66 /          ; f    form feed       U+000C
                %x6E /          ; n    line feed       U+000A
                %x72 /          ; r    carriage return U+000D
                %x74 /          ; t    tab             U+0009
                %x75 4HEXDIG )  ; uXXXX                U+XXXX
        escape = %x5C              ; \
        quotation-mark = %x22      ; "
        unescaped = %x20-21 / %x23-5B / %x5D-10FFFF
    value = array
        array = %x5B ws [ value *( ws %x2C ws value ) ] ws %x5D
        %x5B = [
        %x2C = ,
        %x5D = ]
****************************************************************/

/* The initial allocated stack size */
#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

/* Determines that characters are equal, if so, move the pointer back one bit */
#define EXPECT(c,ch) do { assert(*c->json == (ch)); c->json++; } while(0)
/* Determine if it‘s a number */
#define ISDIGIT(ch) ((ch) >= '0' && (ch) <= '9')
/* Determine if it‘s a number except 0 */
#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')
/* Determine if it‘s a hex number */
#define ISHEX(ch) (ISDIGIT(ch) || ((ch) >= 'A' && (ch) <= 'F') || ((ch) >= 'a' && (ch) <= 'f'))
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
    EXPECT(c, literal[0]);
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

/* parse the 4 hexadecimal digits */
static const char* lept_parse_hex4(const char* p, unsigned* u) {
    size_t i;
    *u = 0;
    for (i = 0; i < 4; i++) {
        char ch = *p++;
        /* shift left 4 bits to make room for a hexadecimal digit */
        *u <<= 4;
        /* converts a hexadecimal digit to it's corresponding Unicode value */
        if (ch >= '0' && ch <= '9') *u |= ch - '0';
        else if (ch >= 'A' && ch <= 'F') *u |= ch - ('A' - 10);
        else if (ch >= 'a' && ch <= 'f') *u |= ch - ('a' - 10);
        else return NULL;
    }
    /* invalid hexadecimal digit */
    return p;
}

/* encode UTF-8 */
/************************************************************
Unicode 十六进制码点范围	UTF-8 二进制
0000 0000 - 0000 007F	0xxxxxxx
0000 0080 - 0000 07FF	110xxxxx 10xxxxxx
0000 0800 - 0000 FFFF	1110xxxx 10xxxxxx 10xxxxxx
0001 0000 - 0010 FFFF	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
************************************************************/
static void lept_encode_utf8(lept_context* c, unsigned u) {
    /* 1 byte */
    if (u <= 0x7F) {
        PUTC(c, u & 0xFF);
    }
    /* 2 bytes */
    else if (u <= 0x7FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
        PUTC(c, 0x80 | ( u       & 0x3F));
    }
    /* 3 bytes */
    else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
    /* 4 bytes */
    else {
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
}

/* parse raw string */
static int lept_parse_string_raw(lept_context* c, char** str, size_t* len) {
    /* set the head of the string */
    size_t head = c->top;
    /* temporary storage of surrogates */
    unsigned u, u2;
    const char* p;
    /* validate the first character */
    EXPECT(c, '\"');
    p = c->json;
    /* loop to find the end of the string */
    for(;;) {
        /* When this statement is executed,
        ch is assigned the value p,and p = p+1 after execution */
        char ch = *p++;
        switch (ch) {
            case '\"':
                /* get the length of the string */
                *len = c->top - head;
                /* copy the string from the stack */
                *str = lept_context_pop(c, *len);
                /* update the json string */
                c->json = p;
                return LEPT_PARSE_OK;
            case '\\':
                /* deal with escape characters */
                switch (*p++) {
                    case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/' ); break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    /* deal with surrogates */
                    case 'u':
                        /* validate the 4 hexadecimal digits */
                        if (!(p = lept_parse_hex4(p, &u)))
                            STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                        /* check the high surrogate */
                        if (u >= 0xD800 && u <= 0xDBFF) {
                            /* validate the low surrogate */
                            if (p[0] == '\\' && p[1] == 'u') {
                                /* skip '\u' */
                                p += 2;
                                if (!(p = lept_parse_hex4(p, &u2)))
                                    STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                                if (u2 < 0xDC00 || u2 > 0xDFFF)
                                    STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                                /* calculate the code point */
                                /* codepoint = 0x10000 + (H − 0xD800) × 0x400 + (L − 0xDC00) */
                                u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                            }
                            else
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                        }
                        /* encode the code point as utf8 */
                        lept_encode_utf8(c,u);
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

/* forward declaration */
static int lept_parse_value(lept_context* c, lept_value* v);

/* parse array */
static int lept_parse_array(lept_context* c, lept_value* v) {
    int ret;
    size_t size = 0, i;
    EXPECT(c, '[');
    lept_parse_whitespace(c);
    /* empty array */
    if (*c->json == ']') {
        c->json++;
        lept_set_array(v, 0);
        return LEPT_PARSE_OK;
    }
    /* parse elements */
    for (;;) {
        lept_value e;
        lept_init(&e);
        /* parse element */
        if ((ret = lept_parse_value(c, &e)) != LEPT_PARSE_OK)
            break;
        /* store the element to stack */
        memcpy(lept_context_get(c, sizeof(lept_value)), &e, sizeof(lept_value));
        size++;
        lept_parse_whitespace(c);
        /* check the next character */
        if (*c->json == ',') {
            c->json++;
            lept_parse_whitespace(c);
        }
        /* end of array */
        else if (*c->json == ']') {
            c->json++;
            lept_set_array(v, size);
            /* copy the elements from the stack */
            memcpy(v->a.e, lept_context_pop(c, size * sizeof(lept_value)), size * sizeof(lept_value));
            v->a.size = size;
            return LEPT_PARSE_OK;
        }
        else {
            ret = LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }
    /* pop and free the elements on the stack */
    for (i = 0; i < size; i++)
        lept_free((lept_value*)lept_context_pop(c, sizeof(lept_value)));
    v->type = LEPT_NULL;
    return ret;
}

/* parse object */
static int lept_parse_object(lept_context* c, lept_value* v) {
    size_t i, size;
    lept_member m;
    int ret;
    EXPECT(c, '{');
    lept_parse_whitespace(c);
    /* empty array */
    if (*c->json == '}') {
        c->json++;
        lept_set_object(v, 0);
        return LEPT_PARSE_OK;
    }
    /* initialize the member */
    m.k = NULL;
    size = 0;
    /* parse member */
    for (;;) {
        char *str;
        lept_init(&m.v);
        /* parse member::key */
        if (*c->json != '"') {
            ret = LEPT_PARSE_MISS_KEY;
            break;
        }
        if ((ret = lept_parse_string_raw(c, &str, &m.klen)) != LEPT_PARSE_OK)
            break;
        /* copy the key from stack */
        memcpy(m.k = (char*) malloc(m.klen + 1), str, m.klen);
        m.k[m.klen] = '\0';
        /* parse ws colon ws */
        lept_parse_whitespace(c);
        if (*c->json != ':') {
            ret = LEPT_PARSE_MISS_COLON;
            break;
        }
        c->json++;
        lept_parse_whitespace(c);
        /* parse member::value */
        if ((ret = lept_parse_value(c, &m.v)) != LEPT_PARSE_OK)
            break;
        /* store the member to stack */
        memcpy(lept_context_get(c, sizeof(lept_member)), &m, sizeof(lept_member));
        size++;
        m.k = NULL; /* ownership is transferred to member on stack */
        /* parse ws [comma | right-curly-brace] ws */
        lept_parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            lept_parse_whitespace(c);
        }
        /* end of object */
        else if (*c->json == '}') {
            c->json++;
            lept_set_object(v, size);
            /* copy the member from the stack */
            memcpy(v->o.m, lept_context_pop(c, size * sizeof(lept_member)), size * sizeof(lept_member));
            v->o.size = size;
            return LEPT_PARSE_OK;
        }
        else {
            ret = LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
    }
    /* free the key */
    free(m.k);
    /* pop and free the members on the stack */
    for (i = 0; i < size; i++) {
        lept_member* mf = ((lept_member*)lept_context_pop(c, sizeof(lept_member)));
        free(mf->k);
        lept_free(&mf->v);
    }
    v->type = LEPT_NULL;
    return ret;
}

/* parse single literal */
static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case '"':  return lept_parse_string(c, v);
        case '[':  return lept_parse_array(c, v);
        case '{':  return lept_parse_object(c, v);
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
    /* initialize lept_value */
    lept_init(v);
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
    /* free the memory of lept_context */
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

void lept_free(lept_value* v) {
    assert(v != NULL);
    size_t i;
    /* free the memory */
    switch (v->type) {
        case LEPT_STRING:
            free(v->s.s);
            break;
        case LEPT_ARRAY:
            /* free the memory of each element */
            for (i = 0; i < v->a.size; i++)
                lept_free(&v->a.e[i]);
            /* free the memory of the array */
            free(v->a.e);
            break;
        case LEPT_OBJECT:
            /* free the memory of each member */
            for (i = 0; i < v->o.size; i++) {
                free(v->o.m[i].k);
                lept_free(&v->o.m[i].v);
            }
            /* free the memory of the object */
            free(v->o.m);
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

void lept_set_array(lept_value* v, size_t capacity) {
    assert(v != NULL);
    lept_free(v);
    v->type = LEPT_ARRAY;
    v->a.size = 0;
    v->a.capacity = capacity;
    v->a.e = capacity > 0 ? (lept_value*)malloc(capacity * sizeof(lept_value)) : NULL;
}

size_t lept_get_array_size(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    return v->a.size;
}

size_t lept_get_array_capacity(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    return v->a.capacity;
}

void lept_reserve_array(lept_value* v, size_t capacity) {
    /* todo */
}

void lept_shrink_array(lept_value* v) {
    /* todo */
}

void lept_clear_array(lept_value* v) {
    /* todo */
}

lept_value* lept_get_array_element(lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    assert(index < v->a.size);
    return &v->a.e[index];
}

lept_value* lept_pushback_array_element(lept_value* v) {
    /* todo */
}

void lept_popback_array_element(lept_value* v) {
    /* todo */
}

lept_value* lept_insert_array_element(lept_value* v, size_t index) {
    /* todo */
}

void lept_erase_array_element(lept_value* v, size_t index, size_t count) {
    /* todo */
}

void lept_set_object(lept_value* v, size_t capacity) {
    assert(v != NULL);
    lept_free(v);
    v->type = LEPT_OBJECT;
    v->o.size = 0;
    v->o.capacity = capacity;
    v->o.m = capacity > 0 ? (lept_member*)malloc(capacity * sizeof(lept_member)) : NULL;
}

size_t lept_get_object_size(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    return v->o.size;
}
size_t lept_get_object_capacity(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    return v->o.capacity;
}

void lept_reserve_object(lept_value* v, size_t capacity) {
    /* todo */
}
void lept_shrink_object(lept_value* v) {
    /* todo */
}
void lept_clear_object(lept_value* v) {
    /* todo */
}

const char* lept_get_object_key(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index < v->o.size);
    return v->o.m[index].k;
}

size_t lept_get_object_key_length(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index < v->o.size);
    return v->o.m[index].klen;
}

lept_value* lept_get_object_value(lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index < v->o.size);
    return &v->o.m[index].v;
}

size_t lept_find_object_index(const lept_value* v, const char* key, size_t klen) {
    /* todo */
}

lept_value* lept_find_object_value(lept_value* v, const char* key, size_t klen) {
    /* todo */
}

lept_value* lept_set_object_value(lept_value* v, const char* key, size_t klen) {
    /* todo */
}

void lept_remove_object_value(lept_value* v, size_t index) {
    /* todo */
}
