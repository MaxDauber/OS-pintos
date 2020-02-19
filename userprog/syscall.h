#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "list.h"

void syscall_init (void);

struct file_node * find_file_node (int fd);
void remove_all_file_node (struct list *list_ptr);

struct lock file_sys_lock;

struct file_node
{
    int fd;
    struct file *file;
    struct list_elem elem;
};

void exit (int status);

#endif /* userprog/syscall.h */
