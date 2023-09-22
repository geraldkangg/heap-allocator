/* This file contains the implicit list allocator. This uses first in, first out functionality and 
 * iterates through every block header, allocating the requested size into the first possible free
 * block. If possible, it creates a new block in the remaining space.
 */
#include <stdio.h>
#include <string.h>
#include "./allocator.h"
#include "./debug_break.h"

#define BYTES_PER_LINE 32
#define MINSIZE 16
#define SEVEN 7
#define SIX 6

static void *segment_start;
static void *segment_end;
static size_t segment_size;
static size_t nused;

/* This function is taken from the starter files and rounds up a size to the nearest multiple of mult
 */
size_t roundup(size_t sz, size_t mult) {
    return (sz + mult - 1) & ~(mult - 1);
}

/* This function gets the size of the memory block at the current header
 */
size_t get_size(void *curHeader) {
    unsigned long bitmask = ~SEVEN;
    size_t size = *(unsigned long *)curHeader & bitmask;
    return size;
}

/* This function takes in a pointer to the start of the heap and a heap size
 * and returns if they are valid.
 */
bool myinit(void *heap_start, size_t heap_size) {
    if (!heap_start || heap_size < MINSIZE) {
        return false;
    }
    segment_start = heap_start;
    segment_end = heap_start;
    segment_size = heap_size;
    nused = 0;
    return true;
}

/* This function memory allocates the requested size into the heap. It creates
 * a 8 byte header which stores the size and allocation status of the memory block
 * which can be used to recycle the memory block later on. This function then returns
 * a pointer to the memory location which the user can use as the heap.
 */
void *mymalloc(size_t requested_size) {
    int i = 0;
    void *curHeader = segment_start;   
    size_t needed = roundup(requested_size, ALIGNMENT);
    // round requested_size to a factor of 8
    if (requested_size == 0) {
        // if user inputted size of 0
        return NULL;
    }
    while (curHeader != segment_end) {
        // Iterate through all possible blocks
        size_t size = get_size(curHeader);        
        if ((*(unsigned long *)curHeader & 1) == 0) {
            // if memory block is free
            if (size >= needed) {
                // if memory block can store requested size
                memcpy(curHeader, &needed, ALIGNMENT);
                *(unsigned long *)curHeader |= 1;
                void *ptr = (char *)curHeader + ALIGNMENT;
                if (size - needed > ALIGNMENT) {
                    // if leftover space is 16 bytes or greater
                    void *newHeader = (char *)curHeader + needed + ALIGNMENT;
                    size_t space = size - needed - ALIGNMENT;
                    memcpy(newHeader, &space, ALIGNMENT);
                    *(unsigned long *)newHeader &= ~1;
                    // create a new header representing leftover space as another block
                } else if (size - needed == ALIGNMENT) {
                    // if leftover space is exactly 8
                    *(size_t *)curHeader += ALIGNMENT;
                    // create as padding
                }                 
                return ptr;
            }
        }
        curHeader = (char *)curHeader + size + ALIGNMENT;
        i += size + ALIGNMENT;
        // iterate header
    }
    if (needed + ALIGNMENT + nused > segment_size) {
        // if needed size (with header) exceeds the heap size
        return NULL;
    }    
    void *header = (char *)segment_start + nused;
    memcpy(header, &needed, ALIGNMENT);
    *(unsigned long *)header |= 1;
    nused += needed + ALIGNMENT;
    void *ptr = (char *)header + ALIGNMENT;
    segment_end = (char *)ptr + needed;
    // create a new memory block
    return ptr;
}

/* This function frees the memory space specified by the ptr inputted by the user
 */
void myfree(void *ptr) {
    if (!ptr) {
        return;
    }
    void *curHeader = (char *)ptr - ALIGNMENT;
    unsigned long bitmask = ~1;
    *(unsigned long *)curHeader &= bitmask;
}

/* This function reallocates a memory space to a new size in the heap. It takes
 * in the pointer to the old memory location, and the new size and returns a 
 * ptr to the new memory location with the new size.
 */
void *myrealloc(void *old_ptr, size_t new_size) {
    if (!old_ptr) {
        // Handle case where inserted ptr is NULL
        if (new_size == 0) { return NULL; }
        return mymalloc(new_size);
    }
    if (new_size == 0 && old_ptr) {
        // Free ptr if new_size == 0
        myfree(old_ptr);
        return NULL;
    }
    void *new_ptr = mymalloc(new_size);
    memcpy(new_ptr, old_ptr, new_size);
    myfree(old_ptr);
    return new_ptr;
}
/* This function checks the validity of the heap. First checks if the heap use more than
 * available. 
 */
bool validate_heap() {
    if (nused > segment_size) {
        printf("used more heap than available\n");
        breakpoint();
        return false;
    }
    unsigned char *cur = segment_start;
    while (cur != segment_end) {
        // checks if bits were overwritten
        unsigned long bitmask = SIX;
        if ((bitmask & *(unsigned long *)cur) != 0) {
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
