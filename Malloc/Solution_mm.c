/*
 * mm-naive.c - The least memory-efficient malloc package.
 *
 * This is an explicit free list approach for malloc.
 * Using a linked list from one unallocated block to the next it can find a spot of memory of the requested size
 * If there is no spot avaible that can it it it will extend by adding a new page and placing it in the front of the freelist
 * doubling each time it sends a request for more memory. It can coalesce if neighboring blocks are also free when calling  mm_free.
 * When an entire page us unallocated it will unmap it and update the free-list accordingly. 
 * Each block has a header and footer used for coalescing.  
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

/* always use 16-byte alignment */
#define ALIGNMENT 16

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

/* rounds up to the nearest multiple of mem_pagesize() */
#define PAGE_ALIGN(size) (((size) + (mem_pagesize()-1)) & ~(mem_pagesize()-1))

//Some macros the Assignment gave us that I used
#define GET_SIZE(p) ((block_header *)(p))->size
#define GET_ALLOC(p) ((block_header *)(p))->allocated
#define HDRP(bp) ((char *)(bp) - sizeof(block_header))
#define FTRP(bp) ((char *)(bp)+GET_SIZE(HDRP(bp))-OVERHEAD)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE((char *)(bp)-OVERHEAD))
//Over Head is 32 because Header and footer are each 16 bytes
#define OVERHEAD 32

//So it knows list Remove will be defined below
void list_remove(void*bp);

//structs from videos! going to be used for my header!
typedef struct {
size_t size;
char allocated;
} block_header;

typedef struct{
size_t size;
int filler;
} block_footer;
//Struct for Page list
typedef struct{
  void *next_page;
  int filler;
} next_p;
//Struct for the Free list list
typedef struct{
  void *pred;
  void *succ;
} free_list;


void *free_l;
size_t next_size;// Next size for page mapping
size_t max_size = 100 * 4096;// Max size for page mapping

/*
Helper method used for extending the the free list by adding more pages!
This is called in init and Malloc
Size: size being requested
*/
void* extend(size_t size){
  //Get a get how much the mem they are requesting
  size = PAGE_ALIGN((size *4)+ OVERHEAD+16);
  //If its less than next size set it to next size
  if(size < next_size){
    size = next_size;
  }
  //Dont let size be over max_size
  if(size > max_size){
    size = max_size;
  }
  //Double the next size
  next_size = PAGE_ALIGN(next_size *2);

  //Get new page using mem_map
  void * bp = mem_map(size);

  //Move 16 forward to make space for Prolong header
  bp = bp + 16;
  //Add Prolong header and footer
  GET_SIZE(HDRP(bp)) = 32; 
  GET_ALLOC(HDRP(bp))= 1;
  GET_SIZE(FTRP(bp)) = 32;
  //Go to start of free space
  bp = bp + OVERHEAD;
  //Set up first header to show the entire page is unallocated and set proper size
  GET_SIZE(HDRP(bp)) = size-(OVERHEAD+16); 
  GET_ALLOC(HDRP(bp))= 0;
  GET_SIZE(FTRP(bp)) = size-(OVERHEAD+16);
  
  //Set up the term block
  void* temp_bp = NEXT_BLKP(bp);
  GET_SIZE(HDRP(temp_bp)) = 0;
  GET_ALLOC(HDRP(temp_bp))= 1;
  
  //Add page to the front of the free_list!
  ((free_list*)bp)->succ = free_l;
  ((free_list*)bp)->pred = NULL;
  if(free_l){//In case free_l was NULL
    ((free_list*)free_l)->pred = bp;
  }
  free_l = bp;
  //Returning the first usefull payload location.
  return bp;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
  //Might not need first page anymore***
  //Set start of the free list to be NULL
  free_l = NULL;
  //Set first next_size to one page
  next_size = 4096;
  //void * first_bp = extend(4096);
  extend(4096);
  return 0;
}

/*
* This is used for allocating a chunk of memory of size of new_size starting at bp pointer in memory
*/
void set_allocated(void *bp, size_t new_size){
  //Get what the original size is and find out how much space will be remaining
  size_t old_size = GET_SIZE(HDRP(bp));
  size_t remaining = old_size - new_size;

  //get next free Blocks from free list.
  void* pred = ((free_list*)bp)->pred;
  void* succ = ((free_list*)bp)->succ;

  //If has no usable space remaining remove it from the free list!
  if(remaining < 48){
    new_size = old_size;
    GET_ALLOC(HDRP(bp))= 1;
    list_remove(bp);

  }else{//It had usable space still remaining so adjust freelist to its new location! And set up remaing space header /footer
    //SET footer of orginal free block to be updated.
    GET_SIZE(FTRP(bp)) = remaining;

    //Set up size and allocated
    GET_SIZE(HDRP(bp)) = new_size;
    GET_ALLOC(HDRP(bp))= 1;
    GET_SIZE(FTRP(bp)) = new_size;

    //set up a new header for remaining memory.
    void * temp_bp = NEXT_BLKP(bp);
    GET_SIZE(HDRP(temp_bp)) = remaining;
    GET_ALLOC(HDRP(temp_bp))= 0;
    //If it is the start of the free list

    //Update the free list of is new location
    ((free_list*)temp_bp)->succ =succ;
    ((free_list*)temp_bp)->pred =pred;
    if(pred){
      ((free_list*)pred)->succ = temp_bp;
    }
    if(succ){
      ((free_list*)succ)->pred = temp_bp;
    }
    if(bp == free_l){// In case it was the start of the free list!
      free_l = temp_bp;
      ((free_list*)temp_bp)->pred = NULL;
      ((free_list*)temp_bp)->succ = succ;
    } 
    
  }

}

/* 
 * mm_malloc - Allocate a block by using bytes from free list,
 *     grabbing a new page if necessary.
 */
void *mm_malloc(size_t size)
{
  //How much space it needs
  int new_size = ALIGN(size+OVERHEAD);
  //Get first node from free_l
  void*bp = free_l;

  //While there is another node in free list
  while(bp != NULL){
    if((GET_SIZE(HDRP(bp)) >= new_size)) {// If it can fit in this node
      //allocate
      set_allocated(bp,new_size);
      return bp;
    }
    bp = ((free_list*)bp)->succ;
  }
  //if it gets Here we need to add a new page.
  bp = extend(new_size);
  //Allocate in newly added page
  set_allocated(bp,new_size);

  return bp;
}

/*
Helper method for removing an item from the free linked list.
*/
void list_remove(void *bp){
  
  void* n_succ = ((free_list*)bp)->succ;
  void* n_pred = ((free_list*)bp)->pred;
  //If n_succ is valid set succ's pred to the next
  //Case 1: It is the whole ass list
  if((n_succ == NULL) && (n_pred == NULL)){
    free_l = NULL;
    return;

  }else if((n_succ != NULL) && (n_pred != NULL)){//Case 2: Its in the middle of the free list
    ((free_list*)n_pred)->succ = n_succ;
    ((free_list*)n_succ)->pred = n_pred;
    return;

  }else if(n_pred != NULL){// Case 3: at the end of the list
    ((free_list*)n_pred)->succ = NULL;
    return;
  }else if(n_succ != NULL){// We are at the start of the list
    ((free_list*)n_succ)->pred = NULL;
    free_l = n_succ;
    return;
  }
}
/*
Helper method to coalesce if two free blocks are next to each other!
*/
void* coalesce(void * bp){
  size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  //Case 1 neither side is free
  if(prev_alloc && next_alloc){
    // Sets up BP to tbe the new start to the free list
    ((free_list*)bp)->succ =free_l;
    ((free_list*)bp)->pred = NULL;
    if(free_l){
      ((free_list*)free_l)->pred =bp;
    }
    free_l = bp;

  } else if(prev_alloc && !next_alloc){
    // case 2: the next block is free
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    //Remove next node because we are adding it to the current
    list_remove(NEXT_BLKP(bp));
    GET_SIZE(HDRP(bp)) = size;
    GET_SIZE(FTRP(bp)) = size;
    //Made the current location the start of the Free list
    ((free_list*)bp)->succ =free_l;
    ((free_list*)bp)->pred = NULL;
    if(free_l){
      ((free_list*)free_l)->pred =bp;
    }
    free_l = bp;
    
  } else if(!prev_alloc && next_alloc){
    // case 3 The Pred block is free, Just update size of Prev block
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    GET_SIZE(HDRP(PREV_BLKP(bp))) = size;
    GET_SIZE(FTRP(bp)) = size;
    bp = PREV_BLKP(bp);
  } else {
    //case 4: Both prev and next block are unallocated so merge everything togeathe
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
    //removing the next block from the free list
    list_remove(NEXT_BLKP(bp));
    GET_SIZE(HDRP(PREV_BLKP(bp))) = size;
    GET_SIZE(FTRP(NEXT_BLKP(bp))) = size;
    bp = PREV_BLKP(bp);
  }
  
  return bp;
}

/*
 * mm_free - Given a pointer it unallocates and coaleseces that location.
 */
void mm_free(void *ptr)
{
  
  void * bp = coalesce(ptr);
  GET_ALLOC(HDRP(bp)) = 0;
  //Checks if we can unmap the page. 
  if((GET_SIZE(HDRP(NEXT_BLKP(bp))) == 0) && (GET_SIZE(HDRP(PREV_BLKP(bp)))== 32)){
    list_remove(bp);
    size_t size = (GET_SIZE(HDRP(bp))) + OVERHEAD + 16;
    mem_unmap(PREV_BLKP(bp)-16,size);
    
  }
}
