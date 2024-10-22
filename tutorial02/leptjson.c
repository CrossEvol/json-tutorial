#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL, strtod() */
#include <errno.h>
#include <stdio.h>
#include <math.h>

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)

#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')

#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')

#define FALSE 0

#define TRUE 1

#define UNKNOWN 2

typedef struct {
    const char* json;
}lept_context;

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, const int length,  lept_type type) {
    EXPECT(c, literal[0]);
    for (size_t i = 1; i < length; i++)
    {
        if (c->json[i-1] != literal[i]) return LEPT_PARSE_INVALID_VALUE;
    }
    c->json += length - 1;
    v->type = type;
    return LEPT_PARSE_OK;
}

static int lept_parse_true(lept_context* c, lept_value* v) {
    return lept_parse_literal(c, v, "true", 4, LEPT_TRUE);
}

static int lept_parse_false(lept_context* c, lept_value* v) {
    return lept_parse_literal(c,v,"false",5,LEPT_FALSE);
}

static int lept_parse_null(lept_context* c, lept_value* v) {
    return lept_parse_literal(c, v, "null", 4, LEPT_NULL);
}

enum {
    NUMBER_BEGIN,
    NUMBER_SYMBOL,
    NUMBER_ZERO_OR_OTHER,
    NUMBER_ZERO,
    NUMBER_DIGIT,
    NUMBER_DOT,
    NUMBER_EXPONENT,
    NUMBER_END,
};

int validateNumberText(lept_context* c)
{
    int hasDot = FALSE;
    int hasExponent = FALSE;
    const char* p = c->json;

    int phase = NUMBER_BEGIN;
    while (phase != NUMBER_END)
    {
        switch(phase)
        {
            case NUMBER_BEGIN:
                phase = NUMBER_SYMBOL;
                ;
                break;
            case NUMBER_SYMBOL:
                if (*p == '-')
                {
                    ++p;
                    phase = NUMBER_ZERO_OR_OTHER;
                }
                else if (ISDIGIT(*p))
                {
                    phase = NUMBER_ZERO_OR_OTHER;
                }
                else
                {
                    return FALSE;
                }
                ;
                break;
            case NUMBER_DIGIT:
                for (p++; ISDIGIT(*p); p++);
                if (*p == '.')
                {
                    if (hasDot)
                    {
                        return FALSE;
                    }
                    else
                    {
                        ++p;
                        phase = NUMBER_DOT;
                    }
                }
                else if (*p == 'e' || *p == 'E')
                {
                    if (hasExponent)
                    {
                        return FALSE;
                    }
                    else
                    {
                        ++p;
                        phase = NUMBER_EXPONENT;
                    }
                }
                else if (*p == '\0')
                {
                    phase = NUMBER_END;
                }
                else
                {
                    return FALSE;
                }
                break;
            case NUMBER_ZERO_OR_OTHER:
                if (*p == '0')
                {
                    ++p;
                    if (*p == '.')
                    {
                        ++p;
                        phase = NUMBER_DOT;
                    }
                    else if (*p == '\0')
                    {
                        phase = NUMBER_END;
                    }
                    else
                    {
                        return UNKNOWN;
                    }
                }
                else if (ISDIGIT1TO9(*p))
                {
                    phase = NUMBER_DIGIT;
                }
                else
                {
                    return FALSE;
                }
                ;
                break;
            case NUMBER_DOT:
                hasDot = TRUE;
                if (ISDIGIT(*p))
                {
                    phase = NUMBER_DIGIT;
                }
                else
                {
                    return FALSE;
                }
                ;
                break;
            case NUMBER_EXPONENT:
                hasExponent = TRUE;
                if (*p == '+' || *p == '-')
                {
                    ++p;
                    phase = NUMBER_DIGIT;
                }
                else if (ISDIGIT(*p))
                {
                    phase = NUMBER_DIGIT;
                }
                else
                {
                    return FALSE;
                }
                ;
                break;
            case NUMBER_END:
                ;
                break;
            default: 
                return FALSE;
                ;
                break;
        };
        
    }
    return TRUE;
}

static int lept_parse_number(lept_context* c, lept_value* v) {
    char* end;

    /* \TODO validate number */
    int flag = validateNumberText(c);
    if (flag == FALSE)
    {
        v->type = LEPT_NULL;
        return LEPT_PARSE_INVALID_VALUE;
    }
    if (flag == UNKNOWN)
    {
        v->type = LEPT_NULL;
        return LEPT_PARSE_ROOT_NOT_SINGULAR;
    }

    errno = 0;
    v->n = strtod(c->json, &end);
    if (errno == ERANGE && (v->n == HUGE_VAL || v->n == -HUGE_VAL)) {
        return LEPT_PARSE_NUMBER_TOO_BIG;
    }
    if (c->json == end)
        return LEPT_PARSE_INVALID_VALUE;
    c->json = end;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't':  return lept_parse_true(c, v);
        case 'f':  return lept_parse_false(c, v);
        case 'n':  return lept_parse_null(c, v);
        default:   return lept_parse_number(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->n;
}
