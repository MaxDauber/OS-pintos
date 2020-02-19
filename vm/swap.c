#include <bitmap.h>
#include <stdio.h>
#include "threads/vaddr.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "vm/swap.h"
#include "vm/page.h"
#include "vm/frame.h"

static const size_t NUM_SECTORS = PGSIZE / BLOCK_SECTOR_SIZE;
static size_t swap_space_size;
static struct block *swap_disk;
static struct bitmap *swap_map;

struct lock swap_lock;

void swap_init (void);
void swap_free (uint32_t swap_idx);
void swap_get (uint32_t swap_idx, void *page);
uint32_t swap_put (void *page);

/**
 * Purpose:
 *  Initialize swap table list
 * 
 * Args:
 *  None
 * 
 * Returns:
 *  None
 */ 
void
swap_init (void)
{

  // Initialize the swap disk
  swap_disk = block_get_role(BLOCK_SWAP);

  // get the swap space size
  swap_space_size = block_size(swap_disk) / NUM_SECTORS;

  //Initialize bitmap for representation of swap map
  swap_map = bitmap_create(swap_space_size);

  //Set all elements in bitmap to available
  bitmap_set_all(swap_map, true);

  // lock_init (&swap_lock);
  
  return;
}

/**
 * Purpose:
 *  Frees page to be written to (for when closing down and resetting 
 *   swap space)
 * 
 * Args:
 *  uint32_t swap_idx the index in the swap table to set
 * 
 * Returns:
 *  N/A
 */ 
void
swap_free (uint32_t swap_idx){

  // lock_acquire (&swap_lock);
  
  // printf("free swap index %d\n", swap_idx);

  //validate index
  ASSERT (swap_idx < swap_space_size);

  // printf("Index %d is available? %d\n", swap_idx, bitmap_test(swap_map, swap_idx));

  //fails if index is still available
  // ASSERT (!bitmap_test(swap_map, swap_idx));
  
  //set index to available
  bitmap_set(swap_map, swap_idx, true);

  // lock_release (&swap_lock);

  return;
}

/**
 * Purpose:
 *  Get specified page from swap space
 * 
 * Args:
 *  {uint32_t swap_idx}
 *  {void *page} 
 * 
 * Returns:
 *  None
 */ 
void
swap_get (uint32_t swap_idx, void *page){

  // printf("get page %p from swap index %d!\n", page, swap_idx);

  // lock_acquire (&swap_lock);

  //fails if index is still available
  ASSERT (!bitmap_test(swap_map, swap_idx));

  uint32_t sector;
  for (sector = 0; sector < NUM_SECTORS; ++sector) {

    //read from swap block using the sector number and indexed target addr
    block_read (swap_disk, swap_idx * NUM_SECTORS + sector,
        page + (BLOCK_SECTOR_SIZE * sector)
        );
  }

  //set written block sector bit to accessed in swap table
  bitmap_set(swap_map, swap_idx, true);

  // lock_release (&swap_lock);

  return;
}

/**
 * Purpose:
 *  Puts page into swap space 
 * 
 * Args:
 *  None
 * 
 * Returns:
 *  {uint32_t} Swap index of page
 */ 
uint32_t
swap_put (void *page){
  printf("problem page = %p\n", page);
  //get the next available swap slot by scanning bitmap
  uint32_t swap_index = bitmap_scan (swap_map, 0, 1, true);

  uint32_t sector = 0;
  for (sector = 0; sector < NUM_SECTORS; sector++) {
    //write to swap block using the sector number and indexed target addr
    block_write(swap_disk, swap_index * NUM_SECTORS + sector,
        page + (BLOCK_SECTOR_SIZE * sector)
        );
  }

  //set written block sector bit to occupied in swap table
  bitmap_set(swap_map, swap_index, false);
  printf("put frame %p in swap index %d\n", page, swap_index);
  return swap_index;
}



