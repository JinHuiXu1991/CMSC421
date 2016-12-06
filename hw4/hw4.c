/* 
   CMSC421 Homework 4
   Author: 	Jin Hui Xu
   E-mail:	ac39537@umbc.edu
   Description: This program will simulate a machine's memory system. The machine has 8 page frames, 
		where each frame holds 32 bytes, for 256 bytes total. The memory allocator in this program 
		will implement a first-fit allocation strategy. User can display information about memory 
		allocations as well as deallocate a memory region after the memory is allocated

     I will use flag and block array to track which frames are allocated and how large each memory block is.
   Each time page frame is allocated, the corresponding flag in the array will be set to 0. If consecutive 
   frames are allocated, their flags will be set to 0. For memory block, because of each frame has 32 bytes 
   memory, the size of memory block can be compute according to the request size and index (frame-aligned pointer), 
   and then store it into block array.

   How would a call to my_free() would know to deallocate 5 frames if the user had previously called my_malloc() for 130 bytes?
     my_free() will check the size of memory block from block array based on the input pointer before deallocate, if the size of
   memory block is 160 (32 bytes aligned) which equal to 5 frames, it will set all corresponding flags to 1 and block memory to 0.

   How does my_free() know it needs to send a SEGFAULT if the user tries to free a pointer that points into the middle of a memory block?
     Because of the returned address of malloc() and calloc() are frame-aligned, my_free() will check if the input pointer matches the 
   frame-aligned address. If it is not, then my_free() will know it needs to send a SEGFAULT.
*/

#define _XOPEN_SOURCE
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

extern void hw4_test(void);

//global variables
static unsigned char memory[256];		//total memory 256 bytes
static int flag[8] = {1, 1, 1, 1, 1, 1, 1, 1};  //represent each frame is free(1) or not(0), initialize to free
static unsigned int block[8];			//store the current memory of blocks
static size_t index = 0;			//indicate the frame-aligned pointer
pthread_mutex_t lock;   			//synchronizer 

//round up to next 32 bytes
size_t align32(size_t num) { 
	size_t remainder = num % 32;
	if (remainder == 0)
		return num;
	return num + 32 - remainder;
}


//Display information about memory allocations.
void my_malloc_stats(void) {
	pthread_mutex_lock(&lock);
	//Display the actual bytes stored in memory
	printf("Memory contents:\n");
	for (int i = 0; i < 256; i+=32) {
		printf("  ");
		//one frame per line, 8 lines total
		for(int j = i+0; j < i+32; j++) {
			//If the byte is unprintable ASCII value less than 32 or greater than 126
			//then display a dot instead.
			if(memory[j] > 126 || memory[j] < 32) 
				printf(".");
			else
				printf("%c", memory[j]);
		}
		printf("\n");
	}

	printf("Memory allocations:\n  ");
	for(int i =0; i < 8; i++) {
		//Indicate reserved frames with R, free memory with f
		if(flag[i] == 1) 
			printf("f");
		else 
			printf("R");
	}
	
	printf("\n");
	pthread_mutex_unlock(&lock);
}


//Allocate and return a contiguous memory block that is within the memory mapped region.
void *my_malloc(size_t size) {
	void * loc = NULL; //return location pointer
	int space = 0, remainMemory = 0, mem = 0;

	//If size is 0, return NULL
	pthread_mutex_lock(&lock);
	if(size == 0) {
		pthread_mutex_unlock(&lock);
		return NULL;
	}

	//align the size to 32 bytes
	size_t s = size;
	s = align32(s);

	for(int i = 0; i<8; i++) {
		space += block[i];
	}
	// @c NULL if no space could be found
	if(sizeof(memory) - space < size) {
		pthread_mutex_unlock(&lock);
		return NULL;
	}

	size_t temp = index;
	//check every page frame to find the available one for request size
	//loop until the index pointer exceed maximum memory
	while(temp < 257) {
		//if the current page frame is free
		if(flag[temp/32] == 1) {
			//if yes, 32 bytes memory available
			mem = 32;
			
			for(int i = temp/32; i < 8; i++) {
				//if the available memory is less than the request size
				//and if the following page frame is free
				if(mem < size && flag[i+1] == 1) {
					//increase available memory by 32 bytes
					mem += 32;
				}
				//if the following page frame is not free, break because they are not consecutive frame
				else if (flag[i+1] == 0) 
					break;
			}
			//if the current available memory is less than request size
			if(mem < size)
				//check next page frame
				temp += 32;
			//if the current available memory is large enough, break 
			else
				break;
		}
		//if the current page frame is not free
		else
			//check next page frame
			temp += 32;
	}
	
	//check if the index pointer exceed our maximum memory
	if(temp > 256)
		//set remain memory to 0 because we don't have any memory left
		remainMemory = 0;
	else {
		//if the index pointer is within our memory region
		//set the new index for allocation
		index = temp;
		//set the remain memory for allocation
		remainMemory = mem;
	}

	//check if the index page frame is free or not,
	//and the remain memory is large enough for allocation
	if(flag[index/32] == 1 && remainMemory >= size) {
		//if yes, set the return pointer point to the index pointer in memory 
		loc = &memory[index];
	}
	//if out of memory, set errno to @c ENOMEM
	else {
		errno = ENOMEM;
		pthread_mutex_unlock(&lock);
		return NULL;
	}

	//update the flags for each page frame and the allocated block memory after allocation
	for(unsigned int i = 0; i < 8; i++) {
		if(sizeof(memory) - index != 0 && size > 32 * i) {
			flag[index/32+i] = 0;
			block[index/32] += 32;
		}
	}
	
	//update the index pointer point to next available page frame
	index += s;
	pthread_mutex_unlock(&lock);

	return loc;

}

//Deallocate a memory region that was returned by my_malloc().
void my_free(void *ptr) {
	pthread_mutex_lock(&lock);
	// check if @c NULL,
	//if @c NULL, do nothing.
	if(ptr != NULL) {
		//check if @a ptr is a pointer returned by my_malloc()
		if (ptr == &memory[0]) {
			//calling my_free() on a previously freed region results in a SIGSEGV
			if(flag[0] == 1) {
				//send SIGSEGV signal
				kill(0, SIGSEGV);
			}
			else {
				//check how many bytes memory in this page frame
				for(unsigned int i = 0; i < 8; i++) {
					if(block[0] == ((i+1) * 32) && flag[i] == 0)
						//set the corresponding page frames to free 
						for(int j = i; j > -1; j--)
							flag[j] = 1;
				}
			}
			//free the memory store in this page frame
			block[0] = 0;
		}
		else if (ptr == &memory[32]) {
			if(flag[1] == 1) {
				kill(0, SIGSEGV);
			}
			else {
				for(unsigned int i = 1; i < 8; i++) {
					if(block[1] == (i * 32) && flag[i] == 0) 
						for( int j = i; j > 0; j--)
							flag[j] = 1;
				}
			}
			block[1] = 0;
		}
		else if (ptr == &memory[64]) {
			if(flag[2] == 1) {
				kill(0, SIGSEGV);
			}
			else {
				for(unsigned int i = 2; i < 8; i++) {
					if(block[2] == ((i-1) * 32) && flag[i] == 0) 
						for(int j = i; j > 1; j--)
							flag[j] = 1;
				}
			}
			block[2] = 0;
		}
		else if (ptr == &memory[96]) {
			if(flag[3] == 1) {
				kill(0, SIGSEGV);
			}
			else {
				for(unsigned int i = 3; i < 8; i++) {
					if(block[3] == ((i-2) * 32) && flag[i] == 0) 
						for(int j = i; j > 2; j--)
							flag[j] = 1;
				}
			}
			block[3] = 0;
		}
		else if (ptr == &memory[128]) {
			if(flag[4] == 1) {
				kill(0, SIGSEGV);
			}
			else {
				for(unsigned int i = 4; i < 8; i++) {
					if(block[4] == ((i-3) * 32) && flag[i] == 0) 
						for(int j = i; j > 3; j--)
							flag[j] = 1;
				}
			}
			block[4] = 0;
		}
		else if (ptr == &memory[160]) {
			if(flag[5] == 1) {
				kill(0, SIGSEGV);
			}
			else {
				for(unsigned int i = 5; i < 8; i++) {
					if(block[5] == ((i-4) * 32) && flag[i] == 0) 
						for(int j = i; j > 4; j--)
							flag[j] = 1;
				}
			}
			block[5] = 0;
		}
		else if (ptr == &memory[192]) {
			if(flag[6] == 1) {
				kill(0, SIGSEGV);
			}
			else {
				for(unsigned int i = 6; i < 8; i++) {
					if(block[6] == ((i-5) * 32) && flag[i] == 0) 
						for(int j = i; j > 5; j--)
							flag[j] = 1;
				}
			}
			block[6] = 0;
		}
		else if (ptr == &memory[224]) {
			if(flag[7] == 1) {
				kill(0, SIGSEGV);
			}
			else {
				for(unsigned int i = 7; i < 8; i++) {
					if(block[7] == ((i-6) * 32) && flag[i] == 0) 
						for(int j = i; j > 6; j--)
							flag[j] = 1;
				}
			}
			block[7] = 0;
		}
		//if @a ptr is not a pointer returned by my_malloc()
		//send a SIGSEGV signal to the calling process.
		else {
			kill(0, SIGSEGV);
			
		}

		//set the frame-aligned pointer point to the new free page frame
		for(int i = 0; i < 8; i++) {
			if(flag[i] == 1) {
				index = i * 32;
				break;
			}
		}
		
	}
	pthread_mutex_unlock(&lock);

}


//Allocate an array of memory and set it to zero.
//my_calloc use the same first-fit allocation strategy as my_malloc()
void *my_calloc(size_t nmemb, size_t size) {
	void *loc = NULL;
	int space = 0, remainMemory = 0, mem = 0;
	size_t s, total;	//store the actually request memory

	pthread_mutex_lock(&lock);
	//if number of elements or number of bytes per element is 0, return NULL
	if(nmemb == 0 || size == 0) {
		pthread_mutex_unlock(&lock);
		return NULL;
	}

	//calculate the request memory, and round it to next 32 bytes
	total = nmemb * size;
	s = align32(total);

	for(int i = 0; i<8; i++) {
		space += block[i];
	}

	if(sizeof(memory) - space < total) {
		pthread_mutex_unlock(&lock);
		return NULL;
	}

	size_t temp = index;
	while(temp < 257) {
		if(flag[temp/32] == 1) {
			mem = 32;
			for(int i = temp/32; i < 8; i++) {
				if(mem < total && flag[i+1] == 1) {
					mem += 32;
				}
				else if (flag[i+1] == 0) 
					break;
			}
			if(mem < total)
				temp += 32;
			else
				break;
		}
		else
			temp += 32;
	}
	
	if(temp > 256)
		remainMemory = 0;
	else {
		index = temp;
		remainMemory = mem;
	}

	if(flag[index/32] == 1 && remainMemory >= total) {
		loc = &memory[index];
	}
	else {
		errno = ENOMEM;
		pthread_mutex_unlock(&lock);
		return NULL;
	}

	for(unsigned int i = 0; i < 8; i++) {
		if(sizeof(memory) - index != 0 && total > 32 * i) {
			flag[index/32+i] = 0;
			block[index/32] += 32;
		}
	}

	//initialize memory content of page frame to 0
	for(unsigned int i = index; i < index + block[index/32]; i++) 
		memory[i] = 0;

	index += s;
	pthread_mutex_unlock(&lock);
	return loc;
}


int main() {
	//initialize memory content to 0
	for (int i = 0; i < 256; i ++) 
		memory[i] = 0;
	
	//initialize the pthread mutex
        pthread_mutex_init(&lock, NULL);
	
	//call test file to test the functions
	hw4_test();

	//delete pthread mutexes
	pthread_mutex_destroy(&lock);

	return 0;
}
