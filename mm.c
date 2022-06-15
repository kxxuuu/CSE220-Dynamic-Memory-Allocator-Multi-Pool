#include <string.h>
#include <stdio.h>
#include <unistd.h>
/* The standard allocator interface from stdlib.h.  These are the
 * functions you must implement, more information on each function is
 * found below. They are declared here in case you want to use one
 * function in the implementation of another. */
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void getPage(int free_list_idx);
void get_free_list();

/* When requesting memory from the OS using sbrk(), request it in
 * increments of CHUNK_SIZE. */
#define CHUNK_SIZE (1<<12)

/*
 * This function, defined in bulk.c, allocates a contiguous memory
 * region of at least size bytes.  It MAY NOT BE USED as the allocator
 * for pool-allocated regions.  Memory allocated using bulk_alloc()
 * must be freed by bulk_free().
 *
 * This function will return NULL on failure.
 */
extern void *bulk_alloc(size_t size);

/*
 * This function is also defined in bulk.c, and it frees an allocation
 * created with bulk_alloc().  Note that the pointer passed to this
 * function MUST have been returned by bulk_alloc(), and the size MUST
 * be the same as the size passed to bulk_alloc() when that memory was
 * allocated.  Any other usage is likely to fail, and may crash your
 * program.
 */
extern void bulk_free(void *ptr, size_t size);

/*
 * This function computes the log base 2 of the allocation block size
 * for a given allocation.  To find the allocation block size from the
 * result of this function, use 1 << block_size(x).
 *
 * Note that its results are NOT meaningful for any
 * size > 4088!
 *
 * You do NOT need to understand how this function works.  If you are
 * curious, see the gcc info page and search for __builtin_clz; it
 * basically counts the number of leading binary zeroes in the value
 * passed as its argument.
 */
static inline __attribute__((unused)) int block_index(size_t x) {
    if (x <= 8) {
        return 5;
    } else {
        return 32 - __builtin_clz((unsigned int)x + 7);
    }
}



typedef struct block {
    size_t header;
    void * actually_memory; // alloc to user (size - sozeof(size_t))
} Block;

Block ** free_list = NULL;


void getPage(int free_list_idx){
    void *ptr = sbrk(CHUNK_SIZE);
    Block *block = ptr;
    block->header = (1 << free_list_idx); // 2^free_list_idx

    // connect to next node (point to next)
    int block_num = CHUNK_SIZE / block->header;
    void *prev_block = (void*)block;
    for (int i = 0; i <= block_num - 1; i++) {
        if (i != block_num - 1){
                void *curr_b = prev_block + block->header;
                ((Block*)curr_b) -> header = block->header;
                ((Block *)prev_block)->actually_memory = curr_b;
                prev_block = curr_b;

        } else {
            // last node point to NULL
            ((Block *)prev_block)->actually_memory = NULL;
        }
    }
    free_list[free_list_idx] = block;
}

void get_free_list(){

    void *page = sbrk(CHUNK_SIZE);
    free_list = page;
    for (int i = 0; i < 13; i++){
        if (i >= 5){
            getPage(i);
        }
    }
}



Block * next_block(int idx) {
    Block * block = free_list[idx];
    if (block != NULL) {
        free_list[idx] = block->actually_memory;
    }
    return block;
}


/*
 * You must implement malloc().  Your implementation of malloc() must be
 * the multi-pool allocator described in the project handout.
 */
void *malloc(size_t size) {
    if (size == 0){
        return NULL;
    }
    if (free_list == NULL){
        get_free_list();
    }
    void * ptr = NULL;
    if ((size + sizeof(size_t)) > CHUNK_SIZE){
        ptr = bulk_alloc(size + sizeof(size_t));
        *(size_t*)ptr = size + sizeof(size_t);
        ptr += sizeof(size_t);
    } else {
        int free_list_index = block_index(size); // get index in free list
        Block * block = next_block(free_list_index);
        if (block == NULL){
            getPage(free_list_index);
            block = next_block(free_list_index); // position for user
        }

        ptr = ((void*)block + sizeof(size_t));
    }

    return ptr;
}



/*
 * You must also implement calloc().  It should create allocations
 * compatible with those created by malloc().  In particular, any
 * allocations of a total size <= 4088 bytes must be pool allocated,
 * while larger allocations must use the bulk allocator.
 *
 * calloc() (see man 3 calloc) returns a cleared allocation large enough
 * to hold nmemb elements of size size.  It is cleared by setting every
 * byte of the allocation to 0.  You should use the function memset()
 * for this (see man 3 memset).
 */
void *calloc(size_t nmemb, size_t size) {
    void *ptr = malloc(nmemb * size);
    memset(ptr, 0, nmemb * size);
    return ptr;
}

/*
 * You must also implement realloc().  It should create allocations
 * compatible with those created by malloc(), honoring the pool
 * alocation and bulk allocation rules.  It must move data from the
 * previously-allocated block to the newly-allocated block if it cannot
 * resize the given block directly.  See man 3 realloc for more
 * information on what this means.
 *
 * It is not possible to implement realloc() using bulk_alloc() without
 * additional metadata, so the given code is NOT a working
 * implementation!
 */
void *realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return NULL;
    }
    int free_list_index = block_index(size);
    Block * block = (ptr - sizeof(size_t));
    int old_size = (int)(block->header);
    int new_size = (1 << free_list_index);
    // increase size
    if (old_size < new_size){
        void * new_ptr = malloc(size);
        memcpy(new_ptr, ptr, (old_size - sizeof(size_t)));
        free(ptr);
        ptr = new_ptr;
    }
    // decrease size
    else {
        // do nothing
    }

    return ptr;
}


/*
 * You should implement a free() that can successfully free a region of
 * memory allocated by any of the above allocation routines, whether it
 * is a pool- or bulk-allocated region.
 *
 * The given implementation does nothing.
 */
void free(void *ptr) {
    if (ptr == NULL){
        return;
    }
    Block * block = ptr - sizeof(size_t);
    int block_size = (int)(block->header);
    if (block_size > CHUNK_SIZE){
        bulk_free(ptr - 8, block_size);
        return;
    }
    int free_list_idx = block_index(block_size - sizeof(size_t));
//    *(Block **)(& block->actually_memory) = free_list[free_list_idx];
    block->actually_memory = free_list[free_list_idx];
    free_list[free_list_idx] = block;
}


