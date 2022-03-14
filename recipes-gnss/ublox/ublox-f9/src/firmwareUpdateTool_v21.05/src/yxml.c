/*
  Copyright (c) 2013-2014 Yoran Heling

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "yxml.h"
#include <string.h>

typedef enum {
    YXMLS_string,
    YXMLS_attr0,
    YXMLS_attr1,
    YXMLS_attr2,
    YXMLS_attr3,
    YXMLS_attr4,
    YXMLS_cd0,
    YXMLS_cd1,
    YXMLS_cd2,
    YXMLS_comment0,
    YXMLS_comment1,
    YXMLS_comment2,
    YXMLS_comment3,
    YXMLS_comment4,
    YXMLS_dt0,
    YXMLS_dt1,
    YXMLS_dt2,
    YXMLS_dt3,
    YXMLS_dt4,
    YXMLS_elem0,
    YXMLS_elem1,
    YXMLS_elem2,
    YXMLS_elem3,
    YXMLS_enc0,
    YXMLS_enc1,
    YXMLS_enc2,
    YXMLS_enc3,
    YXMLS_etag0,
    YXMLS_etag1,
    YXMLS_etag2,
    YXMLS_init,
    YXMLS_le0,
    YXMLS_le1,
    YXMLS_le2,
    YXMLS_le3,
    YXMLS_lee1,
    YXMLS_lee2,
    YXMLS_leq0,
    YXMLS_misc0,
    YXMLS_misc1,
    YXMLS_misc2,
    YXMLS_misc2a,
    YXMLS_misc3,
    YXMLS_pi0,
    YXMLS_pi1,
    YXMLS_pi2,
    YXMLS_pi3,
    YXMLS_pi4,
    YXMLS_std0,
    YXMLS_std1,
    YXMLS_std2,
    YXMLS_std3,
    YXMLS_ver0,
    YXMLS_ver1,
    YXMLS_ver2,
    YXMLS_ver3,
    YXMLS_xmldecl0,
    YXMLS_xmldecl1,
    YXMLS_xmldecl2,
    YXMLS_xmldecl3,
    YXMLS_xmldecl4,
    YXMLS_xmldecl5,
    YXMLS_xmldecl6,
    YXMLS_xmldecl7
} yxml_state_t;


#define yxml_isChar(c) 1
/* 0xd should be part of SP, too, but yxml_parse() already normalizes that into 0xa */
#define yxml_isSP(c) (c == 0x20 || c == 0x09 || c == 0x0a)
#define yxml_isAlpha(c) ((c|32)-'a' < 26)
#define yxml_isNum(c) (c-'0' < 10)
#define yxml_isHex(c) (yxml_isNum(c) || (c|32)-'a' < 6)
#define yxml_isEncName(c) (yxml_isAlpha(c) || yxml_isNum(c) || c == '.' || c == '_' || c == '-')
#define yxml_isNameStart(c) (yxml_isAlpha(c) || c == ':' || c == '_' || c >= 128)
#define yxml_isName(c) (yxml_isNameStart(c) || yxml_isNum(c) || c == '-' || c == '.')
/* XXX: The valid characters are dependent on the quote char, hence the access to x->quote */
#define yxml_isAttValue(c) (yxml_isChar(c) && c != x->quote && c != '<' && c != '&')
/* Anything between '&' and ';', the yxml_ref* functions will do further
 * validation. Strictly speaking, this is "yxml_isName(c) || c == '#'", but
 * this parser doesn't understand entities with '.', ':', etc, anwyay.  */
#define yxml_isRef(c) (yxml_isNum(c) || yxml_isAlpha(c) || c == '#')

#define INTFROM5CHARS(a, b, c, d, e) ((((uint64_t)(a))<<32) | (((uint64_t)(b))<<24) | (((uint64_t)(c))<<16) | (((uint64_t)(d))<<8) | (uint64_t)(e))


/* Set the given char value to ch (0<=ch<=255).
 * This can't be done with simple assignment because char may be signed, and
 * unsigned-to-signed overflow is implementation defined in C. This function
 * /looks/ inefficient, but gcc compiles it down to a single movb instruction
 * on x86, even with -O0. */
static inline void yxml_setchar(char *dest, unsigned ch) {
    unsigned char _ch = ch;
    memcpy(dest, &_ch, 1);
}


/* Similar to yxml_setchar(), but will convert ch (any valid unicode point) to
 * UTF-8 and appends a '\0'. dest must have room for at least 5 bytes. */
static void yxml_setutf8(char *dest, unsigned ch) {
    if(ch <= 0x007F)
        yxml_setchar(dest++, ch);
    else if(ch <= 0x07FF) {
        yxml_setchar(dest++, 0xC0 | (ch>>6));
        yxml_setchar(dest++, 0x80 | (ch & 0x3F));
    } else if(ch <= 0xFFFF) {
        yxml_setchar(dest++, 0xE0 | (ch>>12));
        yxml_setchar(dest++, 0x80 | ((ch>>6) & 0x3F));
        yxml_setchar(dest++, 0x80 | (ch & 0x3F));
    } else {
        yxml_setchar(dest++, 0xF0 | (ch>>18));
        yxml_setchar(dest++, 0x80 | ((ch>>12) & 0x3F));
        yxml_setchar(dest++, 0x80 | ((ch>>6) & 0x3F));
        yxml_setchar(dest++, 0x80 | (ch & 0x3F));
    }
    *dest = 0;
}


static inline int yxml_datacontent(yxml_t *x, unsigned ch) {
    yxml_setchar(x->data, ch);
    x->data[1] = 0;
    return YXML_CONTENT;
}


static inline int yxml_datapi1(yxml_t *x, unsigned ch) {
    yxml_setchar(x->data, ch);
    x->data[1] = 0;
    return YXML_PICONTENT;
}


static inline int yxml_datapi2(yxml_t *x, unsigned ch) {
    x->data[0] = '?';
    yxml_setchar(x->data+1, ch);
    x->data[2] = 0;
    return YXML_PICONTENT;
}


static inline int yxml_datacd1(yxml_t *x, unsigned ch) {
    x->data[0] = ']';
    yxml_setchar(x->data+1, ch);
    x->data[2] = 0;
    return YXML_CONTENT;
}


static inline int yxml_datacd2(yxml_t *x, unsigned ch) {
    x->data[0] = ']';
    x->data[1] = ']';
    yxml_setchar(x->data+2, ch);
    x->data[3] = 0;
    return YXML_CONTENT;
}


static inline int yxml_dataattr(yxml_t *x, unsigned ch) {
    /* Normalize attribute values according to the XML spec section 3.3.3. */
    yxml_setchar(x->data, ch == 0x9 || ch == 0xa ? 0x20 : ch);
    x->data[1] = 0;
    return YXML_ATTRVAL;
}


static int yxml_pushstack(yxml_t *x, char **res, unsigned ch) {
    if(x->stacklen+2 >= x->stacksize)
        return YXML_ESTACK;
    x->stacklen++;
    *res = (char *)x->stack+x->stacklen;
    x->stack[x->stacklen] = ch;
    x->stacklen++;
    x->stack[x->stacklen] = 0;
    return YXML_OK;
}


static int yxml_pushstackc(yxml_t *x, unsigned ch) {
    if(x->stacklen+1 >= x->stacksize)
        return YXML_ESTACK;
    x->stack[x->stacklen] = ch;
    x->stacklen++;
    x->stack[x->stacklen] = 0;
    return YXML_OK;
}


static void yxml_popstack(yxml_t *x) {
    do
        x->stacklen--;
    while(x->stack[x->stacklen]);
}


static inline int yxml_elemstart  (yxml_t *x, unsigned ch) { return yxml_pushstack(x, &x->elem, ch); }
static inline int yxml_elemname   (yxml_t *x, unsigned ch) { return yxml_pushstackc(x, ch); }
static inline int yxml_elemnameend(yxml_t *x, unsigned ch) { return YXML_ELEMSTART; }


/* Also used in yxml_elemcloseend(), since this function just removes the last
 * element from the stack and returns ELEMEND. */
static int yxml_selfclose(yxml_t *x, unsigned ch) {
    yxml_popstack(x);
    if(x->stacklen) {
        x->elem = (char *)x->stack+x->stacklen-1;
        while(*(x->elem-1))
            x->elem--;
        return YXML_ELEMEND;
    }
    x->elem = (char *)x->stack;
    x->state = YXMLS_misc3;
    return YXML_ELEMEND;
}


static inline int yxml_elemclose(yxml_t *x, unsigned ch) {
    if(*((unsigned char *)x->elem) != ch)
        return YXML_ECLOSE;
    x->elem++;
    return YXML_OK;
}


static inline int yxml_elemcloseend(yxml_t *x, unsigned ch) {
    if(*x->elem)
        return YXML_ECLOSE;
    return yxml_selfclose(x, ch);
}


static inline int yxml_attrstart  (yxml_t *x, unsigned ch) { return yxml_pushstack(x, &x->attr, ch); }
static inline int yxml_attrname   (yxml_t *x, unsigned ch) { return yxml_pushstackc(x, ch); }
static inline int yxml_attrnameend(yxml_t *x, unsigned ch) { return YXML_ATTRSTART; }
static inline int yxml_attrvalend (yxml_t *x, unsigned ch) { yxml_popstack(x); return YXML_ATTREND; }


static inline int yxml_pistart  (yxml_t *x, unsigned ch) { return yxml_pushstack(x, &x->pi, ch); }
static inline int yxml_piname   (yxml_t *x, unsigned ch) { return yxml_pushstackc(x, ch); }
static inline int yxml_pinameend(yxml_t *x, unsigned ch) {
    return (x->pi[0]|32) == 'x' && (x->pi[1]|32) == 'm' && (x->pi[2]|32) == 'l' && !x->pi[3] ? YXML_ESYN : YXML_PISTART;
}
static inline int yxml_pivalend (yxml_t *x, unsigned ch) { yxml_popstack(x); x->pi = (char *)x->stack; return YXML_PIEND; }


static inline int yxml_refstart(yxml_t *x, unsigned ch) {
    memset(x->data, 0, sizeof(x->data));
    x->reflen = 0;
    return YXML_OK;
}


static int yxml_ref(yxml_t *x, unsigned ch) {
    if(x->reflen >= sizeof(x->data)-1)
        return YXML_EREF;
    yxml_setchar(x->data+x->reflen, ch);
    x->reflen++;
    return YXML_OK;
}


static int yxml_refend(yxml_t *x, int ret) {
    unsigned char *r = (unsigned char *)x->data;
    unsigned ch = 0;
    if(*r == '#') {
        if(r[1] == 'x')
            for(r += 2; yxml_isHex((unsigned)*r); r++)
                ch = (ch<<4) + (*r <= '9' ? *r-'0' : (*r|32)-'a' + 10);
        else
            for(r++; yxml_isNum((unsigned)*r); r++)
                ch = (ch*10) + (*r-'0');
        if(*r)
            ch = 0;
    } else {
        uint64_t i = INTFROM5CHARS(r[0], r[1], r[2], r[3], r[4]);
        ch =
            i == INTFROM5CHARS('l','t', 0,  0, 0) ? '<' :
            i == INTFROM5CHARS('g','t', 0,  0, 0) ? '>' :
            i == INTFROM5CHARS('a','m','p', 0, 0) ? '&' :
            i == INTFROM5CHARS('a','p','o','s',0) ? '\'':
            i == INTFROM5CHARS('q','u','o','t',0) ? '"' : 0;
    }

    /* Codepoints not allowed in the XML 1.1 definition of a Char */
    if(!ch || ch > 0x10FFFF || ch == 0xFFFE || ch == 0xFFFF || (ch-0xDFFF) < 0x7FF)
        return YXML_EREF;
    yxml_setutf8(x->data, ch);
    return ret;
}


static inline int yxml_refcontent(yxml_t *x, unsigned ch) { return yxml_refend(x, YXML_CONTENT); }
static inline int yxml_refattrval(yxml_t *x, unsigned ch) { return yxml_refend(x, YXML_ATTRVAL); }


void yxml_init(yxml_t *x, void *stack, size_t stacksize) {
    memset(x, 0, sizeof(*x));
    x->line = 1;
    x->stack = stack;
    x->stacksize = stacksize;
    *x->stack = 0;
    x->elem = x->pi = x->attr = (char *)x->stack;
    x->state = YXMLS_init;
}


yxml_ret_t yxml_parse(yxml_t *x, int _ch) {
    /* Ensure that characters are in the range of 0..255 rather than -126..125.
     * All character comparisons are done with positive integers. */
    unsigned ch = (unsigned)(_ch+256) & 0xff;
    if(!ch)
        return YXML_ESYN;
    x->total++;

    /* End-of-Line normalization, "\rX", "\r\n" and "\n" are recognized and
     * normalized to a single '\n' as per XML 1.0 section 2.11. XML 1.1 adds
     * some non-ASCII character sequences to this list, but we can only handle
     * ASCII here without making assumptions about the input encoding. */
    if(x->ignore == ch) {
        x->ignore = 0;
        return YXML_OK;
    }
    x->ignore = (ch == 0xd) * 0xa;
    if(ch == 0xa || ch == 0xd) {
        ch = 0xa;
        x->line++;
        x->byte = 0;
    }
    x->byte++;

    switch((yxml_state_t)x->state) {
    case YXMLS_string:
        if(ch == *x->string) {
            x->string++;
            if(!*x->string)
                x->state = x->nextstate;
            return YXML_OK;
        }
        break;
    case YXMLS_attr0:
        if(yxml_isName(ch))
            return yxml_attrname(x, ch);
        if(yxml_isSP(ch)) {
            x->state = YXMLS_attr1;
            return yxml_attrnameend(x, ch);
        }
        if(ch == (unsigned char)'=') {
            x->state = YXMLS_attr2;
            return yxml_attrnameend(x, ch);
        }
        break;
    case YXMLS_attr1:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'=') {
            x->state = YXMLS_attr2;
            return YXML_OK;
        }
        break;
    case YXMLS_attr2:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'\'' || ch == (unsigned char)'"') {
            x->state = YXMLS_attr3;
            x->quote = ch;
            return YXML_OK;
        }
        break;
    case YXMLS_attr3:
        if(yxml_isAttValue(ch))
            return yxml_dataattr(x, ch);
        if(ch == (unsigned char)'&') {
            x->state = YXMLS_attr4;
            return yxml_refstart(x, ch);
        }
        if(x->quote == ch) {
            x->state = YXMLS_elem2;
            return yxml_attrvalend(x, ch);
        }
        break;
    case YXMLS_attr4:
        if(yxml_isRef(ch))
            return yxml_ref(x, ch);
        if(ch == (unsigned char)'\x3b') {
            x->state = YXMLS_attr3;
            return yxml_refattrval(x, ch);
        }
        break;
    case YXMLS_cd0:
        if(ch == (unsigned char)']') {
            x->state = YXMLS_cd1;
            return YXML_OK;
        }
        if(yxml_isChar(ch))
            return yxml_datacontent(x, ch);
        break;
    case YXMLS_cd1:
        if(ch == (unsigned char)']') {
            x->state = YXMLS_cd2;
            return YXML_OK;
        }
        if(yxml_isChar(ch)) {
            x->state = YXMLS_cd0;
            return yxml_datacd1(x, ch);
        }
        break;
    case YXMLS_cd2:
        if(ch == (unsigned char)']')
            return yxml_datacontent(x, ch);
        if(ch == (unsigned char)'>') {
            x->state = YXMLS_misc2;
            return YXML_OK;
        }
        if(yxml_isChar(ch)) {
            x->state = YXMLS_cd0;
            return yxml_datacd2(x, ch);
        }
        break;
    case YXMLS_comment0:
        if(ch == (unsigned char)'-') {
            x->state = YXMLS_comment1;
            return YXML_OK;
        }
        break;
    case YXMLS_comment1:
        if(ch == (unsigned char)'-') {
            x->state = YXMLS_comment2;
            return YXML_OK;
        }
        break;
    case YXMLS_comment2:
        if(ch == (unsigned char)'-') {
            x->state = YXMLS_comment3;
            return YXML_OK;
        }
        if(yxml_isChar(ch))
            return YXML_OK;
        break;
    case YXMLS_comment3:
        if(ch == (unsigned char)'-') {
            x->state = YXMLS_comment4;
            return YXML_OK;
        }
        if(yxml_isChar(ch)) {
            x->state = YXMLS_comment2;
            return YXML_OK;
        }
        break;
    case YXMLS_comment4:
        if(ch == (unsigned char)'>') {
            x->state = x->nextstate;
            return YXML_OK;
        }
        break;
    case YXMLS_dt0:
        if(ch == (unsigned char)'>') {
            x->state = YXMLS_misc1;
            return YXML_OK;
        }
        if(ch == (unsigned char)'\'' || ch == (unsigned char)'"') {
            x->state = YXMLS_dt1;
            x->quote = ch;
            x->nextstate = YXMLS_dt0;
            return YXML_OK;
        }
        if(ch == (unsigned char)'<') {
            x->state = YXMLS_dt2;
            return YXML_OK;
        }
        if(yxml_isChar(ch))
            return YXML_OK;
        break;
    case YXMLS_dt1:
        if(x->quote == ch) {
            x->state = x->nextstate;
            return YXML_OK;
        }
        if(yxml_isChar(ch))
            return YXML_OK;
        break;
    case YXMLS_dt2:
        if(ch == (unsigned char)'?') {
            x->state = YXMLS_pi0;
            x->nextstate = YXMLS_dt0;
            return YXML_OK;
        }
        if(ch == (unsigned char)'!') {
            x->state = YXMLS_dt3;
            return YXML_OK;
        }
        break;
    case YXMLS_dt3:
        if(ch == (unsigned char)'-') {
            x->state = YXMLS_comment1;
            x->nextstate = YXMLS_dt0;
            return YXML_OK;
        }
        if(yxml_isChar(ch)) {
            x->state = YXMLS_dt4;
            return YXML_OK;
        }
        break;
    case YXMLS_dt4:
        if(ch == (unsigned char)'\'' || ch == (unsigned char)'"') {
            x->state = YXMLS_dt1;
            x->quote = ch;
            x->nextstate = YXMLS_dt4;
            return YXML_OK;
        }
        if(ch == (unsigned char)'>') {
            x->state = YXMLS_dt0;
            return YXML_OK;
        }
        if(yxml_isChar(ch))
            return YXML_OK;
        break;
    case YXMLS_elem0:
        if(yxml_isName(ch))
            return yxml_elemname(x, ch);
        if(yxml_isSP(ch)) {
            x->state = YXMLS_elem1;
            return yxml_elemnameend(x, ch);
        }
        if(ch == (unsigned char)'/') {
            x->state = YXMLS_elem3;
            return yxml_elemnameend(x, ch);
        }
        if(ch == (unsigned char)'>') {
            x->state = YXMLS_misc2;
            return yxml_elemnameend(x, ch);
        }
        break;
    case YXMLS_elem1:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'/') {
            x->state = YXMLS_elem3;
            return YXML_OK;
        }
        if(ch == (unsigned char)'>') {
            x->state = YXMLS_misc2;
            return YXML_OK;
        }
        if(yxml_isNameStart(ch)) {
            x->state = YXMLS_attr0;
            return yxml_attrstart(x, ch);
        }
        break;
    case YXMLS_elem2:
        if(yxml_isSP(ch)) {
            x->state = YXMLS_elem1;
            return YXML_OK;
        }
        if(ch == (unsigned char)'/') {
            x->state = YXMLS_elem3;
            return YXML_OK;
        }
        if(ch == (unsigned char)'>') {
            x->state = YXMLS_misc2;
            return YXML_OK;
        }
        break;
    case YXMLS_elem3:
        if(ch == (unsigned char)'>') {
            x->state = YXMLS_misc2;
            return yxml_selfclose(x, ch);
        }
        break;
    case YXMLS_enc0:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'=') {
            x->state = YXMLS_enc1;
            return YXML_OK;
        }
        break;
    case YXMLS_enc1:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'\'' || ch == (unsigned char)'"') {
            x->state = YXMLS_enc2;
            x->quote = ch;
            return YXML_OK;
        }
        break;
    case YXMLS_enc2:
        if(yxml_isAlpha(ch)) {
            x->state = YXMLS_enc3;
            return YXML_OK;
        }
        break;
    case YXMLS_enc3:
        if(yxml_isEncName(ch))
            return YXML_OK;
        if(x->quote == ch) {
            x->state = YXMLS_xmldecl4;
            return YXML_OK;
        }
        break;
    case YXMLS_etag0:
        if(yxml_isNameStart(ch)) {
            x->state = YXMLS_etag1;
            return yxml_elemclose(x, ch);
        }
        break;
    case YXMLS_etag1:
        if(yxml_isName(ch))
            return yxml_elemclose(x, ch);
        if(yxml_isSP(ch)) {
            x->state = YXMLS_etag2;
            return yxml_elemcloseend(x, ch);
        }
        if(ch == (unsigned char)'>') {
            x->state = YXMLS_misc2;
            return yxml_elemcloseend(x, ch);
        }
        break;
    case YXMLS_etag2:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'>') {
            x->state = YXMLS_misc2;
            return YXML_OK;
        }
        break;
    case YXMLS_init:
        if(ch == (unsigned char)'\xef') {
            x->state = YXMLS_string;
            x->nextstate = YXMLS_misc0;
            x->string = (unsigned char *)"\xbb\xbf";
            return YXML_OK;
        }
        if(yxml_isSP(ch)) {
            x->state = YXMLS_misc0;
            return YXML_OK;
        }
        if(ch == (unsigned char)'<') {
            x->state = YXMLS_le0;
            return YXML_OK;
        }
        break;
    case YXMLS_le0:
        if(ch == (unsigned char)'!') {
            x->state = YXMLS_lee1;
            return YXML_OK;
        }
        if(ch == (unsigned char)'?') {
            x->state = YXMLS_leq0;
            return YXML_OK;
        }
        if(yxml_isNameStart(ch)) {
            x->state = YXMLS_elem0;
            return yxml_elemstart(x, ch);
        }
        break;
    case YXMLS_le1:
        if(ch == (unsigned char)'!') {
            x->state = YXMLS_lee1;
            return YXML_OK;
        }
        if(ch == (unsigned char)'?') {
            x->state = YXMLS_pi0;
            x->nextstate = YXMLS_misc1;
            return YXML_OK;
        }
        if(yxml_isNameStart(ch)) {
            x->state = YXMLS_elem0;
            return yxml_elemstart(x, ch);
        }
        break;
    case YXMLS_le2:
        if(ch == (unsigned char)'!') {
            x->state = YXMLS_lee2;
            return YXML_OK;
        }
        if(ch == (unsigned char)'?') {
            x->state = YXMLS_pi0;
            x->nextstate = YXMLS_misc2;
            return YXML_OK;
        }
        if(ch == (unsigned char)'/') {
            x->state = YXMLS_etag0;
            return YXML_OK;
        }
        if(yxml_isNameStart(ch)) {
            x->state = YXMLS_elem0;
            return yxml_elemstart(x, ch);
        }
        break;
    case YXMLS_le3:
        if(ch == (unsigned char)'!') {
            x->state = YXMLS_comment0;
            x->nextstate = YXMLS_misc3;
            return YXML_OK;
        }
        if(ch == (unsigned char)'?') {
            x->state = YXMLS_pi0;
            x->nextstate = YXMLS_misc3;
            return YXML_OK;
        }
        break;
    case YXMLS_lee1:
        if(ch == (unsigned char)'-') {
            x->state = YXMLS_comment1;
            x->nextstate = YXMLS_misc1;
            return YXML_OK;
        }
        if(ch == (unsigned char)'D') {
            x->state = YXMLS_string;
            x->nextstate = YXMLS_dt0;
            x->string = (unsigned char *)"OCTYPE";
            return YXML_OK;
        }
        break;
    case YXMLS_lee2:
        if(ch == (unsigned char)'-') {
            x->state = YXMLS_comment1;
            x->nextstate = YXMLS_misc2;
            return YXML_OK;
        }
        if(ch == (unsigned char)'[') {
            x->state = YXMLS_string;
            x->nextstate = YXMLS_cd0;
            x->string = (unsigned char *)"CDATA[";
            return YXML_OK;
        }
        break;
    case YXMLS_leq0:
        if(ch == (unsigned char)'x') {
            x->state = YXMLS_string;
            x->nextstate = YXMLS_xmldecl0;
            x->string = (unsigned char *)"ml";
            return YXML_OK;
        }
        if(yxml_isNameStart(ch)) {
            x->state = YXMLS_pi1;
            x->nextstate = YXMLS_misc1;
            return yxml_pistart(x, ch);
        }
        break;
    case YXMLS_misc0:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'<') {
            x->state = YXMLS_le0;
            return YXML_OK;
        }
        break;
    case YXMLS_misc1:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'<') {
            x->state = YXMLS_le1;
            return YXML_OK;
        }
        break;
    case YXMLS_misc2:
        if(ch == (unsigned char)'<') {
            x->state = YXMLS_le2;
            return YXML_OK;
        }
        if(ch == (unsigned char)'&') {
            x->state = YXMLS_misc2a;
            return yxml_refstart(x, ch);
        }
        if(yxml_isChar(ch))
            return yxml_datacontent(x, ch);
        break;
    case YXMLS_misc2a:
        if(yxml_isRef(ch))
            return yxml_ref(x, ch);
        if(ch == (unsigned char)'\x3b') {
            x->state = YXMLS_misc2;
            return yxml_refcontent(x, ch);
        }
        break;
    case YXMLS_misc3:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'<') {
            x->state = YXMLS_le3;
            return YXML_OK;
        }
        break;
    case YXMLS_pi0:
        if(yxml_isNameStart(ch)) {
            x->state = YXMLS_pi1;
            return yxml_pistart(x, ch);
        }
        break;
    case YXMLS_pi1:
        if(yxml_isName(ch))
            return yxml_piname(x, ch);
        if(ch == (unsigned char)'?') {
            x->state = YXMLS_pi4;
            return yxml_pinameend(x, ch);
        }
        if(yxml_isSP(ch)) {
            x->state = YXMLS_pi2;
            return yxml_pinameend(x, ch);
        }
        break;
    case YXMLS_pi2:
        if(ch == (unsigned char)'?') {
            x->state = YXMLS_pi3;
            return YXML_OK;
        }
        if(yxml_isChar(ch))
            return yxml_datapi1(x, ch);
        break;
    case YXMLS_pi3:
        if(ch == (unsigned char)'>') {
            x->state = x->nextstate;
            return yxml_pivalend(x, ch);
        }
        if(yxml_isChar(ch)) {
            x->state = YXMLS_pi2;
            return yxml_datapi2(x, ch);
        }
        break;
    case YXMLS_pi4:
        if(ch == (unsigned char)'>') {
            x->state = x->nextstate;
            return yxml_pivalend(x, ch);
        }
        break;
    case YXMLS_std0:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'=') {
            x->state = YXMLS_std1;
            return YXML_OK;
        }
        break;
    case YXMLS_std1:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'\'' || ch == (unsigned char)'"') {
            x->state = YXMLS_std2;
            x->quote = ch;
            return YXML_OK;
        }
        break;
    case YXMLS_std2:
        if(ch == (unsigned char)'y') {
            x->state = YXMLS_string;
            x->nextstate = YXMLS_std3;
            x->string = (unsigned char *)"es";
            return YXML_OK;
        }
        if(ch == (unsigned char)'n') {
            x->state = YXMLS_string;
            x->nextstate = YXMLS_std3;
            x->string = (unsigned char *)"o";
            return YXML_OK;
        }
        break;
    case YXMLS_std3:
        if(x->quote == ch) {
            x->state = YXMLS_xmldecl6;
            return YXML_OK;
        }
        break;
    case YXMLS_ver0:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'=') {
            x->state = YXMLS_ver1;
            return YXML_OK;
        }
        break;
    case YXMLS_ver1:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'\'' || ch == (unsigned char)'"') {
            x->state = YXMLS_string;
            x->quote = ch;
            x->nextstate = YXMLS_ver2;
            x->string = (unsigned char *)"1.";
            return YXML_OK;
        }
        break;
    case YXMLS_ver2:
        if(yxml_isNum(ch)) {
            x->state = YXMLS_ver3;
            return YXML_OK;
        }
        break;
    case YXMLS_ver3:
        if(yxml_isNum(ch))
            return YXML_OK;
        if(x->quote == ch) {
            x->state = YXMLS_xmldecl2;
            return YXML_OK;
        }
        break;
    case YXMLS_xmldecl0:
        if(yxml_isSP(ch)) {
            x->state = YXMLS_xmldecl1;
            return YXML_OK;
        }
        break;
    case YXMLS_xmldecl1:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'v') {
            x->state = YXMLS_string;
            x->nextstate = YXMLS_ver0;
            x->string = (unsigned char *)"ersion";
            return YXML_OK;
        }
        break;
    case YXMLS_xmldecl2:
        if(yxml_isSP(ch)) {
            x->state = YXMLS_xmldecl3;
            return YXML_OK;
        }
        if(ch == (unsigned char)'?') {
            x->state = YXMLS_xmldecl7;
            return YXML_OK;
        }
        break;
    case YXMLS_xmldecl3:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'?') {
            x->state = YXMLS_xmldecl7;
            return YXML_OK;
        }
        if(ch == (unsigned char)'e') {
            x->state = YXMLS_string;
            x->nextstate = YXMLS_enc0;
            x->string = (unsigned char *)"ncoding";
            return YXML_OK;
        }
        if(ch == (unsigned char)'s') {
            x->state = YXMLS_string;
            x->nextstate = YXMLS_std0;
            x->string = (unsigned char *)"tandalone";
            return YXML_OK;
        }
        break;
    case YXMLS_xmldecl4:
        if(yxml_isSP(ch)) {
            x->state = YXMLS_xmldecl5;
            return YXML_OK;
        }
        if(ch == (unsigned char)'?') {
            x->state = YXMLS_xmldecl7;
            return YXML_OK;
        }
        break;
    case YXMLS_xmldecl5:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'?') {
            x->state = YXMLS_xmldecl7;
            return YXML_OK;
        }
        if(ch == (unsigned char)'s') {
            x->state = YXMLS_string;
            x->nextstate = YXMLS_std0;
            x->string = (unsigned char *)"tandalone";
            return YXML_OK;
        }
        break;
    case YXMLS_xmldecl6:
        if(yxml_isSP(ch))
            return YXML_OK;
        if(ch == (unsigned char)'?') {
            x->state = YXMLS_xmldecl7;
            return YXML_OK;
        }
        break;
    case YXMLS_xmldecl7:
        if(ch == (unsigned char)'>') {
            x->state = YXMLS_misc1;
            return YXML_OK;
        }
        break;
    }
    return YXML_ESYN;
}


yxml_ret_t yxml_eof(yxml_t *x) {
    if(x->state != YXMLS_misc3)
        return YXML_EEOF;
    return YXML_OK;
}


/* vim: set noet sw=4 ts=4: */
