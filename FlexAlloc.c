#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include "FlexAlloc.h"



#define SIZE 100000
int myArray[SIZE];  // global array of 100,000 integers



/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block.
 */
typedef struct blockHeader {

    /*
     * 1) The size of each heap block must be a multiple of 8
     * 2) heap blocks have blockHeaders that contain size and status bits
     * 3) free heap block contain a footer, but we can use the blockHeader
     *.
     * All heap blocks have a blockHeader with size and status
     * Free heap blocks have a blockHeader as its footer with size only
     *
     * Status is stored using the two least significant bits.
     *   Bit0 => least significant bit, last bit
     *   Bit0 == 0 => free block
     *   Bit0 == 1 => allocated block
     *
     *   Bit1 => second last bit
     *   Bit1 == 0 => previous block is free
     *   Bit1 == 1 => previous block is allocated
     *
     * Start Heap:
     *  The blockHeader for the first block of the heap is after skip 4 bytes.
     *  This ensures alignment requirements can be met.
     *
     * End Mark:
     *  The end of the available memory is indicated using a size_status of 1.
     *
     * Examples:
     *
     * 1. Allocated block of size 24 bytes:
     *    Allocated Block Header:
     *      If the previous block is free      p-bit=0 size_status would be 25
     *      If the previous block is allocated p-bit=1 size_status would be 27
     *
     * 2. Free block of size 24 bytes:
     *    Free Block Header:
     *      If the previous block is free      p-bit=0 size_status would be 24
     *      If the previous block is allocated p-bit=1 size_status would be 26
     *    Free Block Footer:
     *      size_status should be 24
     */
    int size_status;

} blockHeader;

/* Global variable - DO NOT CHANGE NAME or TYPE.
 * It must point to the first block in the heap and is set by init_heap()
 * i.e., the block at the lowest address.
 */
blockHeader *heap_start = NULL;

/* Size of heap allocation padded to round to nearest page size.
 */
int alloc_size;

/**
 * Retrieves a bitmask that clears the status bits in a block header's size_status.
 *
 * Returns:
 * - A bitmask that can be used to clear the status bits of size_status.
 */
int getSizeClearMask() {
    return ~3;
}

/**
 * Checks if the previous block is allocated.
 *
 * Pre-conditions:
 * - hdr: Pointer to the block header.
 *
 * Returns:
 * - 1 if the previous block is allocated, 0 otherwise.
 */
int isPrevAlloc(blockHeader *hdr) {
    return hdr->size_status & 2;
}

/**
 * Checks if a block is allocated.
 *
 * Pre-conditions:
 * - hdr: Pointer to the block header.
 *
 * Returns:
 * - 1 if the block is allocated, 0 otherwise.
 */
int isAlloc(blockHeader *hdr) {
    return hdr->size_status & 1;
}


/**
 * Retrieves the size of the block from its block header.
 *
 * Pre-conditions:
 * - hdr: Pointer to the block header.
 *
 * Returns:
 * - Size of the block in bytes.
 */
int getBlkSize(blockHeader *hdr) {
    return hdr->size_status & getSizeClearMask();
}

/**
 * Retrieves the block header of the next block.
 *
 * Pre-conditions:
 * - hdr: Pointer to the block header.
 *
 * Returns:
 * - Pointer to the block header of the next block.
 */
blockHeader *getNextBlk(blockHeader *hdr) {
    return (blockHeader *) (((char *) hdr) + getBlkSize(hdr));
}

/**
 * Checks if a block is the end marker.
 *
 * Pre-conditions:
 * - hdr: Pointer to the block header.
 *
 * Returns:
 * - 1 if the block is the end marker, 0 otherwise.
 */
int isEnd(blockHeader *hdr) {
    return hdr->size_status == 1;
}

/**
 * Retrieves the footer of the block.
 *
 * Pre-conditions:
 * - hdr: Pointer to the block header.
 *
 * Returns:
 * - Pointer to the footer of the block.
 */
blockHeader *getFoot(blockHeader *hdr) {
    return getNextBlk(hdr) - 1;
}

/*
 * Additional global variables may be added as needed below
 * TODO: add global variables needed by your function
 */

/**
 * Allocates a block of memory of the specified size from the heap.
 *
 * Pre-conditions:
 * - size: Size of the desired block of memory in bytes. Should be >= 1.
 * - heap_start: Pointer to the start of the heap managed by this allocator.
 *     Should be initialized prior to calling this function.
 *
 * Returns:
 * - A pointer to the allocated block of memory.
 * - NULL if the function fails to find a suitable block to allocate.
 */
void *balloc(int size) {
    blockHeader *bestBlk = NULL, *currBlk = heap_start, *nextBlk;
    int leftSize;

    // Handle edge cases where size is zero or negative
    if (size <= 0) {
        return NULL;
    }

    // Calculate the required block size including the header and padding
    size += sizeof(blockHeader);
    if (size % 8 != 0) {
        size += 8 - (size % 8); // Align to 8-byte boundary
    }

    // should not pass the heap_start as a left bound
    if (currBlk == heap_start) {
        currBlk->size_status |= 2;  // Mark p-bit for heap start
    }

    // Search for the best-fit free block
    while (!isEnd(currBlk)) {
        if (!isAlloc(currBlk)) {
            if (getBlkSize(currBlk) == size) { // Found a block with an exact size match
                currBlk->size_status |= 1; // Mark block as allocated
                if (!isEnd(getNextBlk(currBlk))) {
                    getNextBlk(currBlk)->size_status |= 2; // Mark next block's previous-allocated bit
                }
                return currBlk + 1;
            }
            // Update bestBlk if this block is smaller and free
            if ((bestBlk == NULL || getBlkSize(currBlk) < getBlkSize(bestBlk)) && size < getBlkSize(currBlk)) {
                bestBlk = currBlk;
            }
        }
        currBlk = getNextBlk(currBlk);
    }

    // No suitable block found
    if (bestBlk == NULL) {
        return NULL;
    }

    // Calculate the size of the remaining part after the block is allocated
    leftSize = getBlkSize(bestBlk) - size;

    // Create a new header for the remaining free space and split
    nextBlk = bestBlk + (size / sizeof(blockHeader));
    nextBlk->size_status = ((leftSize | 2) & ~1);
    getFoot(nextBlk)->size_status = getBlkSize(nextBlk); // mask only for footer

    // Update the best-fit block and mark it as allocated
    // retains the existing status bits while updating the size.
    bestBlk->size_status = size | (bestBlk->size_status & 3);
    bestBlk->size_status |= 1;

    // Update the p-bit of the next block if it exists
    if (!isEnd(getNextBlk(bestBlk))) {
        getNextBlk(bestBlk)->size_status |= 2;
    }

    // Update the p-bit for heap start
    if (bestBlk == heap_start) {
        bestBlk->size_status |= 2;
    }

    return bestBlk + 1;
}

/**
 * Frees a previously allocated block of memory on the heap.
 *
 * Pre-conditions:
 * - ptr: Pointer to the block of memory to be freed.
 *   Should be non-NULL, a multiple of 8, within the heap space, and not already freed.
 *
 * Post-conditions:
 * - The block of memory pointed to by ptr is freed.
 * - Adjacent free blocks are coalesced into a single larger free block.
 * - Headers and footers are updated as necessary.
 *
 * Returns:
 * - 0 if the operation is successful.
 * - -1 if any of the following conditions are met:
 *     - ptr is NULL.
 *     - ptr is not a multiple of 8.
 *     - ptr is outside the heap space.
 *     - The block at ptr is already freed.
 */
int bfree(void *ptr) {
    // pointers to current and previous blocks
    blockHeader *blk, *prevBlk;

    if (ptr == NULL) {
        return -1;
    }

    // ptr is not a multiple of 8
    if ((unsigned long) ptr % 8 != 0) {
        return -1;
    }

    // ptr is not outside the heap space
    blk = (blockHeader *) ptr - 1;
    if (blk < heap_start || blk >= (blockHeader *) ((char *) heap_start + alloc_size)) {
        return -1;
    }

    // ptr is already freed
    if (isAlloc(blk) == 0) {
        return -1;
    }

    blockHeader *nextBlk = getNextBlk(blk); // next block of current block

    // Set the block as free
    blk->size_status &= ~1; // Unset the allocated bit
    // Update the p-bit of the next block if it's not the end mark
    if (!isEnd(nextBlk)) {
        nextBlk->size_status &= ~2; // Unset the previous-allocated bit
    }
    getFoot(blk)->size_status = getBlkSize(blk); // Set footer

    // Complete coalescing
    // Coalesce with next block if it's free
    if (!isEnd(nextBlk) && !isAlloc(nextBlk)) {
        // cannot erase the blk p-bit
        blk->size_status = blk->size_status + nextBlk->size_status;
        getFoot(blk)->size_status = getBlkSize(blk); // mask only for footer
    }

    // Coalesce with previous block if it's free
    if (!isPrevAlloc(blk)) { // blk 000
        prevBlk = (blockHeader *) ((void *) blk - (blk - 1)->size_status);
        // cannot erase the prevBlk p-bit
        prevBlk->size_status = blk->size_status + prevBlk->size_status;
        getFoot(prevBlk)->size_status = getBlkSize(prevBlk); // mask only for footer
    }

    return 0;
}

/*
 * Initializes the memory allocator.
 * Called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int init_heap(int sizeOfRegion) {
    static int allocated_once = 0; //prevent multiple myInit calls

    int pagesize; // page size
    int padsize;  // size of padding when heap size not a multiple of page size
    void *mmap_ptr; // pointer to memory mapped area
    int fd;

    blockHeader *end_mark;

    if (0 != allocated_once) {
        fprintf(stderr,
                "Error:mem.c: InitHeap has allocated space during a previous call\n");
        return -1;
    }

    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize from O.S.
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    alloc_size = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    mmap_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == mmap_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }

    allocated_once = 1;

    // for double word alignment and end mark
    alloc_size -= 8;

    // Initially there is only one big free block in the heap.
    // Skip first 4 bytes for double word alignment requirement.
    heap_start = (blockHeader *) mmap_ptr + 1;

    // Set the end mark
    end_mark = (blockHeader *) ((void *) heap_start + alloc_size);
    end_mark->size_status = 1;

    // Set size in header
    heap_start->size_status = alloc_size;

    // Set p-bit as allocated in header
    // note a-bit left at 0 for free
    heap_start->size_status += 2;

    // Set the footer
    blockHeader *footer = (blockHeader *) ((void *) heap_start + alloc_size - 4);
    footer->size_status = alloc_size;

    return 0;
}

/* STUDENTS MAY EDIT THIS FUNCTION, but do not change function header.
 * TIP: review this implementation to see one way to traverse through
 *      the blocks in the heap.
 *
 * Can be used for DEBUGGING to help you visualize your heap structure.
 * It traverses heap blocks and prints info about each block found.
 *
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts)
 * t_End    : address of the last byte in the block
 * t_Size   : size of the block as stored in the block header
 */
void disp_heap() {
    int counter;
    char status[6];
    char p_status[6];
    char *t_begin = NULL;
    char *t_end = NULL;
    int t_size;

    blockHeader *current = heap_start;
    counter = 1;

    int used_size = 0;
    int free_size = 0;
    int is_used = -1;

    fprintf(stdout,
            "*********************************** HEAP: Block List ****************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout,
            "---------------------------------------------------------------------------------\n");

    while (current->size_status != 1) {
        t_begin = (char *) current;
        t_size = current->size_status;

        if (t_size & 1) {
            // LSB = 1 => used block
            strcpy(status, "alloc");
            is_used = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "FREE ");
            is_used = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "alloc");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "FREE ");
        }

        if (is_used)
            used_size += t_size;
        else
            free_size += t_size;

        t_end = t_begin + t_size - 1;

        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%4i\n", counter, status,
                p_status, (unsigned long int) t_begin, (unsigned long int) t_end, t_size);

        current = (blockHeader *) ((char *) current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout,
            "---------------------------------------------------------------------------------\n");
    fprintf(stdout,
            "*********************************************************************************\n");
    fprintf(stdout, "Total used size = %4d\n", used_size);
    fprintf(stdout, "Total free size = %4d\n", free_size);
    fprintf(stdout, "Total size      = %4d\n", used_size + free_size);
    fprintf(stdout,
            "*********************************************************************************\n");
    fflush(stdout);

    return;
}


// end FlexAlloc.c (Fall 2023)