// ____________________________________  replace  __________________________________________________________

// TODO: old code under here.
void Replace_Free_Block(__int128_t* old, __int128_t* new) {
if(1) {
  // Update the free list head.
  __int128_t* old_head = free_list_head;
  free_list_head = new;
        printf("old head: %d\n", *(__int128_t*)old_head);
        printf("new head: %d\n", *(__int128_t*)free_list_head);
        printf("new: %d\n", *(__int128_t*)new);

    // Update the prev and next pointers
    __int128_t* old_prev = (__int128_t*) ((char*)old_head + 8*sizeof(__int128_t));
    *old_prev = new; // TODO: right here.
        printf("\nold.prev == %d\n", *(__int128_t**) old_prev );

    __int128_t* new_prev = (__int128_t*)((char*)new + 8*sizeof(__int128_t));
    new_prev = NULL;
        printf("new.prev == %d\n", (new_prev == NULL)?0:*(__int128_t**) new_prev );

    __int128_t* new_next = (__int128_t*) ((char*)new + 8*sizeof(__int128_t) + 8*sizeof(__int128_t*));
    new_next = old_head;
        printf("new.next == %d\n", (new_next == NULL)?0:*(__int128_t**) new_next);

        printf("\nfree list has head: %d\n", *free_list_head);
        printf("its previous has: %d\n", *((char*)free_list_head + 8*sizeof(__int128_t)) );
        printf("its next has: %d\n", *(__int128_t**) ((char*)free_list_head + 8*sizeof(__int128_t)) + 8*sizeof(__int128_t*) );

    return;
  }
// else there is something to replace

  // Determine where the new next and previous nodes are.
  __int128_t* prev = old + 8*sizeof(__int128_t);
  __int128_t* next = old + 8*sizeof(__int128_t) + 8*sizeof(__int128_t*);

  // If we are replacing the head of the free list or if the first one is allocated.
  if(*prev == NULL) {

    // If we're removing the whole thing
    if(new == NULL) {
      mm_init();
      return;
    }
    // Else give the remaining free block pointers to prev and next.
    *(__int128_t*) (new + 8*sizeof(__int128_t)) = prev; // prev
    *(__int128_t*) (new + 8*sizeof(__int128_t) + 8*sizeof(__int128_t*)) = next; // next

    // Update the free list head to not include the allocated block.
    free_list_head = new;

    return;
  }


    // prev.next = (we're removing the current node)? next:new;
  __int128_t* prev_next = (__int128_t*) ((char*)prev + 8*sizeof(__int128_t) + 8*sizeof(__int128_t*) );
  *(__int128_t**) prev_next = (new == NULL)? next:new; 

  // If this is the end of the free list then immediately return to avoid NULL pointer error
  if(next == NULL) 
    return;
  
  // next.prev = prev;
  __int128_t* next_prev = (__int128_t*) ((char*)next + 8*sizeof(__int128_t) );
  *(__int128_t**)next_prev = prev; 

  printf("-------free list head header is %d------\n", *(__int128_t*)free_list_head);
}


    // ____________________________________    __________________________________________________________