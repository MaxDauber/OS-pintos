#include "stdio.h"
#include <hash.h>
#include "lib/kernel/list.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

#include "userprog/pagedir.h"

void init_frame (void);
void* get_frame (int flags, void* v_addr);
struct ft_entry* eviction_algo (void);
struct ft_entry* evict (struct list* f_table, struct list_elem* clock_hand);
struct ft_entry* evict_test (void);
void clock_hand_init(struct list_elem *clock_hand);
void free_frame (struct ft_entry *victim);

struct list_elem* find_next_elem (struct list* list, struct list_elem* curr);
struct ft_entry* find_ft_entry (void *v_addr);

// frame table lock
struct lock ft_lock;

// frame table list pointer
struct list f_table;

// Clock hand pointer
struct list_elem *clock_hand = NULL;

/**
 * Purpose:
 *  Initializes frame table and LRU clock_hand
 * 
 * Args:
 *  None
 * 
 * Returns:
 *  None
 */
void
init_frame (void)
{ 
  // Initalize frame table lock
  lock_init(&ft_lock);

  // Initialize list
  // f_table = (struct list*) malloc (sizeof (struct list));
  list_init(&f_table);

  // Initialize LRU clock hand
  // clock_hand = (struct list_elem*) malloc (sizeof (struct list_elem));
  clock_hand = NULL;

  return;
}

// THIS IS A GLOBAL FRAME TABLE ACCESS, IT NEEDS A LOCK TO PREVENT MULTI ACCESS
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

int frame_count = 0;
void*
get_frame (int flags, void* v_addr)
{
  // printf("hit get_frame!\n");
  
  lock_acquire (&ft_lock);
  // printf("ft lock acquired\n");
  // wrapped function, gets kpage
  struct thread *cur = thread_current();
  void *p_addr = palloc_get_page (flags);

  // artificial limit of 10 pages on frame table to test swap
  // void* p_addr = NULL;
  // if(frame_count < 10)
  // {  
  //   p_addr = palloc_get_page (flags);
  //   frame_count++;
  // }
  
  // printf("Allocated: %p\n", p_addr);

  // make new ft_entry
  if (p_addr != NULL)
  {
    // consider case where malloc fails, returns NULL
    struct ft_entry *entry = (struct ft_entry*) malloc (sizeof (struct ft_entry));
    entry->v_addr = v_addr;
    entry->p_addr = p_addr;
    entry->curr = cur;
    entry->used = 1;

    list_push_back (&f_table, &entry->elem);
  }
  else
  {
    // kernel panic for now, change this to eviction algorithm (clock LRU)
    // PANIC ("Out of memory!");

    // Uncomment when evict() is working

    // // find victim in frame table entry to evict
    // struct ft_entry *victim = evict (&f_table, clock_hand);
    struct ft_entry *victim = evict_test();
    // printf("evicted %p from %p\n", victim->v_addr, victim->p_addr);
  
    struct spt_entry *entry = spt_find_vaddr(cur->spt, victim->v_addr);
    //printf("problem v.addr = %p\n", victim->v_addr);
    p_addr = victim->p_addr;
    pagedir_clear_page (cur->pagedir, victim->v_addr);
    // lock_release (&ft_lock);
    // printf("ft lock released\n");
    //entry->pinned = true;
    printf("problem v.addr = %p\n", victim->v_addr);

    entry->swap_index = swap_put(victim->p_addr);
    // if (pagedir_is_dirty (cur->pagedir, victim->v_addr))
    // {
    //   // send to swap
    //   entry->swap_index = swap_put(victim->v_addr);
    printf("return from swap = %p\n", victim->v_addr);
    // }
    entry->loaded = false;

    // lock_acquire (&ft_lock);
    // printf("ft lock acquired\n");

    palloc_free_page(victim->p_addr);

    victim->v_addr = v_addr;

    entry->pinned = false;
    // free(victim);

    // p_addr = palloc_get_page(flags);

    // printf("swap done\n");
    // if (pagedir_is_dirty (cur->pagedir, victim->v_addr) || entry->is_stack)
    // {
    //   // send to swap
    //   entry->swap_index = swap_put(victim->v_addr);

    // }
  }
  lock_release (&ft_lock);
  // printf("ft lock released\n");

  // printf("load to %p\n", p_addr);

  return p_addr;
}

/**
 * Purpose:
 *  Algorithm for finding frame to evict, very basic for now, build it in
 *  complexity as you resolve more cases
 * 
 */ 
struct ft_entry*
eviction_algo (void)
{
  struct list_elem *e;
  // for (e = list_begin (&f_table); e != list_end (&f_table); e = list_next (e))
  // {
  //   struct ft_entry *k = list_entry (e, struct ft_entry, elem);
  //   pagedir_is_dirty (thread_current ()->pagedir, k->v_addr);

    
  // }

  e = list_begin (&f_table);
  struct ft_entry *victim_ft = NULL;
  struct spt_entry *victim_spt = NULL;
  struct thread *cur = thread_current ();
  bool isDirty = false;
  bool pinned = false;

  int j = 0;
  for (j = 0; j < 20; j++)
  {
    pinned = false;

    // limit ft to 10 entries for now
    // TODO impose 10 frame limit on frame table
    if (j < 10)
    {
      victim_ft = list_entry (e, struct ft_entry, elem);
      isDirty = pagedir_is_dirty (cur->pagedir, victim_ft->v_addr);
      victim_spt = spt_find_vaddr (cur->spt, victim_ft->v_addr);
      if(victim_spt != NULL)
      {
        pinned = victim_spt->pinned;
      }

      if (!pinned)
      {
        return victim_ft;
      }
   
      // if (isDirty)
      // {
      //   // no evict
      // }
      // else
      // {
      //   // always evict for now to test swapping
      // }
      
    }
    else if (j == 10)
    {
      e = list_begin (&f_table);
      victim_ft = list_entry (e, struct ft_entry, elem);
      isDirty = pagedir_is_dirty (cur->pagedir, victim_ft->v_addr);
      victim_spt = spt_find_vaddr (cur->spt, victim_ft->v_addr);
      if(victim_spt != NULL)
      {
        pinned = victim_spt->pinned;
      }

      if (!isDirty && !pinned)
      {
        return victim_ft;
      }
    }
    else
    {
      // evict dirty as well
      victim_ft = list_entry (e, struct ft_entry, elem);
      isDirty = pagedir_is_dirty (cur->pagedir, victim_ft->v_addr);

      victim_spt = spt_find_vaddr (cur->spt, victim_ft->v_addr);
      if(victim_spt != NULL)
      {
        pinned = victim_spt->pinned;
      }

      if (!pinned)
      {
        return victim_ft;
      }
    }
    e = find_next_elem (&f_table, e);
    
  }
  return NULL;
}

/**
 * Purpose:
 *  Returns frame table entry of frame to evict
 * 
 * Args:
 *  f_table         {list*} Pointer to frame table list
 *  clock_hand {list_elem*} Pointer to last clock hand position
 * 
 * Returns:
 *  {ft_entry*} Frame table entry of evicted frame
 */
struct ft_entry*
evict_test (void)
{
  int table_size = list_size(&f_table);
  struct thread* cur = thread_current();
  bool pinned = false;

  // printf("tablesize: %d", table_size);
  //ASSERT(table_size != 0);
  int iter = 0;
  for (iter = 0; iter < 2 * table_size ; iter++){

    if (clock_hand == NULL || clock_hand == list_end(&f_table)){
      clock_hand = list_begin(&f_table);
    }
    else{
      clock_hand = list_next(clock_hand);
    }
    struct ft_entry *victim = list_entry(clock_hand, struct ft_entry, elem);
   
    if(pagedir_is_accessed(cur->pagedir, victim->v_addr)) {
      pagedir_set_accessed(cur->pagedir, victim->v_addr, false);
    }
    else{
      
      struct spt_entry *victim_spt = spt_find_vaddr (cur->spt, victim->v_addr);
      if(victim_spt != NULL)
      {
        pinned = victim_spt->pinned;
      }

      if (!pinned)
      {
        return victim;
      }
      
    }

  }
  return NULL;
}

// /**
//  * Purpose:
//  *  Returns frame table entry of frame to evict
//  * 
//  * Args:
//  *  f_table         {list*} Pointer to frame table list
//  *  clock_hand {list_elem*} Pointer to last clock hand position
//  * 
//  * Returns:
//  *  {ft_entry*} Frame table entry of evicted frame
//  */
// struct ft_entry*
// evict (struct list *f_table, struct list_elem *clock_hand)
// {
//   lock_acquire (&ft_lock);

//   // Maximum number of cycles that LRU clock can turn
//   const int MAX_CYCLES = 4;
//   const int RESET_VAL = 0;

//   // Number of cycles run by clock LRU
//   int num_cycle = 0;
  
//   // try to evict clean pages first
//   bool evict_dirty = false;

//   // initial clock position
//   struct list_elem* init_pos = clock_hand;

//   // frame table entry of victim to be evicted
//   struct ft_entry *victim = NULL;
//   struct ft_entry *curr_entry = NULL;

//   while ((num_cycle < MAX_CYCLES) && (victim == NULL))
//   {
//     curr_entry = list_entry (clock_hand, struct ft_entry, elem);

//     if (!evict_dirty)
//     {
//       // do not evict dirty pages
//       if (curr_entry->used == RESET_VAL && !curr_entry->used)
//       {
//         // find victim if not dirty, used == 0
//         victim = curr_entry;
//       }
//       else
//       {
//         // reset used bit
//         curr_entry->used = RESET_VAL;
//       }
//     }
//     else
//     {
//       // allow dirty page eviction
//       if (curr_entry->used == RESET_VAL)
//       {
//         // find victim if not dirty, used == 0
//         victim = curr_entry;
//       }
//       else
//       {
//         // reset used bit
//         curr_entry->used = RESET_VAL;
//       }
//     }
    
//     if (num_cycle >= MAX_CYCLES / 2)
//     {
//       // allow dirty page eviction after one revolution
//       evict_dirty = true;
//     }

//     // Advance to next element
//     clock_hand = find_next_elem (f_table, clock_hand);
//     if (clock_hand == init_pos)
//     {
//       // increment cycle count after one revolution
//       num_cycle++;
//     }
//   }
//   lock_release (&ft_lock);

//   return victim;
// }

// /**
//  * Purpose:
//  *  Initialize clock hand
//  * 
//  * Args:
//  *  clock_hand {list_elem*} Pointer to last clock hand position
//  * 
//  * Returns:
//  *  None
//  */
// void
// clock_hand_init(struct list_elem *clock_hand)
// {
//   if(clock_hand == NULL)
//   {
//     // set clock hand to beginning if NULL
//     clock_hand = list_begin(&f_table);
//   }

//   return;
// }

// /**
//  * Purpose:
//  *  Frees frame table entry and its associated frame
//  * 
//  * Args:
//  *  victim {ft_entry*} Frame table entry to free
//  * 
//  * Returns:
//  *  None
//  */ 
// void
// free_frame (struct ft_entry *victim)
// {
//   lock_acquire (&ft_lock);
  
//   list_remove(&victim->elem);

//   palloc_free_page(victim->p_addr);

//   free(victim);

//   lock_release (&ft_lock);

//   return;
// }

/**
 * Purpose:
 *  Finds next element in list
 * 
 * Args:
 *  list {list*}      Pointer to list structure
 *  curr {list_elem*} Pointer to current list element
 * 
 * Returns:
 *  {list_elem*} Pointer to next list element
 */ 
struct list_elem*
find_next_elem (struct list* list, struct list_elem* curr)
{
  // TODO: guard against case where list becomes empty
  if (curr == list_end (list))
  {
    // curr has reached tail, set to first element of list
    curr = list_begin (list);
  }
  else
  {
    curr = list_next (curr);
  }

  return curr;
}

// /**
//  * Purpose:
//  *  Find frame table entry by virtual address
//  * 
//  * Args:
//  *  v_addr {void*} Virtual address
//  * 
//  * Returns:
//  *  {ft_entry*} Pointer to frame table entry of passed in virtual address,
//  *              NULL if nothing found 
//  */ 
// struct ft_entry *
// find_ft_entry (void *v_addr)
// {
//   struct list_elem *e;
//   for (e = list_begin (&f_table); e != list_end (&f_table); e = list_next (e))
//   {
//     struct ft_entry *k = list_entry (e, struct ft_entry, elem);
//     if((uint32_t)(k->v_addr) == (uint32_t)v_addr)
//     {
//       return k;
//     }
//   }
//   return NULL;
// }

// void* free_frame ()
// {
  
// }
