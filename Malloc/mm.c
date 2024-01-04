/* 
 * In this approach, I used an explicit-free list whose header is a struct containing an __int128_t size and two pointers to the previous and next 
 * structs in the explicit-free list. I used a best-fit method.
 * 
 * Since all sizes are 16-byte aligned, the lower 4 bits are always unused. Because of this, I used the bottom three blocks to indicate the allocation
 * of the previous, next, and current block in the free list. 
 * 
 * 
 * Some Performance improvements I made:
 *  - Minimum page size is 4096 bytes. Every time malloc is called, this number doubles unless the requested size is bigger. In which case, the 
 *    minumum page size will be changed to match the reqested size.
 *  - I Also keep track of biggest free block in the free list so that when the best fit method was called, it didn't waste time trying to find a 
 *      best fit that didn't exist.
 *  - Instead of adding and removing every time the explicit-free list changes, I made a function called "Replace_Free_Block".
 *    This function receives two pointers as parameters. The first is the item to remove and the second is the item to add. 
 *    Additionally, this method may receive NULL as one of the parameters to indicate that only one pointer is being added or removed.
 * 
 * 
 * @author: Shem Snow u1058151
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

/* ================================================== Definitions ================================================================*/
/* always use 16-byte alignment */
#define ALIGNMENT 16
/* rounds up to the nearest multiple of ALIGNMENT (16) */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
/* rounds up to the nearest multiple of mem_pagesize() */
#define PAGE_ALIGN(size) (((size) + (mem_pagesize()-1)) & ~(mem_pagesize()-1))

typedef struct node_header {
  __int128_t size; // 16-byte
  struct node_header* prev; // 8-byte pointer
  struct node_header* next; // 8-byte pointer
} node_header; // 32 bytes is 16-byte aligned


/* ================================================== Global Variables ================================================================*/
const int PAGE_SIZE = 4096; 
struct node_header* free_list_head; // free_list_tail is the one whose "next" is null.
static size_t free_list_size;

static size_t biggest_free_block_size; // Any time malloc requestes something bigger than this then don't bother finding the best fit

/* ==================================================== Helper Methods ==============================================================*/
struct node_header* Best_Fit(size_t requested_size);
static struct node_header* coalesce(node_header* new_free_block);
void Replace_Free_Block(node_header* old, node_header* new);
void* Allocate_Block(node_header* destination, size_t size);

static void extend_free_list(int pg_size);

// Debugging methods
// void printstatus();
// void print_free_list();

/* ==================================================== Functions to Implement ==============================================================*/

/* 
 * mm_init - initialize the malloc package.
 * 
 * Resets the implementation into its initial state in each case (because the testing program may call it multiple times even though only one init is called per run).
 * Performs any necessary initializations such as initial heap area.
 * 
 * @returns -1 if an error occured. 0 otherwise.
 */
int mm_init(void) {

  // Initialize the free list to be empty (this program may be called after another)
  free_list_head = NULL;
  free_list_size = 0;
  biggest_free_block_size = 0;

  // Initialize the heap area into its original state (a single page of the minimum size).
  extend_free_list(PAGE_SIZE);

  return 0;
}

/* 
 * mm_malloc - Allocate a block by using bytes from current_avail, grabbing a new page if necessary.
 * 
 * The entire allocated block should lie within the heap region and should not overlap with any other allocated block.
 * 
 * 
 * @returns a 16-byte-aligned pointer to an allocated block payload of at least "size" bytes (less than 2^32). 
 */
void *mm_malloc(size_t size_of_new_data) {
  
  if(size_of_new_data == 0)
    return NULL;

  size_t size_of_new_block = ALIGN(size_of_new_data + 8*sizeof(struct node_header));

  // If the free list can't hold the new data then expand it.
  if (free_list_size < size_of_new_block) {
    
    size_t new_size = PAGE_ALIGN(size_of_new_block);

    // Append a new page to the start of the free list.
    extend_free_list(new_size - free_list_size);
    if (free_list_head == NULL)
      return NULL;
  }

  // Store the new data into the free-list
  struct node_header* store_location;
    
  // If there's a best fit then place it there.
  struct node_header* best_fit = Best_Fit(size_of_new_block);
  if(best_fit != NULL)
    store_location = best_fit;

  // Otherwise another page will have to be put at the beginning of the free-list.
  else {
    extend_free_list(size_of_new_block);
    store_location = free_list_head;
    biggest_free_block_size = size_of_new_block;
  }
  // Actually store the new block and make sure to propperly update headers and footers.
  return Allocate_Block(store_location, size_of_new_block);
}

/*
 * mm_free - Frees the block pointed to by "ptr".
 * 
 * This routine is only guaranteed to work when the passed a pointer (ptr) that was returned by an earlier call to mm_malloc and has not yet been freed.
 * 
 */
void mm_free(void *ptr) {

  // Obtain the block to remove.
  struct  node_header* block_to_remove;

  // If the block is not in the free list then do nothing.
  struct node_header* runner;
  for (runner = free_list_head; runner != NULL; runner = runner->next ) {
    if(&runner == &ptr) {
      block_to_remove = ptr;
      break;
    }
  }
  if(block_to_remove == NULL) {
    
    return;
  }
  // block_to_remove = (struct  node_header*) ptr; 

  // Coalesce it.
  struct node_header* remainder = coalesce(block_to_remove);

  // Remove it from the free list and update the size.
  Replace_Free_Block(remainder, NULL);
  free_list_size -= remainder->size;

  // Unmap it.
  mem_unmap(remainder, remainder->size);
}

/* =============================================== Helper Methods ===================================================================*/

/*
 * Iterates through the free list until it finds the best place to fit a new block of the "requested_size".
 *
 * @returns a pointer to the start of the best-fitting free block for the "requested_size". That pointer will be NULL if no free block is large enough.
 */
struct node_header* Best_Fit(size_t requested_size) {

  // Create a pointer that will move alongside the free list to find the best-fitting free block.
  struct node_header* runner;
  struct node_header* best_fit = NULL;

  // Don't bother checking for a best fit if there's not one.
  if(requested_size > biggest_free_block_size)
    return NULL;

  // Iterate through the free list.
  for (runner = free_list_head; runner != NULL; runner = runner->next ) {

    // Get the size of the current free block.
    size_t runner_size = ( runner->size & ~0xF );

    // Don't bother checking space that's too small.
    if(requested_size > runner_size)
      continue;

    // The first block that's large enough will start as the best fit.
    if(best_fit == NULL)
      best_fit = runner;

    // Break out of the loop if the perfect-size free block is found.
    if (runner_size == requested_size) { // To make this first-fit, just change the condition to >=
      best_fit = runner;
      break;
    }

    // Continue the optimization loop and compare the sizes of the current block with the best fitting block so far.
    if (runner_size < ( best_fit->size & ~0xF ) ) {
        best_fit = runner;
    }
    
  }

  return best_fit;
}



/*
* This method will be called only by free().
* 
* Coalesces multiple free blocks if they are next to each other.
* @Returns a pointer to the new coalesced block (or NULL if there are none) and removes or extends the old ones.
* 
* Also calls helper methods to add to and remove from the free list.
* 
* @param new_free_block
*/
static struct node_header* coalesce(node_header* new_free_block) {
  // determine which blocks are allocated.
  int prev = (new_free_block->prev == NULL)? 1: new_free_block->prev->size & 0x1;
  int next = (new_free_block->next == NULL)? 1: new_free_block->next->size & 0x1;

  // There are 4 cases:

  // Case 1: neither the previous nor next block are free.
  if( prev && next) {
    Replace_Free_Block(NULL, new_free_block); // Nothing to coalesce. Just add the "new_free_block" to the free list.
    free_list_size += new_free_block->size;
    return new_free_block;
  }
    

  // Case 2: The previous block is free but the next one is not.
  else if(!prev && next) {

    // Update the allocation bits of the previous block.
    new_free_block->prev->size += (new_free_block->size & ~0xf);
    // Also update the free list size
    free_list_size += new_free_block->size & ~0xF;
    
    // No need to adjust previous or next pointers. Also no need to change the free list.
    return new_free_block->prev;
  }

  // Case 3: The next block is free but the previous is not.
  else if( prev && !next) {

    // Update the size and allocation bits of the "new_free_block".
    __int128_t size_of_block_to_remove = ( new_free_block->next->size & ~0xF);
    free_list_size -= size_of_block_to_remove;

    __int128_t new_size = new_free_block->size + size_of_block_to_remove;
    new_free_block->size = (new_size | 0x6); // header
    free_list_size += new_size;

    // Update the free list
    Replace_Free_Block(new_free_block->next, new_free_block);

    return new_free_block;
  }

  // Case 4: Both the previous and the next block are free. This is just a combination of the logic in cases 2 and 3.
  __int128_t size_of_prev = new_free_block->prev->size & ~0xF;
  __int128_t size_of_next = new_free_block->next->size & ~0xF;
  __int128_t new_size = size_of_prev + new_free_block->size + size_of_next;

  // Update the size and allocation bits of the the previous block
  new_free_block->prev->size = (new_size | 0x6);

  // Extend the previous block to contain all three blocks then update the free list size. Then remove the next free block.
  free_list_size += new_free_block->size;
  Replace_Free_Block(new_free_block->next, NULL);

  return new_free_block->prev;
}

/*
* Uses mem_map to allocate a new chunk of memory for the "requested_space".
* Updates the free_list_head and free_list_size.
* 
* CALLER AGREEMENT: The exact amount of "requested_space" will be allocated so it is up to the caller to make sure it is page-aligned.
* This method will make sure it is a multiple of 4096.
*/
static void extend_free_list(int requested_space) {

  // Make sure the requested space is a multiple of 4096
  int granted_space = (requested_space % 4096 == 0)? requested_space: ((requested_space / 4096) + 1) * 4096;

  if(granted_space > biggest_free_block_size)
    biggest_free_block_size = granted_space;

  // Create a pointer to the new head of the free list.
  void* new_space = mem_map(granted_space);
  if(new_space == NULL)
    return;
  
  // Store its size and allocation bits in a header.
  node_header* new_node = (node_header*) new_space;
  new_node->size = (__int128_t) ((granted_space) | 0x6);

  // Point the previous pointer in the free block to NULL (since it is the new head) and point the next pointer to the current free list head.
  new_node->prev = NULL;
  new_node-> next = free_list_head;

  // Set the head of the free list to this new block and update its size.
  free_list_head = new_node;
  free_list_size += granted_space;



  // Coalesce if the block happens to be next to other free blocks.
  // coalesce(new_head); // Assume this will never happen? If it does then ignore it?
}

/*
 * Replaces one item in the free list with another.
 * CALLER AGREEMENT:
 *    This method does not update the "free_list_size" so it is the caller's responsibility to do that.
 * @param old
 * @param new
 *    NULL may be used as arguments. If "old" is NULL then that means we're adding to the free list.
 *    If "new" is NULL that means we're only removing the "old" value.
 *    Please don't call this method with (NULL, NULL) arguments because that will cause a null-pointer exception.
*/
void Replace_Free_Block(node_header* old, node_header* new) {

  // If there's nothing to replace then just add the new block and return.
  if(old == NULL) {

    // Update the prev and next pointers
    free_list_head->prev = new;
    new->next = free_list_head;
    new->prev = NULL;

    // Update the free list head.
    free_list_head = new;
  }
  
  // If we're replacing the free list head
  if(old->prev == NULL) {

    // If we're removing the only item in the free list
    if(new == NULL) {
      mm_init();
      return;
    }

    // Otherewise give the remaining free block pointers to prev and next.
    else {
      
      // Update the new block.
      new->prev = NULL;
      new->next = old->next;

      // If this is the only item in the free list then don't try to dereference the NULL next pointer.
      if(old->next != NULL) {
        old->next->prev = new;
      }
      
      // Update the free list head.
      free_list_head = new;

      return;
    }
  }

  // If we're removing a node.
  if(new == NULL) {
    new->next = NULL;
    new->prev = old;
    old->prev->next = new;
    return;
  }

  // Otherwise we're replacing some node in the free list.
  new->prev = old->prev;
  new->next = old->next;
  
  old->prev->next = new;
  // If this is the end of the free list then immediately return to avoid NULL pointer error
  if(old->next == NULL) 
    return;
  
  // Otherwise update the prev and next pointers in the prev and next nodes.
  old->next->prev = new;
}

/*
 * Caller Agreement: The block must be able to fit in the specified size and already be 16-byte aligned.
 * @param size is already it BITS (not bytes)
 * @Returns a pointer to the payload of the newly allocated block.
*/
void* Allocate_Block(node_header* destination, size_t size) {

  // Determine if the new block will split an existing free block or occupy the entire thing.
  size_t total_space = (destination->size & ~0xF); 
  size_t remaining_free_space = total_space - size;

  // If the remaining space is too small to be useful then let the new data take up the whole block
  if(remaining_free_space <= (8*sizeof(__int128_t) + 2*8*sizeof(__int128_t*)))
    // Only change the change the allocation status and leave the rest of the size unchanged.
    destination->size = (destination->size | 0x1);

  // Otherwise allocate the new block but also put the remaining free space back into the free list as a new node.
  else {

    // Create a new node header for the remaining free space.
    node_header* remainder = (node_header*) ((char*)destination + size);

    // Udate its size and pointers to prev and next.
    remainder->size = (remaining_free_space | 0x6);
    remainder->prev = destination->prev;
    remainder->next = destination->next;

    // Update the header of the allocated block.
    destination->size = (size | 0x1); // header

    // Replace the old larger free block with the new smaller one then update the free list size.
    Replace_Free_Block(destination, remainder);
    free_list_size -= size;
  }

  return (char*)destination + 8*sizeof(struct node_header);
}

// ===================== methods for debugging =====================



// void printstatus() {
//   printf("\n ------------ STATUS -----------\n");
//   printf("Free List size is %d \n", free_list_size);
//   printf("head header: %d", free_list_head == NULL? 0:free_list_head->size);
//   printf("      size: %d\n", free_list_head->size & ~0xf  );

  
//   printf("second free block:");
//  node_header* second = free_list_head->next;
//   printf("   head: %d", second == NULL? 0: second->size);
//   printf("   size: %d\n", second == NULL? 0: second->size & ~0xf);
//   printf("----------END STATUS---------\n\n");
// }


// void print_free_list() {

//   printf("\nFree list has size %d\n", free_list_size);
//   int count = 0;
//   for (struct node_header* runner = free_list_head; runner != NULL; runner = runner->next, count++ ) {
//     printf("node %d:     ", count);
//     printf("%d <-----------------------------\n", runner->size);
//   }
// }