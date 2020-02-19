#include "lib/kernel/list.h"
#include "filesys/file.h"
#include "filesys/off_t.h"
#include "userprog/pagedir.h"

struct spt_entry{

  //hash element
  struct list_elem elem;

  // key
  void * v_addr;

  // physical frame address
  void * p_addr;

  // File pointer
  struct file *file_pt;

  // File offset
  off_t ofs;

  // Number of bytes read starting at offset ofs
  uint32_t read_bytes;

  // Number of bytes to be zeroes
  uint32_t zero_bytes;

  // True if can write to this page
  bool writable;

  // True if loaded on frame (kpage)
  bool loaded;

  // True if in swap
  int swap_index;

  // True if stack page
  bool is_stack;

  // Pins page so it can not be overwritten
  bool pinned;
};


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
struct list *spt_init(void);

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
                  bool is_stack);

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
bool load_vaddr (struct spt_entry *entry);

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
struct spt_entry *spt_find_vaddr (struct list *spt, void *v_addr);

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
bool in_stack (void * esp, void *fault_addr);