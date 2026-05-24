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

#include <common.h>

void init_monitor(int, char *[]);
void am_init_monitor();
void engine_start();
int is_exit_status_bad();
word_t expr(char *e, bool *success);
extern char *expr_test_file;
//所有没有函数体的函数声明，默认自带 extern 属性

void run_expr_test(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen failed");
        exit(1);
    }

    
    char line[100];
    char expr_buf[100];
    int line_cnt = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_cnt++;

        // 只去掉换行，不改动任何内容
        line[strcspn(line, "\n")] = 0;

        word_t expect;
        char expr_str[100];

        // 读取格式：结果 表达式
        if (sscanf(line, "%u %[^\n]", &expect, expr_str) != 2) {
            continue;
        }

        // 原样复制，和手动输入完全一致
        strcpy(expr_buf, expr_str);
        printf("Testing line %s\n",expr_buf);
        // 调用求值
        bool ok = true;
        word_t res = expr(expr_buf, &ok);
        printf("0x%08x\n", res);
        
        if (!ok) {
            printf("line %d error\n", line_cnt);
            fclose(fp);
            exit(1);
        }

        if (res != expect) {
            printf("FAIL line %d\nexpr: %s\nexpect: %u  got: %u\n",
                line_cnt, expr_buf, expect, res);
            fclose(fp);
            exit(1);
        }
    }

    fclose(fp);
    printf("All tests passed!\n");
    exit(0);
}

int main(int argc, char *argv[]) {

  /* Initialize the monitor. */
#ifdef CONFIG_TARGET_AM
  am_init_monitor();
#else
  init_monitor(argc, argv);
#endif
  if (expr_test_file) {
    run_expr_test(expr_test_file);
    return 0;
  }
  /* Start engine. */
  engine_start();

  return is_exit_status_bad();
}


