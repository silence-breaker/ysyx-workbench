/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/
#include <stdbool.h>//C99标准引入的头文件，提供了bool类型和true/false常量
#include <stdint.h>
#include <stdlib.h>
#include <isa.h>
#include <memory/vaddr.h>
#include <memory/paddr.h>
#include "expr.h"
/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
/*
符号	作用
\d	任意数字
\w	字母 / 数字 / 下划线
\s	空格、换行
.	任意单个字符
*	前面内容出现 0 次 / 多次
+	前面内容出现 1 次 / 多次
?	前面内容可选（0 或 1 次）
{n}	固定 n 位
^	开头
$	结尾
[]	范围，如 [0-9a-z]

*/
enum {
  TK_NOTYPE = 256, TK_NEQ,TK_EQ, TK_NUM, TK_NEG, TK_AND, TK_DEREF, TK_REG, TK_HEX

  /* TODO: Add more token types */

};
//enum里面第一个元素赋值之后，后续的元素自动赋值+1

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {"[ \t]+", TK_NOTYPE},       // 空格（必须第一）

  {"0[xX][0-9a-fA-F]+", TK_HEX}, // 十六进制（必须在数字前）
  {"[0-9]+", TK_NUM},            // 十进制数字

  {"\\$[a-zA-Z0-9]+", TK_REG},   // 寄存器

  {"==", TK_EQ},                 // 双字符运算符必须放前面
  {"!=", TK_NEQ},
  {"&&", TK_AND},

  {"\\+", '+'},                  // 单字符运算符
  {"-", '-'},
  {"\\*", '*'},
  {"/", '/'},
  {"\\(", '('},
  {"\\)", ')'},
  // 负数的处理无法写到rules里面，在make_token函数里面处理
};

#define NR_REGEX ARRLEN(rules)
//这个数组用来存放编译后的正则
static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);//生成错误码，如果没错ret=0，re数组里面存放编译后的机器码
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);//把错误码翻译成错误信息
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;
//attribute((used))
//这个属性告诉编译器，即使该数组在代码中看起来未被使用，也不要优化掉它 。数组初始化为空
Token tokens[100] __attribute__((used)) = {};
int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;
  //这个结构体用来存储正则匹配的结果，包含两个成员：rm_so和rm_eo，分别表示匹配子串在原字符串中的起始位置和结束位置的偏移量。

  nr_token = 0;
//这里要求pmatch.rm_so == 0，表示匹配必须从当前字符串的起始位置开始，否则就不算匹配成功。这是为了确保每次匹配都是从字符串的当前位置开始，而不是在字符串的其他位置找到一个匹配。
  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;//因为你rm_so是0，所以匹配长度就是rm_eo

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);
          //%.*s(substr_len, substr_start)表示输出substr_start指向的字符串，但最多输出substr_len个字符。这种格式说明符允许我们在输出字符串时指定一个最大长度，防止输出过长的字符串。
        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          case TK_NUM:
            tokens[nr_token].type = TK_NUM;
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            //strncpy函数将substr_start指向的字符串复制到tokens[nr_token].str中，复制的长度为substr_len。这样就把匹配到的数字字符串存储在token的str字段中。
            tokens[nr_token].str[substr_len] = '\0';
            nr_token++;
            break;
          case TK_NOTYPE: break;
          case '+': case '-': case '*': case '/': case '(': case ')':
            tokens[nr_token].type = rules[i].token_type;
            tokens[nr_token].str[0] = rules[i].token_type;
            tokens[nr_token].str[1] = '\0';
            nr_token++;
            break;
          case TK_EQ:
            tokens[nr_token].type = TK_EQ;
            tokens[nr_token].str[0] = '=';
            tokens[nr_token].str[1] = '=';
            tokens[nr_token].str[2] = '\0';
            nr_token++;
            break;
          case TK_NEQ:
            tokens[nr_token].type = TK_NEQ;
            tokens[nr_token].str[0] = '!';
            tokens[nr_token].str[1] = '=';
            tokens[nr_token].str[2] = '\0';
            nr_token++;
            break;
          case TK_AND:
            tokens[nr_token].type = TK_AND;
            tokens[nr_token].str[0] = '&';
            tokens[nr_token].str[1] = '&';
            tokens[nr_token].str[2] = '\0';
            nr_token++;
            break;
          case TK_REG:
            tokens[nr_token].type = TK_REG;
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            nr_token++;
            break;
          case TK_HEX:
            tokens[nr_token].type = TK_HEX;
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            nr_token++;
            break;
          default:
            assert(0);//拦截非预期情况
        }
        //处理TK_NEG情况
        if (rules[i].token_type == '-' && (nr_token == 1 || (tokens[nr_token - 2].type != TK_NUM && tokens[nr_token - 2].type != ')'))) {
          tokens[nr_token - 1].type = TK_NEG;
        }

        //处理TK_DEREF情况
        if (rules[i].token_type == '*' && (nr_token == 1 || (tokens[nr_token - 2].type != TK_NUM && tokens[nr_token - 2].type != ')' && tokens[nr_token - 2].type != TK_REG))) {
          tokens[nr_token - 1].type = TK_DEREF;
        }

        break;//只要识别到一个token，就跳出for循环，继续识别下一个token
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");//这里%*.s输出position个空格，然后再输出一个^来指出出错的位置
      return false;
    }
  }

  return true;
}
//检查括号是否合法，要求括号必须成对出现，并且每个左括号都必须有一个对应的右括号，且它们之间不能有未匹配的括号。
bool check_parentheses(int p, int q) {
  int count = 0;
  for (int i = p ; i < q; i ++) {
    if (tokens[i].type == '(') {
      count ++;
    }
    else if (tokens[i].type == ')') {
      if (count == 0) {
        return false;
      }
      count --;
    }
  }

  return count == 0;
}
//需要把中缀表达式变成后缀表达式(TK_HEX和TK_REG段都做处理)
int get_priority(int op) {
  switch (op) {
    case '+': case '-': return 3;
    case '*': case '/': return 4;
    case '(': return 0;
    case TK_NEG: return 5;
    case TK_DEREF: return 5;
    case TK_EQ: return 2;
    case TK_NEQ: return 2;
    case TK_AND: return 1;
    default: assert(0);
  }
}//get_priority(tokens[i].type)

int op_top = 0;
int out_top = 0;
Token op_stack[32];//token.type是int
Token out_stack[100];//存储最后的后缀表达式

void push_op(Token op) {
  op_stack[op_top++] = op;
}

void push_out(Token out) {
  out_stack[out_top++] = out;
}

Token pop_op() {
  return op_stack[--op_top];
}

Token pop_out() {
  return out_stack[--out_top];
}
//把中缀表达式改成后缀表达式
void infix_to_postfix(){
  for (int i = 0; i < nr_token; i ++) {
    if (tokens[i].type == TK_NUM) {
      push_out(tokens[i]);
    }
    else if (tokens[i].type == TK_HEX) {
      push_out(tokens[i]);
    }
    else if (tokens[i].type == TK_REG) {
      push_out(tokens[i]);
    }
    else if (tokens[i].type == '(') {
      push_op(tokens[i]);
    }
    else if (tokens[i].type == ')') {
      while (op_stack[op_top - 1].type != '(') {
        push_out(pop_op());
      }
      pop_op();
    }
    else {
      while (op_top > 0 && get_priority(op_stack[op_top - 1].type) >= get_priority(tokens[i].type)) {
        push_out(pop_op());
      }
      push_op(tokens[i]);
    }
  }

  while (op_top > 0) {
    push_out(pop_op());
  }
}

int cal_postfix() {
  int result[100];
  int res_top = 0;
  printf("out_top is %d\n", out_top);
  //else只能跟和它最近的if配对，所以第一个if必须要加continue
  for (int i = 0; i < out_top; i ++) {
    if (out_stack[i].type == TK_NEG){
      if (res_top == 0) {
        printf("Invalid expression: unary minus operator has no operand\n");
        assert(0);
      }
      result[res_top - 1] = -result[res_top - 1];
    }
    else if (out_stack[i].type == TK_DEREF) {
      if (res_top == 0) {
        printf("Invalid expression: dereference operator has no operand\n");
        assert(0);
      }
      result[res_top - 1] = vaddr_read(result[res_top - 1], 4);
    }
    else if (out_stack[i].type == TK_NUM) {
      result[res_top++] = atoi(out_stack[i].str);
    }
    else if (out_stack[i].type == TK_HEX) {
      result[res_top++] = strtoul(out_stack[i].str, NULL, 16);
    }
    else if (out_stack[i].type == TK_REG) {
      bool success = true;
      word_t reg_val = isa_reg_str2val(out_stack[i].str + 1, &success);
      if (!success) {
        printf("Unknown register '%s'\n", out_stack[i].str);
        return 0;
      }
      result[res_top++] = reg_val;
    }
    else if (out_stack[i].type == TK_EQ || out_stack[i].type == TK_NEQ || out_stack[i].type == TK_AND) {
      int b = result[--res_top];
      int a = result[--res_top];
      switch (out_stack[i].type) {
        case TK_EQ: result[res_top++] = (a == b); break;
        case TK_NEQ: result[res_top++] = (a != b); break;
        case TK_AND: result[res_top++] = (a && b); break;
        default: assert(0);
      }
    }
    else if (out_stack[i].type == '+' || out_stack[i].type == '-' || out_stack[i].type == '*' || out_stack[i].type == '/') {
        int b = result[--res_top];
        int a = result[--res_top];
        switch (out_stack[i].type) {
          case '+': result[res_top++] = a + b; break;
          case '-': result[res_top++] = a - b; break;
          case '*': result[res_top++] = a * b; break;
          case '/': result[res_top++] = a / b; break;
          default: printf("Unknown operator: %d\n", out_stack[i].type);
                   assert(0);
        }
    }
  }
  return result[0];
}

 

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    printf("The regular expression detection is not up to standard\n");
    return 0;
  }
  if (check_parentheses(0, nr_token) == false) {
    *success = false;
    printf("The brackets are not legal\n");
    return 0;
  }
  /* TODO: Insert codes to evaluate the expression. */
  *success = true;
  for (int i = 0; i < nr_token; i ++) {
    printf("tokens[%d]: type = %d, str = %s\n", i, tokens[i].type, tokens[i].str);
  }
  
  infix_to_postfix();
  for (int i = 0; i < out_top; i ++) {
    printf("out_stack[%d] = %s\n", i, out_stack[i].str);
  }
  word_t result = (word_t)cal_postfix();
  op_top = 0;
  out_top = 0;
  return result;
}
