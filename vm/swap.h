#include "bitmap.h"

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
void swap_init (void);

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
void swap_free (uint32_t swap_idx);

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
void swap_get (uint32_t swap_idx, void *page);

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
uint32_t swap_put (void *page);
