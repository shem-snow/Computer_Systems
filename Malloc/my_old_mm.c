/*
 * mm-naive.c - The least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by allocating a
 * new page as needed.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused.
 *
 * TODO: Replace this header comment with your own header comment that gives a high level description of your solution (the details are in your notebook).
 * Minimimum page size is 4096 bytes
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
  struct node_header* prev;
  struct node_header* next;
} node_header;


/* ================================================== Global Variables ================================================================*/
const int PAGE_SIZE = 4096; 
__int128_t *free_list_head; // free_list_tail is the one whose "next" is null.
static size_t free_list_size;

/* ==================================================== Helper Methods ==============================================================*/
__int128_t* Best_Fit(__int128_t requested_size);
static void* coalesce(void *new_free_block);
void Replace_Free_Block(__int128_t* old, __int128_t* new);
void* Allocate_Block(__int128_t* destination, int size);

static void extend_free_list(__int128_t pg_size);

void printstatus(); // TODO: remove

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
  printf("\n!!!!!!!!!!!!!! in init() !!!!!!!!!!!!!!!!\n");
  // Initialize the free list to be empty (this program may be called after another)
  free_list_head = NULL;
  free_list_size = 0;

  // Initialize the heap area into its original state (a single page of the minimum size).
  extend_free_list(PAGE_SIZE);

  printf("\n!!!!!!!!!!!!!! back in init() !!!!!!!!!!!!!!!!\n");
  printstatus();
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
printf("\n___________________in malloc() ______________________________\n");
printf(" size of new data is %d", size_of_new_data);
  __int128_t size_of_new_block = ALIGN(size_of_new_data);
printf("size of new block is %d\n", size_of_new_block);
printstatus();
  // If the free list can't hold the new data then expand it.
  if (free_list_size < size_of_new_block) {
    __int128_t new_size = PAGE_ALIGN(size_of_new_block);
          printf("free list is not big enough for the new block. Extending the free list\n");
    // Append a new page to the start of the free list.
    extend_free_list(new_size - free_list_size);
    if (free_list_head == NULL)
      return NULL;
  }

  // Store the new data into the free-list
  __int128_t* store_location;
    
  // If there's a best fit then place it there.
  __int128_t* best_fit = Best_Fit(size_of_new_block);
printf("\n___________________ back in malloc() ______________________________ got the best fit.");
printstatus();
  if(best_fit != NULL) {
    store_location = best_fit;
    printf("there is a best fit with header: %d\n", *(__int128_t*) best_fit);
  }
  // Otherwise another page will have to be put at the beginning of the free-list.
  else {
  printf("best fit is null so the free list needs to be extended.\n");
    extend_free_list(size_of_new_block);
    store_location = free_list_head;
  printf("free_list_head == %d.\n", *(__int128_t*)free_list_head);
  
  
  }
printf("ready to allocate block of size %d ", size_of_new_block); printf("in a block with the header: %d\n", *(__int128_t*)store_location);
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
}

/* =============================================== Helper Methods ===================================================================*/

/*
 * Iterates through the free list until it finds the best place to fit a new block of the "requested_size".
 *
 * @returns a pointer to the start of the best-fitting free block for the "requested_size". That pointer will be NULL if no free block is large enough.
 */
__int128_t* Best_Fit(__int128_t requested_size) {
  printf("\n *********************** In Best fit ********************************\n");
printstatus();

  printf("the requested size is: %d\n", requested_size);
  // Create a pointer that will move alongside the free list to find the best-fitting free block.
  __int128_t* runner;
  __int128_t* best_fit = NULL;

int count = 0; // TODO: remove
  // Iterate through the free list.
  for (runner = free_list_head; runner != NULL; runner = (__int128_t*)*((__int128_t**)(runner + 8*sizeof(__int128_t) + 8*sizeof(__int128_t*))) ) {
    printf("starting to run");
    count++; // TODO: remove

    // Get the size of the current free block.
    int runner_size = ( *(__int128_t* )runner & ~0xF );
printf("\nrunner size is %d\nrequested size is %d \n", runner_size, requested_size);

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
printf("\nbest fit size is: %d\n", *(__int128_t* )best_fit & ~0xF );
    // Continue the optimization loop and compare the sizes of the current block with the best fitting block so far.
    if (runner_size < ( *(__int128_t* )best_fit & ~0xF ) ) {
        best_fit = runner;

    }

    
    
  }
printf("\nnumber of checked free blocks = %d\n", count);

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
static void* coalesce(void *new_free_block) {

  int size_of_new_free_block = *(int*) new_free_block & ~0xF;
  void* prev = new_free_block - *(int*)(new_free_block-8*sizeof(int)); // new_block - size of the previous block (arrives at the header of the previous block
  void* next = new_free_block + *(int*)(new_free_block); // new_block + size of the current block.

  // From this point, there are 4 cases:

  // Case 1: neither the previous nor next block are free.
  if( (*(unsigned int*)prev & 0x1) && (*(unsigned int*)next & 0x1)) {
    Replace_Free_Block(NULL, new_free_block); // Nothing to coalesce. Just add the "new_free_block" to the free list.
    free_list_size += size_of_new_free_block;
    return free_list_head;
  }
    

  // Case 2: The previous block is free but the next one is not.
  else if(!(*(unsigned int*)prev & 0x1) && (*(unsigned int*)next & 0x1)) {

    // Update the size and allocation bits of the previous block.
    free_list_size += size_of_new_free_block;
    int new_size = (*(int*)prev & ~0xF) + size_of_new_free_block; // previous_size += current_size;
    *(int*)prev = (new_size | 0x6); // Header
    *(int*)(new_free_block + size_of_new_free_block) = new_size | 0x6; // Footer
    
    // No need to adjust previous or next pointers. Also no need to change the free list.
    return prev;
  }

  // Case 3: The next block is free but the previous is not.
  else if( (*(unsigned int*)prev & 0x1) && !(*(unsigned int*)next & ~0x1)) {

    // Update the size and allocation bits of the "new_free_block".
    int size_of_block_to_remove = ( *(int*)next & ~0xF);
    free_list_size -= size_of_block_to_remove;
    int new_size = size_of_new_free_block + size_of_block_to_remove;
    *(int*)new_free_block = (new_size | 0x6); // header
    *(int*)(new_free_block + size_of_new_free_block + (*(int*)next & ~0xF ) ) = new_size | 0x6; // Footer
    free_list_size += new_size;

    // Update the free list
    Replace_Free_Block(next, new_free_block);

    return new_free_block;
  }

  
  // Case 4: Both the previous and the next block are free.
  // else {

  // This is just a combination of the logic in cases 2 and 3.
  int size_of_prev = *(int*)prev & ~0xF;
  int size_of_next = *(int*)next & ~0xF;
  int new_size = size_of_prev + size_of_new_free_block + size_of_next;

  *(int*)prev = (new_size | 0x6); // Header 
  *(int*)(new_free_block + size_of_new_free_block + (*(int*)next & ~0xF ) ) = new_size | 0x6; // Footer

  // Update the free list
  free_list_size = free_list_size - size_of_prev - size_of_next + new_size;
  Replace_Free_Block(next, prev);

  return prev;

  // }
}

/*
* Uses mem_map to allocate a new chunk of memory for the "requested_space".
* Updates the free_list_head and free_list_size.
* 
* CALLER AGREEMENT: The exact amount of "requested_space" will be allocated so it is up to the caller to make sure it is page-aligned.
* This method will make sure it is a multiple of 4096.
*/
static void extend_free_list(__int128_t requested_space) {
 printf("\n^^^^^^^^^^^^^^^^^^^^^^^^^ in extend_free_list^^^^^^^^^^^^^^^^^^^^^^^^^^^ free list size starts as: %d\n", free_list_size);

  // Make sure the requested space is a multiple of 4096
  __int128_t granted_space = (requested_space % 4096 == 0)? requested_space: ((requested_space / 4096) + 1) * 4096;

  // Create a pointer to the new head of the free list.
  __int128_t* new_head = mem_map(granted_space);

  // Store its size and allocation bits in the header and footer.
  *(__int128_t*) new_head = (__int128_t) ((granted_space) | 0x6); // header
  __int128_t* head_footer = (__int128_t*)((char*)new_head + granted_space - 8*sizeof(__int128_t) ); // Look at the footer
  *head_footer = *new_head; // footer
printf("\nrequested space: %d\n", requested_space); printf("granted space: %d\n", granted_space);
printf("\nnew head is: %d\n", *(__int128_t*)new_head); printf("its size is: %d\n", *(__int128_t*)new_head & ~0xf);printf("new footer is %d", *head_footer);
  // Point the previous pointer in the free block to NULL (since it is the new head).
  __int128_t* ptr_prev = (__int128_t*) ( (char*)new_head + 8*sizeof(__int128_t) );
  *(__int128_t**)ptr_prev = NULL;

  // Get a pointer at the "next" reference.
  __int128_t* ptr_next = (__int128_t*) ( (char*)new_head + 8*sizeof(__int128_t) + 8*sizeof(void*) );

  // If this is the first time the free list is being extended, then it should be NULL. Otherwise it should be the current free list head.
  *(__int128_t**)ptr_next = (free_list_head == NULL) ? NULL: free_list_head;

  // Set the head of the free list to this new block and update its size.
  free_list_head = new_head;
  free_list_size += granted_space;
printstatus();

  // Coalesce if the block happens to be next to other free blocks.
  // coalesce(new_head); // TODO: Assume this will never happen? If it does then ignore it?
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
void Replace_Free_Block(__int128_t* old, __int128_t* new) {
printf("\n=============== in Replace_Free_Block ========================\n");

printf("\nold = %d\n", (old == NULL)? 0:*old);
printf("new = %d\n", (new == NULL)? 0:*new);

printstatus();

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
      
      old->next->prev = new;
      new->prev = NULL;
      new->next = old->next;

      // Update the free list head.
      free_list_head = new;
    }
  }

  // Otherwise we're replacing some random node in the free list.
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
void* Allocate_Block(__int128_t* destination, int size) {
printf("\n########################    in allocate_block ################################\n");
  // Determine if the new block will split an existing free block or occupy the entire thing.
  int total_space = (*(__int128_t*)destination & ~0xF); 
  int remaining_free_space = total_space - size;
printf("size to allocate is : %d\n", size);
printf("total space is : %d\n", total_space);
printf("remaining free space is : %d\n", remaining_free_space);
printstatus();
  // If the remaining space is too small to be useful then let the new data take up the whole block
  if(remaining_free_space <= (2*8*sizeof(__int128_t) + 2*8*sizeof(__int128_t*))) {
    // Only change the header and footer to be allocated but don't change the size.
    *(__int128_t*) destination = (*(__int128_t*)destination | 0x1); // header
    *(__int128_t*) ((char*)destination + total_space - 8*sizeof(__int128_t) ) = ( *(__int128_t*)destination | 0x1 ); // footer
    printf("No remaining free space because the allocation takes up the entire block.\n");
  }
  // Otherwise allocate the new block but also put the remaining free space back into the free list.
  else {
    // Update the header and footer of the allocated block.
    *(__int128_t*) destination = (size | 0x1); // header
printf("About to allocate block with head: %d\n", *(__int128_t*)destination);
    *(__int128_t*) ( (char*)destination + size - 8*sizeof(__int128_t) ) = (size | 0x1); // footer
printf("and foot: %d\n", *(__int128_t*)( (char*)destination + size - 8*sizeof(__int128_t) ) );
printf("remaining free block has head: %d\n", *(__int128_t*)((char*)destination + size));
printf("and foot: %d\n", *(__int128_t*)((char*)destination + total_space - 8*sizeof(__int128_t) ) );

    // Update the header and footer of the free block.
    *(__int128_t*) ((char*)destination + size) = (remaining_free_space | 0x6); // header
    *(__int128_t*)((char*)destination + total_space - 8*sizeof(__int128_t) ) = remaining_free_space | 0x6; // footer

printf("now it has head: %d\n", *(__int128_t*)((char*)destination + size));
printf("and foot: %d\n", *(__int128_t*)((char*)destination + total_space - 8*sizeof(__int128_t) ) );

    // Replace the old larger free block with the new smaller one.
printstatus();
    Replace_Free_Block(NULL, (__int128_t*)((char*)destination + size )); // Add the new one
    Replace_Free_Block(destination, NULL); // Remove the old one

    // Update the free list size
    free_list_size -= size;
  }


printf("allocated block has head: %d\n", *(__int128_t*) destination);
printf("and foot: %d\n", *(__int128_t*)((char*)destination + size - 8*sizeof(__int128_t)) );

printstatus();
  return destination + (8*sizeof(__int128_t));
}

// TODO: remove all printf statements.

void printstatus() {
  printf("\n ------------ STATUS -----------\n");
  printf("Free List size is %d \n", free_list_size);
  printf("head header: %d", free_list_head == NULL? 0:*(__int128_t*)free_list_head);
  printf("      size: %d\n", *(__int128_t*) ( (char*)free_list_head) & ~0xf  );
  __int128_t* foot = (__int128_t*) ( (char*)free_list_head + (*(__int128_t*)free_list_head & ~0xf) - 8*sizeof(__int128_t) ); // TODO: segmentation fault occurs because free list head is a huge number.
  printf("head footer: %d", (foot == NULL)? 0: *(__int128_t*)foot);
  printf("     size: %d\n", *(__int128_t*) ( (char*)free_list_head) & ~0xf  );

  
  printf("second free block:");
  __int128_t* second = (__int128_t*)( (char*)free_list_head + ( *(__int128_t* )free_list_head & ~0xf) );
  printf("   head: %d", *second);
  printf("   size: %d\n", second == NULL? 0: *second & ~0xf);
  printf("----------END STATUS---------\n\n");
}

/* TODO: performance improvements:d
* - Double malloc size every time (unless new size is bigger than 2^x. In which case, match the reqested size).
* - Also keep track of biggest item so you don't have to find the best fit.
*/