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

#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include <memory/vaddr.h>
#include <memory/paddr.h>
#include "watchpoint.h"
static int is_batch_mode = false;

void init_regex();
void init_wp_pool();
WP* new_wp();
void free_wp(WP* wp);
/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  return -1;
}

static int cmd_help(char *args);

static int cmd_si(char *args){
  int n = 1;
  if (args) {
    n = atoi(args);//把字符串转成数字
  }
  cpu_exec(n);//单步执行n步
  return 0;
}

static int cmd_info(char *args){
  switch (args[0]) {
    case 'r': isa_reg_display(); break;// 打印寄存器信息
    //case 'w': wp_display(); break;// 打印监视点信息
    case 'w': {
      WP* wp = get_wp_head();
      if (wp == NULL) {
        printf("No watchpoints.\n");
      }
      while (wp != NULL) {
        printf("Watchpoint %d: %s = 0x%08x\n", wp->NO, wp->expr, wp->val);
        wp = wp->next;
      }
      break;
    }
    default: printf("Unknown subcommand '%c'\n", args[0]);
  }
  return 0;
}

static int cmd_x(char *args){
  // 输入 x N EXPR
  char *N_str = strtok(args, " ");//获取第一个参数N
  char *EXPR_str = strtok(NULL, " ");//获取第二个参数EXPR
  //因为还没有实现表达式求值功能，所以先规定EXPR只能是一个十六进制数，后续再完善表达式求值功能
  if (N_str == NULL || EXPR_str == NULL) {
    printf("Usage: x N EXPR\n");
    return 0;
  }
  int N = atoi(N_str);//把字符串转成数字
  //word_t addr = expr(EXPR_str, NULL);//计算表达式的值，得到地址
  word_t addr = strtoul(EXPR_str, NULL, 16);//把十六进制字符串转成数字
  for (int i = 0; i < N; i++) {
    // 以十六进制形式输出连续的N个4字节
    printf("0x%08x: 0x%08x\n", addr + i * 4, vaddr_read(addr + i * 4, 4));
  }
  return 0;
}

static int cmd_p(char *args){
  bool success = true;
  word_t result = expr(args, &success);
  if (success) {
    printf("0x%08x\n", result);
    return 0;
  }
  else {
    return 0;
  }
};

static int cmd_w(char *args){
  WP* wp = new_wp();
  strcpy(wp->expr, args);
  bool success = true;
  wp->val = expr(args, &success);
  if (!success) {
    printf("Invalid expression for watchpoint: %s\n", args);
    free_wp(wp);
    return 0;
  }
  printf("Watchpoint %d: %s = 0x%08x\n", wp->NO, wp->expr, wp->val);
  return 0;
}

static int cmd_d(char* args){
  int wp_no = atoi(args);
  WP* wp = get_wp_head();
  while (wp != NULL) {
    if (wp->NO == wp_no) {
      free_wp(wp);
      return 0;
    }
    wp = wp->next;
  }
  printf("Watchpoint %d not found!\n", wp_no);
  return 0;
}

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Step into instruction, usage: si [N]", cmd_si },
  { "info", "Print the information of registers or watchpoints, usage: info SUBCMD\nSUBCMD can be 'r' for registers and 'w' for watchpoints", cmd_info },
  { "x", "Scan the memory, usage: x N EXPR\nThis command evaluates the expression EXPR, treating the result as a memory address. It then outputs N consecutive 4-byte values in hexadecimal format, starting from that address.", cmd_x },
  { "p", "Evaluate the expression EXPR and print its value, usage: p EXPR\nThis command evaluates the expression EXPR and prints its value in hexadecimal format.", cmd_p },
  { "w", "Set a watchpoint at the specified address, usage: w EXPR\nThis command sets a watchpoint at the given expression.", cmd_w },
  { "d", "Delete a watchpoint, usage: d WP_NO\nThis command deletes the watchpoint with the specified number.", cmd_d },

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}



void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
