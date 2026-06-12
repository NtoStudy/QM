#ifndef CALCULATE_H
#define CALCULATE_H

/*
 * calculate.h
 * 四则混合表达式计算引擎 - 头文件
 * 功能：提供递归下降语法分析器对外调用接口
 * 支持：整数 +-*/ () 混合运算，自动校验语法与运行时错误
 */

/**
 * @brief 计算四则混合表达式
 * @param expr  输入表达式字符串
 * @param result 存储计算结果/错误提示
 */
void calculate(const char *expr, char *result);

#endif /* CALCULATE_H */
