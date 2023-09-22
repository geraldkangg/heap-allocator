/* This file contains the explicit list allocator which uses the first in first out implementation
 * for the linked list. It properly allocates memory in the heap depending on free block sizes along
 * with having extra functionality such as coalesing free blocks, inplace reallocation, and a linked
 * list of free blocks. The memory address for the prev free block is stored in the first 8 bytes after
 * the header of the memory block while the next free block is stored in the following 8 bytes. This results
 * in this allocator needing a minimum size of 16 bytes to be allocated.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./allocator.h"
#include "./debug_break.h"

#define MINHEAP 24
#define MINSIZE 16
#define SEVEN 7
#define SIX 6

static void *segment_start;
static void *segment_end;
static void *first_free;
static void *last_free;
static size_t segment_size;
static size_t nused;

/* This is the given function which rounds up a respective size to the nearest multiple.
 */
size_t roundup(size_t sz, size_t mult) {
    return (sz + mult - 1) & ~(mult - 1);
}

/* This function gets the size of the memory block at the current header.
 */
size_t get_size(void *curHeader) {
    unsigned long bitmask = ~SEVEN;
    size_t size = *(unsigned long *)curHeader & bitmask;
    return size;
}

/* This function rounds the size to the nearest multiple. If the size is less than the MINSIZE,
 * this function sets the size as the MINSIZE. It returns the final rounded size.
 */
size_t total_round(size_t sz) {
    size_t size = roundup(sz, ALIGNMENT);
    if (size < MINSIZE) {
        size = MINSIZE;
    }
    return size;
}

/* This function adds the size of the current header and the next position together and copies
 * this new size into the current header.
 */
void coalesce(void *curHeader, void *next_pos) {
    size_t new_size = get_size(next_pos) + get_size(curHeader) + ALIGNMENT;   
    memcpy(curHeader, &new_size, ALIGNMENT);
}

/* This function links the next_free and prev_free free blocks together in the linked list
 * and updates the global paramaters respectively.
 */
void update_free(void *next_free, void *prev_free) {
    if (*(char **)prev_free) {
        // If there is a previous free block
        memcpy(*(char **)prev_free + MINSIZE, next_free, ALIGNMENT);
    } else {
        // Set the first free block as the next_free block
        first_free = *(char **)next_free;
    }
    if (*(char **)next_free) {
        // If there is a next free block
        memcpy(*(char **)next_free + ALIGNMENT, prev_free, ALIGNMENT);
    } else {
        // Set the last free block as the prev_free block
        last_free = *(char **)prev_free;
    }
}

/* This function updates the global paramters, first_free and last_free depending on the newly created
 * current header.
 */
void update_param(void *curHeader) {
    if (!first_free) {
        // If the current header is the first free block
        first_free = curHeader;
        memset((char *)curHeader + ALIGNMENT, 0, ALIGNMENT);
    } else {
        memcpy((char *)curHeader + ALIGNMENT, &last_free, ALIGNMENT);
        memcpy((char *)last_free + MINSIZE, &curHeader, ALIGNMENT);
    }
    last_free = curHeader;
}

/* This function iterates through the next free blocks and properly adds the size to the new_free header.
 */
void sum_free(void *next_pos, void *new_free) {
    while ((*(unsigned long *)next_pos & SEVEN) == 0 && next_pos != segment_end) {
        update_free((char *)next_pos + MINSIZE, (char *)next_pos + ALIGNMENT);
        coalesce(new_free, next_pos);
        next_pos = (char *)new_free + get_size(new_free) + ALIGNMENT;
    }
}

/* This function takes in a pointer to the current header and creates a new block
 * at the next possible location (after needed bytes and ALIGNMENT). It also
 * properly intializes the next_free block as 0 and returns a pointer to the new 
 * free block header.
 */
void *create_new_block(void *curHeader, size_t needed, size_t x) {
    void *new_free = (char *)curHeader + needed + ALIGNMENT;
    size_t size = x - ALIGNMENT;
    memcpy(new_free, &size, ALIGNMENT);
    *(unsigned long *)new_free &= ~1;
    memset((char *)new_free + MINSIZE, 0, ALIGNMENT);
    return new_free;
}

/* This function handles the situation where a memory block's size passes the segment end.
 * This function proplery updates the nused and segment_end variables and returns old_ptr
 * once the block is updated or NULL if it is not possible.
 */
void *create_block_past_end(void *curHeader, size_t needed, void *old_ptr) {
    size_t block_size = get_size(curHeader);
    memcpy(curHeader, &needed, ALIGNMENT);
    *(unsigned long *)curHeader |= 1;
    nused += needed - block_size;
    // Extend nused
    if (nused > segment_size) {
        // If we need more blocks than available
        return NULL;
    }
    segment_end = (char *)segment_start + nused;
    return old_ptr;
}

/* This function checks the respective null cases for myrealloc and returns
 * either the pointer to the newly allocated block (when !old_ptr) or NULL if it frees the 
 * old_ptr (when old_ptr and new_size == 0).
 */
void *null_case(void *old_ptr, size_t new_size) {
    if (!old_ptr) {
        if (new_size == 0) { return NULL; }
        return mymalloc(new_size);
    } else {
        myfree(old_ptr);
        return NULL;
    }
}

/* This function is called when we cannot realloc in place. It essentially
 * recalls mymalloc and returns a pointer to the new location.
 */
void *remalloc(size_t new_size, void *old_ptr) {
    void *new_ptr = mymalloc(new_size);
    memcpy(new_ptr, old_ptr, new_size);   
    myfree(old_ptr);
    return new_ptr;
}

/* This function initializes the heap. It takes in a pointer to the start of the heap and its
 * size and properly initializes the global variables and returns true.
 * If the user inputs an invalid pointer of size, then the function returns false.
 */
bool myinit(void *heap_start, size_t heap_size) {
    if (!heap_start || heap_size < MINHEAP) {
        return false;
    }
    segment_start = heap_start;
    segment_end = heap_start;
    segment_size = heap_size;
    first_free = NULL;
    last_free = NULL;
    nused = 0;
    return true;
}

/* Mymalloc function takes in a requested size and allocates its respective size into the heap.
 * It iterates through all of the free blocks before allocating at the end of all initialized 
 * blocks. Mymalloc returns a pointer to the memory location of the requested size.
 */
void *mymalloc(size_t requested_size) {
    size_t needed = total_round(requested_size);
    if (requested_size == 0 || needed + nused > segment_size) {
        return NULL;
    }
    void *cur_free = first_free;
    while (cur_free) {
        // Iterating through all free blocks
        void *ptr = (char *)cur_free + ALIGNMENT;
        size_t size = get_size(cur_free);       
        if (size >= needed) {
            // If free block has space for requested size
            update_free((char *)cur_free + MINSIZE, (char *)cur_free + ALIGNMENT);
            memcpy(cur_free, &needed, ALIGNMENT);
            size_t x = size - needed;
            if (x > MINSIZE) {
                // Create new free block if there is leftover space
                void *new_free = create_new_block(cur_free, needed, x);
                if (!first_free) {
                    // If there are no free blocks
                    first_free = new_free;
                    memset((char *)new_free + ALIGNMENT, 0, ALIGNMENT);
                    last_free = new_free;
                } else {
                    memcpy((char *)new_free + ALIGNMENT, &last_free, ALIGNMENT);
                    memcpy((char *)last_free + MINSIZE, &new_free, ALIGNMENT);
                    last_free = new_free;
                }
            } else {
                // If space is not enough for a new block, use as padding
                memcpy(cur_free, &size, ALIGNMENT);
            }
            *(unsigned long *)cur_free |= 1;
            return ptr;
        }
        // Go to next free block
        cur_free = *(char **)((char *)cur_free + MINSIZE);
    }
    // Else create a new memory block at the end of the initialized blocks
    void *header = (char *)segment_start + nused;
    memcpy(header, &needed, ALIGNMENT);
    *(unsigned long *)header |= 1;
    nused += needed + ALIGNMENT;
    void *ptr = (char *)header + ALIGNMENT;
    segment_end = (char *)ptr + needed;
    return ptr;
}

/* myfree function takes in a pointer to a specific memory location and properly
 * frees its header.
*/
void myfree(void *ptr) {
    if (!ptr) {
        return;
    }
    void *curHeader = (char *)ptr - ALIGNMENT;
    *(unsigned long *)curHeader &= ~1;
    size_t size = get_size(curHeader);
    void *next_pos = (char *)curHeader + size + ALIGNMENT;
    memset((char *)curHeader + MINSIZE, 0, ALIGNMENT);

    // Coalesce all free blocks to the right
    sum_free(next_pos, curHeader);
    update_param(curHeader);
}

/* This is my realloc function which takes in an old_ptr and reallocates it to a newly
 * requested size. It returns NULL if new_size is 0 otherwise this function returns a pointer
 * to the newly reallocated memory block.
*/
void *myrealloc(void *old_ptr, size_t new_size) {
    if (!old_ptr || new_size == 0) {
        return null_case(old_ptr, new_size);
    }
    void *curHeader = (char *)old_ptr - ALIGNMENT;
    size_t old_size = get_size(curHeader);
    void *next_pos = (char *)curHeader + ALIGNMENT + old_size;
    size_t needed = total_round(new_size);
    // If there is no need to realloc
    if (needed == old_size) { return old_ptr; }  
    if (old_size > needed) {
        // Reallocating to a smaller size
        size_t x = old_size - needed;
        if (x > MINSIZE) {
            memcpy(curHeader, &needed, ALIGNMENT);
            *(unsigned long *)curHeader |= 1;
            // Create new block for remaining space
            void *new_free = create_new_block(curHeader, needed, x);
            sum_free(next_pos, new_free);        
            update_param(new_free);
        }
        return old_ptr;
    } else {
        // If requested size is greater than initial size
        while ((*(unsigned long *)next_pos & SEVEN) == 0 && next_pos != segment_end) {
            update_free((char *)next_pos + MINSIZE, (char *)next_pos + ALIGNMENT);
            coalesce(curHeader, next_pos);
            size_t total_size = get_size(curHeader);
            if (total_size >= needed) {
                size_t x = total_size  - needed;
                if (x > MINSIZE) {
                    // If a new free block can be made
                    memcpy(curHeader, &needed, ALIGNMENT);
                    *(unsigned long *)curHeader |= 1;
                    void *new_free = create_new_block(curHeader, needed, x);
                    update_param(new_free);
                } else {
                    *(unsigned long *)curHeader |= 1;
                }
                return old_ptr;
            }
            next_pos = (char *)curHeader + get_size(curHeader) + ALIGNMENT;
        }
    }
    // If the next block is neither free nor allocated
    if (next_pos == segment_end) { return create_block_past_end(curHeader, needed, old_ptr); }
    // If inplace reallocation is not possible
    return remalloc(new_size, old_ptr); 
}

bool validate_heap() {
    /* TODO(you!): remove the line below and implement this to
     * check your internal structures!
     * Return true if all is ok, or false otherwise.
     * This function is called periodically by the test
     * harness to check the state of the heap allocator.
     * You can also use the breakpoint() function to stop
     * in the debugger - e.g. if (something_is_wrong) breakpoint();
     */
    if (nused > segment_size) {
        printf("used more heap than available\n");
        breakpoint();
        return false;
    }
    unsigned char *cur = segment_start;
    while (cur != segment_end) {
      if ((SIX & *(unsigned long *)cur) != 0) {
          printf("bytes overwritten\n");
          breakpoint();
          return false;
      }
      size_t size = get_size(cur);
      cur = (unsigned char *)cur + size + ALIGNMENT;
    }
    return true;
}

/* Function: dump_heap
 * -------------------
 * This function prints out the the block contents of the heap.  It is not
 * called anywhere, but is a useful helper function to call from gdb when
 * tracing through programs.  It prints out the total range of the heap, and
 * information about each block within it.
 */
void dump_heap() {
    printf("Heap segment starts at address %p, ends at %p. %lu bytes currently used.",
           segment_start, (char *)segment_start + segment_size, nused);
    int i = 0;
    unsigned char *cur = segment_start;
    while (cur != segment_end) {
        size_t size = get_size(cur);
        printf("\n%p size=%ld", cur, size);
        i += size + ALIGNMENT;
        cur = (unsigned char *)segment_start + i;
    }
}
