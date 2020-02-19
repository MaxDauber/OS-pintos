#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <stdio.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "lib/kernel/stdio.h"
#include "lib/stdio.h"
#include <syscall-nr.h>
#include "userprog/syscall.h"

#define CODE_SEG 0x08048000

typedef struct file file;
struct lock filesys_lock;

static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static void syscall_handler (struct intr_frame *f);

void check_ptr (const void *vaddr);
void check_read_ptr (const void *ptr);
void check_buffer(void* buffer, unsigned size);

int add_file_node(file *file);
void close_file (int fd);
struct file_node *find_file_node (int fd);
void remove_file_node (int fd);
void remove_all_file_node (struct list *list_ptr);

int write (int fd, void *buffer, uint32_t size);
void exit (int status);
int sys_read (int fd, void *buffer, unsigned size);
void close (int fd);
void halt (void);

tid_t exec (const char *cmd_line);
int sys_open (const char *file);
int filesize (int fd);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
void seek (int fd, unsigned position);
unsigned tell (int fd);
int sys_wait(tid_t pid);

void syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static void syscall_handler (struct intr_frame * f){
  // printf("SYSCALL HANDLER HIT CODE: %d\n", (int)*(uint32_t *) f->esp);
  const int FD = 4;
  const int BUF = 8;
  const int SIZE = 12;

  // save stack pointer to TCB on syscall
  struct thread *cur = thread_current ();
  cur->esp = f->esp;

  check_ptr(f->esp);
  check_ptr(f->esp+FD);
  check_ptr(f->esp+BUF);
  check_ptr(f->esp+SIZE);
  // cast f->esp to int*, then dereference for int
  switch ((int)*(uint32_t *) f->esp)
  {
    case SYS_WRITE:
      check_ptr(f->esp+FD);
      check_ptr(f->esp+BUF);
      check_ptr(f->esp+SIZE);

      // printf("write ptr ok\n");
      f->eax = write ((int)*((uint32_t *) (f->esp + FD)), (void*)*(uint32_t *) (f->esp + BUF), 
                      (uint32_t)*((uint32_t *) (f->esp + SIZE)));
      // printf("SYS WRITE: %d\n", f->eax);
      break;

    case SYS_WAIT:
      check_ptr(f->esp+FD);
      f->eax = sys_wait((int)*((uint32_t *) (f->esp + FD)));
      break;

    case SYS_EXIT:
      check_ptr(f->esp+FD);
	    exit ((int)*(uint32_t *)(f->esp + FD));
      break;

    case SYS_READ:
      // printf("SYS READ HIT \n");
      check_ptr(f->esp+FD);
      check_ptr(f->esp+BUF);
      check_ptr(f->esp+SIZE);

      // printf("READ ME\n");
      f->eax = sys_read((int)*((uint32_t *) (f->esp + FD)), (void*)*((uint32_t *)(f->esp + BUF)), 
                      (uint32_t)*((uint32_t *) (f->esp + SIZE)));

      break;

    case SYS_CREATE:
      check_ptr(f->esp+FD);
      check_ptr(f->esp+BUF);
      f->eax = create((const char *)*((uint32_t *) (f->esp + FD)), (unsigned)*((uint32_t *)(f->esp + BUF)));
      break;

    case SYS_EXEC:
      check_ptr(f->esp+FD);
      f->eax = exec((const char *)*((uint32_t *) (f->esp + FD)));
      // printf("SYS EXEC: %d\n", f->eax);
      break;

    case SYS_HALT:
      halt();
      break;

    case SYS_OPEN:
      check_ptr(f->esp+FD); 
      int test = sys_open((const char *)*((uint32_t *) (f->esp + FD)));
      f->eax = test;
      // printf("Open syscall returns %d\n", test);
      break;

    case SYS_REMOVE:
      check_ptr(f->esp+FD);
      f->eax = remove((const char *)*((uint32_t *) (f->esp + FD)));
      break;

    case SYS_SEEK:
      check_ptr(f->esp+FD);
      check_ptr(f->esp+BUF);
      seek((int)*((uint32_t *) (f->esp + FD)), (unsigned)*((uint32_t *)(f->esp + BUF)));
      break;

    case SYS_TELL:
      check_ptr(f->esp+FD);
      f->eax = tell((int)*((uint32_t *)(f->esp + FD)));
      break;

    case SYS_CLOSE:
      check_ptr(f->esp+FD);
      close((int)*((uint32_t *)(f->esp + FD)));
      break;

    case SYS_FILESIZE:
      check_ptr(f->esp+FD);

      f->eax = filesize((int)*((uint32_t *)(f->esp + FD)));
      break;

    default:
      exit(-1);
      break;
  }
}

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE (in kernel virtual memory).
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;

  if ((uint32_t)uaddr >= (uint32_t)CODE_SEG)
  {
    // above PHYS_BASE, seg fault
    return -1;
  }
  else
  {
    asm ("movl $1f, %0; movzbl %1, %0; 1:"
        : "=&a" (result) : "m" (*uaddr));

    return result;
  }
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE (in user virtual memory).
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;

  if ((uint32_t)udst >= (uint32_t)CODE_SEG)
  {
    // below PHYS_BASE, seg fault
    return false;
  }
  else
  {
    asm ("movl $1f, %0; movb %b2, %1; 1:"
        : "=&a" (error_code), "=m" (*udst) : "q" (byte));
    return error_code != -1;
  }
}

void check_read_ptr (const void *ptr){

  if (!is_user_vaddr(ptr) || (uint32_t)ptr < (uint32_t) CODE_SEG)
  {
    exit(-1);
  }
  // if (!pagedir_get_page(thread_current()->pagedir, ptr))
  // {
  //   exit(-1);
  // }
  if (ptr == NULL)
  {
    exit(-1);
  }

  return;
}

void
check_read_buffer(void* buffer, unsigned size){
  unsigned num;
  char* iter = (char *) buffer;
  for(num = 0; num < size; num++){
    check_read_ptr((const void*)iter);
    iter++;
    // printf("Check ptr %p\n", iter);
  }

  return;
}

void check_ptr (const void *ptr){

  if (!is_user_vaddr(ptr) || (uint32_t)ptr < (uint32_t) CODE_SEG)
  {
    exit(-1);
  }
  if (!pagedir_get_page(thread_current()->pagedir, ptr))
  {
    exit(-1);
  }
  if (ptr == NULL)
  {
    exit(-1);
  }

  return;
}

void
check_buffer(void* buffer, unsigned size){
  unsigned num;
  char* iter = (char *) buffer;
  for(num = 0; num < size; num++){
    check_ptr((const void*)iter);
    iter++;
    // printf("Check ptr %p\n", iter);
  }

  return;
}

int
add_file_node(file *file)
{
  struct file_node *node = malloc (sizeof (struct file_node));
  struct thread* cur = thread_current();

  cur->num_fd++;

  node->file = file;
  node->fd = cur->num_fd;

  list_push_front (&cur->file_list, &node->elem);

  return node->fd;
}

void
close_file (int fd)
{
  struct file_node* node = find_file_node (fd);
  if(find_file_node(fd) != NULL){
    file_close (node->file);
    remove_file_node(fd);
  }
}

// find file node in file list by fd
struct file_node *
find_file_node (int fd)
{
  struct list *file_list = &thread_current ()->file_list;
  if(fd < 3){
    // invalid fd
    return NULL;
  }

  struct list_elem *e;
  for (e = list_begin (file_list); e != list_end (file_list); e = list_next (e))
  {
    struct file_node *f = list_entry (e, struct file_node, elem);
    if(f->fd == fd)
    {
      return f;
    }
  }
  return NULL;
}

void
remove_file_node (int fd)
{
  struct list *file_list = &thread_current ()->file_list;
  struct list_elem *e;
  for (e = list_begin (file_list); e != list_end (file_list); e = list_next (e))
  {
    struct file_node *f = list_entry (e, struct file_node, elem);
    if((f != NULL) && (f->fd == fd))
    { 
      list_remove (e);
      // free (e);
      return;
    }
  }
}

//needs to close all files on exit from process!
void
remove_all_file_node (struct list *list_ptr)
{
  struct list_elem *file_iter;
  struct file_node *current;

  // check if these need to be dereferenced
  file_iter = list_begin (list_ptr);
  while (file_iter != list_end (list_ptr))
  {
    current = list_entry (file_iter, struct file_node, elem);
    file_iter = list_next (file_iter);

    file_close (current->file);
    list_remove (&current->elem);
    free (current);
  }
}


/*                   Syscalls Implemented Below                       */

int write (int fd, void* buffer, uint32_t size) {
  check_buffer(buffer, size);
  const int MAX_BUF = 100;
  int size_cpy;

  put_user(buffer, size);

  lock_acquire (&filesys_lock);
  if (fd == 1)
  { 
    size_cpy = size;
    while(size_cpy > MAX_BUF)
    {
      putbuf(buffer, MAX_BUF);
      if(size_cpy > MAX_BUF){
        size_cpy -= MAX_BUF;
      }
    }
    putbuf(buffer, size_cpy);
    lock_release (&filesys_lock);

    return size;
  }
  if (fd == 0 ||fd == 2){
    lock_release (&filesys_lock);
    return 0;
  }
  int status = 0;

  struct file_node *node = find_file_node(fd);
  if(node == NULL)
  {
    lock_release (&filesys_lock);

    return -1;
  }

  file *cur_file = node->file; 
  status = file_write (cur_file, buffer, size);

  lock_release (&filesys_lock);

  return status;
}

void exit (int status){

  struct thread *cur = thread_current();
  cur->exit_stat = status;

  thread_exit();
}

tid_t exec (const char * cmd_line){
  char * iter = cmd_line;
  check_ptr(iter);
  while(*iter != '\0'){
    check_ptr(iter);
    iter++;
  }

  return process_execute(cmd_line);
}

void halt(void) {
  shutdown_power_off();
}

int sys_wait(tid_t pid) {
  return process_wait(pid);
}

int filesize(int fd) {
  int status = -1;
  lock_acquire (&filesys_lock);

  struct file_node *node = find_file_node(fd);
  if(node == NULL)
  {
    lock_release (&filesys_lock);
    return -1;
  }

  file *cur_file = node->file; 
  if (cur_file != NULL)
    status = file_length (cur_file);

  lock_release (&filesys_lock);
  return status;
}

bool create(const char *file, unsigned initial_size) {
  char * iter = file;
  check_ptr(iter);
  while(*iter != '\0'){
    check_ptr(iter);
    iter++;
  }
  const int INVALID = -1;
  if(file == NULL){
    exit(INVALID);
  }

  lock_acquire(&filesys_lock);
  bool ret = filesys_create(file, initial_size);
  lock_release(&filesys_lock);
  return ret; 
} 

 int sys_open(const char *file) {
  // printf("file ptr: %p\n", file);

  check_ptr(file);
  const int INVALID = -1;
  lock_acquire(&filesys_lock);

  if (file == NULL)
  {
    lock_release(&filesys_lock);
    // printf("file ptr is null\n");
    return INVALID;
  }

  // printf("file ptr: %p\n", file);

  struct file* opened = filesys_open (file);
  if (opened == NULL)
  {
    lock_release(&filesys_lock);
    // printf("filesys_open returned a null ptr\n");
    return INVALID;
  }
  int ret = add_file_node(opened);

  // printf("sys open returns %d\n", ret);
  lock_release(&filesys_lock);

  return ret;
}

void seek(int fd, unsigned position) {
  lock_acquire (&filesys_lock);

  struct file_node *node = find_file_node(fd);
  if(node == NULL)
  {
    lock_release (&filesys_lock);
    return;
  }

  file *cur_file = node->file; 
  if (cur_file != NULL)
    file_seek(cur_file, position);

  lock_release (&filesys_lock);
}

unsigned tell(int fd) {
  unsigned offset = 0;
  lock_acquire (&filesys_lock);

  struct file_node *node = find_file_node(fd);
  if(node == NULL)
  {
    lock_release (&filesys_lock);
    return -1;
  }

  file *cur_file = node->file; 
  if (cur_file != NULL)
    offset = file_tell (cur_file);
  lock_release (&filesys_lock);
  
  return offset;
}

bool remove(const char *file) {
  char * iter = file;
  check_ptr(iter);
  while(*iter != '\0'){
    check_ptr(iter);
    iter++;
  }
  lock_acquire(&filesys_lock);
  bool ret = filesys_remove(file);
  lock_release(&filesys_lock);
  return ret;
}


int sys_read (int fd, void *buffer, unsigned size) {
  // printf("Reading now!\n");
  check_read_buffer(buffer, size);
  get_user(buffer);
  lock_acquire(&filesys_lock);
  if (fd == 0){

    uint8_t* buf = (uint8_t *) buffer;
    uint32_t i;
    for (i = 0; i < size; i++)
	  {
	    buf[i] = input_getc();
	  }
    buffer = buf;

    // printf("fd 0, size = %d\n", size);
    lock_release(&filesys_lock);
    return size;
  }
  if (fd == 1){
    // printf("fd 1, read failed\n");
    lock_release(&filesys_lock);
    return -1;
  }

  int read_len = 0;

  struct file_node *node = find_file_node(fd);
  if(node == NULL)
  {
    lock_release(&filesys_lock);
    // printf("File node find failed!\n");
    return -1;
  }

  

  file *cur_file = node->file; 
  if (cur_file != NULL)
  {
    read_len = file_read(cur_file,buffer, size);
  }
  lock_release(&filesys_lock);
  // printf("read success, len = %d\n", read_len);
  return read_len;
}

void close(int fd) {
  lock_acquire(&filesys_lock);

  struct file_node *node = find_file_node(fd);
  if(node == NULL)
  {
    // printf("close error\n");
    lock_release (&filesys_lock);
    return;
  }
  close_file(fd);
  // printf("success close \n");
  lock_release(&filesys_lock);
  return;
}
