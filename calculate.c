/*
 * calculate.c
 * 功能：基于递归下降语法分析器，实现整数四则混合表达式计算
 * 支持特性：整数、加减乘除运算、多层括号、表达式内空格过滤
 * 错误检测：非法字符、括号不匹配、除零异常、多余尾随字符
 *
 * 文法规则（自上而下递归下降）：
 *   expr    → term (('+' | '-') term)*   // 表达式：加减法组合项
 *   term    → factor (('*' | '/') factor)* // 项：乘除法组合因子
 *   factor  → INTEGER | '(' expr ')'    // 因子：整数 或 括号表达式
 *
 * 作者：XXX
 * 完成日期：2026-XX-XX
 */
#include "calculate.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

/* 语法分析器全局内部状态 */
static const char *s;      // 待解析的表达式字符串
static int        pos;     // 当前字符扫描位置
static int        err;     // 错误标记：非0表示解析/计算出错

/**
 * @brief 跳过表达式中的所有空格字符
 */
static void skip_space(void) {
    while (s[pos] == ' ') pos++;
}

// 前置声明：解析表达式函数
static int parse_expr(void);

/**
 * @brief 解析因子：数字 或 括号包裹的表达式
 * @return 解析得到的整数值
 */
static int parse_factor(void) {
    skip_space();
    // 匹配左括号，递归解析内部表达式
    if (s[pos] == '(') {
        pos++;               // 跳过左括号 '('
        int val = parse_expr();
        skip_space();
        // 校验右括号
        if (s[pos] == ')')
            pos++;           // 跳过右括号 ')'
        else
            err = 1;         // 错误：缺少右括号
        return val;
    }

    // 非数字、非左括号，判定为非法字符
    if (!isdigit((unsigned char)s[pos])) {
        err = 1;             // 错误：出现非法字符
        return 0;
    }

    // 解析连续数字，拼接为整数
    int val = 0;
    while (isdigit((unsigned char)s[pos])) {
        val = val * 10 + (s[pos] - '0');
        pos++;
    }
    return val;
}

/**
 * @brief 解析项：因子之间做乘除运算
 * @return 项的计算结果
 */
static int parse_term(void) {
    int val = parse_factor();
    while (!err) {
        skip_space();
        char op = s[pos];
        // 仅处理 * / 运算符
        if (op != '*' && op != '/')
            break;
        pos++;               // 跳过当前运算符
        int rhs = parse_factor();
        // 乘除计算 + 除零校验
        if (op == '*')
            val *= rhs;
        else if (rhs == 0) {
            err = 1;         // 错误：除数为0
            return 0;
        } else
            val /= rhs;
    }
    return val;
}

/**
 * @brief 解析表达式：项之间做加减运算
 * @return 表达式最终计算结果
 */
static int parse_expr(void) {
    int val = parse_term();
    while (!err) {
        skip_space();
        char op = s[pos];
        // 仅处理 + - 运算符
        if (op != '+' && op != '-')
            break;
        pos++;               // 跳过当前运算符
        int rhs = parse_term();
        // 加减计算
        if (op == '+')
            val += rhs;
        else
            val -= rhs;
    }
    return val;
}

/**
 * @brief 对外公开接口：解析并计算四则混合表达式
 * @param expr  输入的四则表达式字符串
 * @param result 输出结果缓冲区：正常返回数字，出错返回 "error"
 */
void calculate(const char *expr, char *result) {
    // 初始化解析状态
    s   = expr;
    pos = 0;
    err = 0;

    int val = parse_expr();
    skip_space();

    // 判定结果：存在错误 或 表达式有多余尾随字符，统一返回 error
    if (err || s[pos] != '\0')
        sprintf(result, "error");
    else
        sprintf(result, "%d", val);
}
