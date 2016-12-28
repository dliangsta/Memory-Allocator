////////////////////////////////////////////////////////////////////////////////
// Main File: mem.c
// This File: mem.c
// Semester: CS 354 Fall 2016
//
// Author: David Liang
// Email: dliang23@wisc.edu
// CS Login: dliang
//////////////////////////// 80 columns wide ///////////////////////////////////

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "memoryAllocator.h"

/* this structure serves as the header for each block */
typedef struct block_hd{
       /* The blocks are maintained as a linked list */
       /* The blocks are ordered in the increasing order of addresses */
       struct block_hd* next;
       /* The size_status field is the size of the payload+padding and is always a multiple of 4 */
       int size_status;
} block_header;

/* Global variable - size of block_header*/
int head_size = sizeof(block_header);
/* Global variable - size of one word*/
int word_size = 4;
/* Global variable - minimum size of one block including header and payload*/
int minimum_block_size = 12;
/* Global variable - This will always point to the first block */
/* ie, the block with the lowest address */
block_header* list_head = NULL;

/*
 * Initializes memory allocator.
 *
 * @param sizeOfRegion Size of the total allowable memory.
 * @return 0 on success, -1 on failure.
 */
int initialize(int sizeOfRegion)
{
       int pagesize;
       int padsize;
       int fd;
       int alloc_size;
       void* space_ptr;
       static int allocated_once = 0;

       if(0 != allocated_once)
       {
              fprintf(stderr,"Error:mem.c: initialize has allocated space during a previous call\n");
              return -1;
       }
       if(sizeOfRegion <= 0)
       {
              fprintf(stderr,"Error:mem.c: Requested block size is not positive\n");
              return -1;
       }

       /* Get the pagesize */
       pagesize = getpagesize();

       /* Calculate padsize as the padding required to round up sizeOfRegio to a multiple of pagesize */
       padsize = sizeOfRegion % pagesize;
       padsize = (pagesize - padsize) % pagesize;

       alloc_size = sizeOfRegion + padsize;

       /* Using mmap to allocate memory */
       fd = open("/dev/zero", O_RDWR);
       if(-1 == fd)
       {
              fprintf(stderr,"Error:mem.c: Cannot open /dev/zero\n");
              return -1;
       }
       space_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
       if (MAP_FAILED == space_ptr)
       {
              fprintf(stderr,"Error:mem.c: mmap cannot allocate space\n");
              allocated_once = 0;
              return -1;
       }

       allocated_once = 1;

       /* To begin with, there is only one big, free block */
       list_head = (block_header*)space_ptr;
       list_head->next = NULL;
       /* Remember that the 'size' stored in block size excludes the space for the header */
       list_head->size_status = alloc_size - (int)head_size;

       return 0;
}


/*
 * Function for allocating a block of a specified size.
 * 
 * @param size The size of memory required.
 * @return Address of allocated block on success, NULL on failure.
 */
void* pseudo_malloc(int size)
{
       // true if size is negative
       if (0 >= size)
       {
              return NULL;
       }
       // true if rounding is needed
       else if (0 != (size % word_size))
       {
              // round size up
              size += word_size - (size % word_size);
       }
       // used to iterate through the linked list
       block_header* curr_head = list_head;
       // points to the header of the block that we're allocating
       block_header* alloc_header = NULL;
       // size of the currently best fitting block
       int current_best_size = -1;
       // Search for the best fit block in the free list
       while (NULL != curr_head)
       {
              // save size of the current head
              int curr_size = curr_head->size_status;
              // true if current block is free
              if (!(curr_size & 1))
              {
                     // true if perfect size
                     if (curr_size == size)
                     {
                            // set alloc_header to point to what curr_head points to
                            alloc_header = curr_head;
                            // end while loop
                            curr_head = NULL;
                     }
                     // else if adequate size
                     else if (curr_size > size)
                     {
                            // true if this block is a better fit
                            if ((curr_size < current_best_size) || (current_best_size == -1))
                            {
                                   // reference this new best block and save its size
                                   current_best_size = curr_head->size_status;
                                   alloc_header = curr_head;
                            }
                     }
              }
              // move curr_head along the linked list
              curr_head = curr_head->next;
       }

       // true if no fit found
       if (alloc_header == NULL)
       {
              return NULL;
       }
       // size of the block before splitting
       int init_alloc_size = alloc_header->size_status;
       // true if block can be split
       if (init_alloc_size - (head_size + size) >= minimum_block_size)
       {
              // create a new block_header for the split block
              block_header* new_header = (block_header*)
                     ((void*)alloc_header + (size + head_size));
              // combine blocks
              new_header->next = alloc_header->next;
              new_header->size_status = init_alloc_size - (head_size + size);
              alloc_header->next = new_header;
              alloc_header->size_status = size;
       }
       // set header as allocated
       alloc_header->size_status += 1;
       // return pointer to payload of allocated block
       return (void*)((void*)alloc_header + head_size);
}

/*
 * Function for freeing up a previously allocated (by this program) block.
 *
 * @param ptr Address of block to be freed.
 * @return 0 on success, -1 on failure.
 */
int pseudo_free(void *ptr)
{
       if (ptr == NULL)
       {
              return -1;
       }
       // create pointer to block's header
       block_header* ptr_header = (block_header*)(ptr - head_size);
       // create a pointer for iterating through the linked list of blocks
       block_header* curr_header = list_head;
       // block that is behind curr_header
       block_header* prev_header = NULL;
       // used to indicate if ptr points to a payload
       int valid = 0;
       // iterate through blocks to determine if ptr points to a payload
       while(curr_header)
       {
              // true if the ptr points to curr_header's payload
              if (curr_header == ptr_header)
              {
                     valid = 1;
                     break;
              }
              //move prev_header along
              prev_header = curr_header;
              // move curr_header along
              curr_header = curr_header->next;
       }
       // true if ptr does not point to a payload
       if (!valid)
       {
              return -1;
       }
       // true if block is free already (cannot be freed)
       else if (!(ptr_header->size_status & 1))
       {
              return -1;
       }
       // set block as free
       ptr_header->size_status -= 1;
       // true if there's a next header
       if (ptr_header->next)
       {
              block_header* next_head = ptr_header->next;
              // true if next block is free
              if (!(next_head->size_status & 1))
              {
                     // coalesce free blocks
                     ptr_header->next = next_head->next;
                     ptr_header->size_status += head_size + next_head->size_status;
              }
       }
       // true if there's a prev header
       if (prev_header)
       {
              // true if the block is free
              if (!(prev_header->size_status & 1))
              {
                     // coalesce blocks
                     prev_header->next = ptr_header->next;
                     prev_header->size_status += head_size + ptr_header->size_status;
              }
       }

       return 0;
}

/* 
 * Function to be used for debugging
 *
 * Prints out a list of all the blocks along with the following information for each block 
 * No.      : Serial number of the block 
 * Status   : free/busy 
 * Begin    : Address of the first useful byte in the block 
 * End      : Address of the last byte in the block 
 * Size     : Size of the block (excluding the header) 
 * t_Size   : Size of the block (including the header) 
 * t_Begin  : Address of the first byte in the block (this is where the header starts) 
 */
void memory_dump()
{
       block_header* current = NULL;
       char* t_Begin = NULL;
       char* Begin = NULL;
       char* End = NULL;
       int counter;
       int Size;
       int t_Size;
       int free_size;
       int busy_size;
       int total_size;
       char status[5];

       free_size = 0;
       busy_size = 0;
       total_size = 0;
       current = list_head;
       counter = 1;

       fprintf(stdout,"************************************Block list***********************************\n");
       fprintf(stdout,"No.\tStatus\tBegin\t\tEnd\t\tSize\tt_Size\tt_Begin\n");
       fprintf(stdout,"---------------------------------------------------------------------------------\n");
       while(NULL != current)
       {
              t_Begin = (char*)current;
              Begin = t_Begin + (int)head_size;
              Size = current->size_status;

              strcpy(status,"Free");

              if(Size & 1) /*LSB = 1 => busy block*/
              {
                     strcpy(status,"Busy");

                     Size = Size - 1; /*Minus one for ignoring status in busy block*/
                     t_Size = Size + (int)head_size;
                     busy_size = busy_size + t_Size;
              }
              else
              {
                     t_Size = Size + (int)head_size;
                     free_size = free_size + t_Size;
              }

              End = Begin + Size - 1;

              fprintf(stdout,"%d\t%s\t0x%08lx\t0x%08lx\t%d\t%d\t0x%08lx\n",counter,status,(unsigned long int)Begin,(unsigned long int)End,Size,t_Size,(unsigned long int)t_Begin);
              
              total_size = total_size + t_Size;
              current = current->next;
              counter = counter + 1;
       }
       fprintf(stdout,"---------------------------------------------------------------------------------\n");
       fprintf(stdout,"*********************************************************************************\n");

       fprintf(stdout,"Total busy size = %d\n",busy_size);
       fprintf(stdout,"Total free size = %d\n",free_size);
       fprintf(stdout,"Total size = %d\n",busy_size+free_size);
       fprintf(stdout,"*********************************************************************************\n");
       fflush(stdout);
       return;
}
