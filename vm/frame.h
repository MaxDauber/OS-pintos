#include "lib/kernel/list.h"

// frame table entry data structure
struct ft_entry
{
  // pointer to user page virtual address
  void *v_addr;

  // pointer to frame memory location
  void *p_addr;

  // current thread
  struct thread *curr; 

  // status if in use, used for eviction
  int used;

  // list element
  struct list_elem elem;
};

/**
 * Purpose:
 *  Initializes frame table
 * 
 * Args:
 *  None
 * 
 * Returns:
 *  None
 */
void init_frame (void);

/**
 * Purpose:
 *  Add element to frame table
 * 
 * Args:
 *  flags    {int} Enum of flags to be passed into `palloc_get_page`
 *  v_addr {void*} Virtual address
 * 
 * Returns:
 *  {void*} Physical mem. address
 */ 
void* get_frame (int flags, void* v_addr);
