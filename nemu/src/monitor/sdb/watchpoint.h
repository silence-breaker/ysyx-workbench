#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#define NR_WP 32

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;
  char expr[32];
  word_t val;
} WP;

void init_wp_pool(void);
WP* new_wp(void);
void free_wp(WP* wp);
WP* get_wp_head(void);
#endif