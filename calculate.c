/*
 * calculate.c
 * 递归下降语法分析器，计算 int 四则混合表达式
 *
 * 语法:
 *   expr    → term (('+' | '-') term)*
 *   term    → factor (('*' | '/') factor)*
 *   factor  → INTEGER | '(' expr ')'
 */

#include "calculate.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

/* ---- 分析器内部状态 ---- */
static const char *s;      /* 当前正在扫描的字符串 */
static int        pos;     /* 当前字符位置 */
static int        err;     /* 非零表示分析过程出错 */

static void skip_space(void) {
    while (s[pos] == ' ') pos++;
}

static int parse_expr(void);

/* factor → INTEGER | '(' expr ')' */
static int parse_factor(void) {
    skip_space();
    if (s[pos] == '(') {
        pos++;               /* 跳过 '(' */
        int val = parse_expr();
        skip_space();
        if (s[pos] == ')')
            pos++;           /* 跳过 ')' */
        else
            err = 1;         /* 缺少右括号 */
        return val;
    }

    if (!isdigit((unsigned char)s[pos])) {
        err = 1;             /* 既不是数字也不是 '('，非法字符 */
        return 0;
    }

    int val = 0;
    while (isdigit((unsigned char)s[pos])) {
        val = val * 10 + (s[pos] - '0');
        pos++;
    }
    return val;
}

/* term → factor (('*' | '/') factor)* */
static int parse_term(void) {
    int val = parse_factor();
    while (!err) {
        skip_space();
        char op = s[pos];
        if (op != '*' && op != '/')
            break;
        pos++;               /* 跳过运算符 */
        int rhs = parse_factor();
        if (op == '*')
            val *= rhs;
        else if (rhs == 0) {
            err = 1;         /* 除零 */
            return 0;
        } else
            val /= rhs;
    }
    return val;
}

/* expr → term (('+' | '-') term)* */
static int parse_expr(void) {
    int val = parse_term();
    while (!err) {
        skip_space();
        char op = s[pos];
        if (op != '+' && op != '-')
            break;
        pos++;
        int rhs = parse_term();
        if (op == '+')
            val += rhs;
        else
            val -= rhs;
    }
    return val;
}

/* ---- 公开接口 ---- */
void calculate(const char *expr, char *result) {
    s   = expr;
    pos = 0;
    err = 0;

    int val = parse_expr();
    skip_space();

    /* 如果还有未处理的字符，或者出错 */
    if (err || s[pos] != '\0')
        sprintf(result, "error");
    else
        sprintf(result, "%d", val);
}
