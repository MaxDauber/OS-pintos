#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

struct thread_node
{
  tid_t tid;
  struct list_elem elem;
  
  bool parent_waiting;
  int exit_stat;
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct thread_node* get_child (tid_t tid, struct thread *cur_thread);
void free_resources (struct thread *t);

#endif
