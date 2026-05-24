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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

static char buf[65536] = {};
static char code_buf[65536 + 128] = {};
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

void gen_num() {
  char num[32];
  sprintf(num, "%u", rand() % 1000);
  strcat(buf, num);
}

void gen(char c) {
  int len = strlen(buf);
  buf[len] = c;
  buf[len + 1] = '\0';
}

void gen_rand_op() {
  switch (rand() % 4) {
    case 0: gen('+'); break;
    case 1: gen('-'); break;
    case 2: gen('*'); break;
    case 3: gen('/'); break;
  }
}

void gen_rand_notype() {
  switch (rand() % 3) {
    case 0: gen(' '); break;
    case 1: gen('\t'); break;
    case 2: break;
  }
}

static void gen_rand_expr(int depth) {
  if (depth > 4) {
    gen_num();
    return;
  }

  switch (rand() % 4) {
    case 0:
      gen_num();
      gen_rand_notype();
      break;
    case 1:
      gen('(');
      
      gen_rand_expr(depth + 1);
      
      gen(')');
      
      break;
    case 2:
      gen_rand_expr(depth + 1);
      
      gen_rand_op();
      
      gen_rand_expr(depth + 1);
      
      break;
    case 3:
      gen('-');
      
      gen_rand_expr(depth + 1);
      
      break;
  }
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }

  int generated = 0;
  while (generated < loop) {
    buf[0] = '\0';          // 清空
    gen_rand_expr(0);       // 生成内部表达式

    // ==============================
    // ✅ 最外层包裹一对 ()
    // ==============================
    char final_buf[65536];
    sprintf(final_buf, "(%s)", buf);  // 👈 核心在这里

    // 用 final_buf 替换原来的 buf
    sprintf(code_buf, code_format, final_buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr 2>/dev/null");
    if (ret != 0) continue;

    ret = system("/tmp/.expr > /tmp/.result 2>/dev/null");
    if (ret != 0) continue;

    fp = fopen("/tmp/.result", "r");
    unsigned result;
    if (fscanf(fp, "%u", &result) != 1) {
      continue;
    }
    fclose(fp);

    // ==============================
    // ✅ 输出：结果 (表达式)
    // ==============================
    printf("%u %s\n", result, final_buf);

    generated++;
  }

  return 0;
}