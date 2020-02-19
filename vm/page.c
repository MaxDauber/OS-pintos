#include <string.h>

#include "stdio.h"
#include "page.h"

#include "filesys/file.h"
#include "filesys/off_t.h"
#include "lib/kernel/list.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/swap.h"

#include "lib/log.h"

struct list *spt_init (void);
bool create_spt_entry (struct list *spt, struct file *file, off_t ofs, void *upage,
                       uint32_t read_bytes, uint32_t zero_bytes, bool writable,
                       bool is_stack);
bool load_vaddr (struct spt_entry* entry);

struct spt_entry *spt_find_vaddr (struct list *spt, void *v_addr);
bool in_stack (void *esp, void *fault_addr);

/**
 * Purpose:
 *  Initialize supplemental page table list
 * 
 * Args:
 *  None
 * 
 * Returns:
 *  {list*} Pointer to supplemental page table list
 */ 
struct list *
spt_init (void)
{

  struct list *spt = malloc (sizeof( struct list));
  
  list_init(spt);
  
  return spt;
}

/**
 * Purpose:
 *  Adds new entry to supplemental page table
 *    * used in `load_segment()` in `process.c`
 * 
 * Args:
 *  spt           {list*} Supplemental page table list
 *  file          {file*} File pointer
 *  ofs           {off_t} File offset
 *  upage         {void*} Virtual address of page
 *  read_bytes {uint32_t} Number of bytes read starting from offset `ofs`
 *  zero_bytes {uint32_t} Number of bytes to be zeroed
 *  writable       {bool} Indicates whether file is writable
 *  is_stack       {bool} True if creating spt entry for stack page
 * 
 * Returns:
 *  {bool} True if entry successfully created
 */ 
bool
create_spt_entry (struct list *spt, struct file *file, off_t ofs, void *upage,
                  uint32_t read_bytes, uint32_t zero_bytes, bool writable,
                  bool is_stack)
{
  const uint32_t PAGE_ZERO = 0x8048000;
  int result = 1;

  struct spt_entry *new_entry = malloc (sizeof (struct spt_entry));
  if(new_entry != NULL){
    // set to not loaded
    new_entry->p_addr = 0;

    // virtual address
    new_entry->v_addr = upage;

    // assign file pointer
    new_entry->file_pt = file;

    // set more fields here as we discover their needs, change below as you see fit
    new_entry->ofs = ofs;

    // number of bytes read startinf from offset
    new_entry->read_bytes = read_bytes;

    // number of bytes to be zeroed
    new_entry->zero_bytes = zero_bytes;

    // indicates if file has write access
    new_entry->writable = writable;
    
    // loaded on to frame
    new_entry->loaded = false;
    
    // swap index is -1 by default
    new_entry->swap_index = -1;

    if (is_stack)
    {
      new_entry->is_stack = true;
    }
    else
    {
      new_entry->is_stack = false;
    }
    

    if((uint32_t) upage == PAGE_ZERO)
    {
      // pin if first stack page
      new_entry->pinned = true;
    }
    else
    {
      new_entry->pinned = false;
    }

    list_push_back(spt, &new_entry->elem);

  }
  else{
    result = 0;
  }

  return result;
}

/**
 * Purpose:
 *  Load virtual address on to frame in memory on page fault
 * 
 * Args:
 *  entry {spt_entry*} Supplemental page table entry of virtual address
 *                     at page fault
 * 
 * Returns:
 *  {bool} True on successful load
 */ 
bool
load_vaddr (struct spt_entry* entry)
{
  // Offset including read bytes
  off_t read_ofs = 0;

  struct thread *cur = thread_current ();

  // physical frame address
  void* frame = get_frame (PAL_USER, entry->v_addr);

  if (frame == NULL)
  {
    printf("ERROR: frame allocation failed!\n");
    return false;
  }
  else if (entry->swap_index != -1)
  {
    // printf("load addr. %p from swap %d\n", entry->v_addr, entry->swap_index);
    // page in swap, load into memory
    entry->pinned = true;
    swap_get(entry->swap_index, frame);
    swap_free(entry->swap_index);
    entry->swap_index = -1;
    entry->pinned = false;
  }
  else if(entry->zero_bytes == PGSIZE)
  {
    // printf("Writing zero page!\n");
    memset (frame, 0, entry->zero_bytes);
  }
  else
  {
    // place data in frame
    // memset a page of zeroes for PAL_ZERO
    // need to handle swap case

    // set position in file to begin reading from
    file_seek (entry->file_pt, entry->ofs);

    // read read_bytes (int) from file_pt (file) into frame (as buffer)
    read_ofs = file_read (entry->file_pt, frame, entry->read_bytes);

    if (read_ofs != (int) entry->read_bytes)
    {
      printf("ERROR: number of bytes read from file != found->read_bytes!\n");
      return false;
    }

    // set remaining bytes to 0
    memset (frame + read_ofs, 0, entry->zero_bytes);
  }

  // set spt entry flags to loaded
  entry->loaded = true;
  entry->p_addr = frame;

  // printf("file pt: %p, read %d bytes, %d byte ofs, %d read ofs, %d zero bytes\n",
  //         entry->file_pt, entry->read_bytes, entry->ofs, read_ofs, entry->zero_bytes);

  // add pagedir/pte entry
  if (pagedir_get_page (cur->pagedir, entry->v_addr))
  {
    printf ("ERROR: %p already in page directory!", entry->v_addr);
    return false;
  }

  // create page table entry
  pagedir_set_page (cur->pagedir, entry->v_addr, frame, entry->writable);

  // set dirty bit to false
  // pagedir_set_dirty (cur->pagedir, entry->v_addr, false);

  // uncomment for hex dump
  // hex_dump( *(int*)frame, frame, 128, true );
  // printf("Page loaded to frame %p!\n", frame);
  return true;
}

/**
 * Purpose:
 *  Find supplemental page table entry by virtual address
 * 
 * Args:
 *  spt    {list*} List pointer
 *  v_addr {void*} Virtual address
 * 
 * Returns:
 *  {spt_entry*} Pointer to supplemental page table entry of passed virtual 
 *               address, NULL if nothing found 
 */ 
struct spt_entry *
spt_find_vaddr (struct list *spt, void *v_addr)
{
  struct list_elem *e;

  for (e = list_begin (spt); e != list_end (spt); e = list_next (e))
  {
    struct spt_entry *k = list_entry (e, struct spt_entry, elem);
    if((uint32_t)(k->v_addr) == (uint32_t)v_addr)
    {
      return k;
    }
  }
  return NULL;
}

/**
 * Purpose:
 *  Load stack page into memory if valid stack address
 * 
 * Args:
 *  esp        {void*} Stack pointer
 *  fault_addr {void*} Fault address
 * 
 * Returns:
 *  {bool} True if page can exist in stack
 */
bool
in_stack (void *esp, void *fault_addr)
{
  // 8 Mb of stack space
  const uint32_t STACK_MAX = 8388608;

  struct thread* cur = thread_current ();

  void* page = pg_round_down(fault_addr);

  if(pagedir_get_page(cur->pagedir, page) != NULL){
    // entry exists in page dir
    PANIC ("Page exists on page table already!\n");
  }
  
  // Check that stack address conforms with x86 push convention
  bool conform_x86_push = (uint32_t)(void*)fault_addr >= ((uint32_t)(esp) - 32);

  // Check fault address in stack space
  bool in_stack_space = (fault_addr < PHYS_BASE) && (fault_addr > (PHYS_BASE - STACK_MAX));

  if(!(in_stack_space && conform_x86_push))
  {
    // Invalid fault address, return

    return false;
  }

  // create spt entry for stack page, all zero bytes
  create_spt_entry (cur->spt, NULL, 0, page, 0, 4096, 1, true);

  struct spt_entry* found = spt_find_vaddr (cur->spt, page);
  found->pinned = true;

  if (found == NULL)
  {
    // spt entry doesn't exist for this v. addr

    return false;
  }
  else
  {
    // spt entry found for v. addr

    if (load_vaddr (found))
    {
      // load successful

      return true;
    }
    else
    {
      // load unsuccessful

      return false;
    }
  }

  return true;
}

