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

#include "sdb.h"
#include "watchpoint.h"
static WP wp_pool[NR_WP] = {};
//head用于组织使用中的监视点结构，free_用于组织空闲的监视点结构
static WP *head = NULL, *free_ = NULL;
//初始化监视点池
void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */
//获取头节点
WP* get_wp_head() {
  return head;
}
//new_wp()从free_链表中返回一个空闲的监视点结构
WP* new_wp(){
  if (free_ == NULL) {
    printf("No free watchpoint!\n");
    assert(0);
  }
  WP* new_wp = free_;
  free_ = free_->next;
  new_wp->next = head;
  head = new_wp;
  return new_wp;
}
//free_wp()将一个监视点结构从head链表中删除，并将其加入free_链表中
void free_wp(WP* wp){
  WP* prev = NULL;
  WP* curr = head;
  while (curr != NULL) {
    if (curr == wp) {
      if (prev == NULL) {
        head = curr->next;
      }
      else {
        prev->next = curr->next;
      }
      curr->next = free_;
      free_ = curr;
      printf("Watchpoint %d deleted!\n", wp->NO);
      return;
    }
    prev = curr;
    curr = curr->next;
  }
  printf("Watchpoint not found!\n");
  assert(0);
}