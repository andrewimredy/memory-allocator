//Andrew Imredy
//api5
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include "mymalloc.h"

// USE THIS GODDAMN MACRO OKAY
#define PTR_ADD_BYTES(ptr, byte_offs) ((void*)(((char*)(ptr)) + (byte_offs)))

// Don't change or remove these constants.
#define MINIMUM_ALLOCATION  16
#define SIZE_MULTIPLE       8

unsigned int round_up_size(unsigned int data_size) {
	if(data_size == 0)
		return 0;
	else if(data_size < MINIMUM_ALLOCATION)
		return MINIMUM_ALLOCATION;
	else
		return (data_size + (SIZE_MULTIPLE - 1)) & ~(SIZE_MULTIPLE - 1);
}

//OUR HEADER STRUCT. STORES INFO ABOUT BLOCK AND LINKS BLOCKS
//size: 16 bytes
typedef struct blockHeader
{
	unsigned int dataSize;
	bool used;
	struct blockHeader* nextBlock;
	struct blockHeader* prevBlock;
} blockHeader;

//GLOBAL VARIABLES: heapTail and heapHead
struct blockHeader* heapHead;
struct blockHeader* heapTail;
//for now these will point to the headers not data
//subject to change

void* create_new_block(unsigned int size){ //creates block at end of heap
	//printf("creating new block\n");
	//save address of new block header & move break 
	blockHeader* ptr = sbrk(size + sizeof(blockHeader));
	//create header at beginning of block
	ptr->dataSize = size;
	ptr->used = true;
	//link prev block to this one (if prev block exists)
	if(heapTail){
		heapTail->nextBlock = ptr;
		ptr->prevBlock = heapTail;
	}
	//since this is a new block, set heapTail
	heapTail = ptr;
	//if this is the first block, set heapHead
	if(!heapHead){
		heapHead = ptr;
	}
	//returns pointer to data
	ptr = PTR_ADD_BYTES(ptr, sizeof(blockHeader));
	return ptr;
} //end create_new_block

//contracts heap when freeing last block
void contract_heap(struct blockHeader* bptr){
	if(heapHead == bptr){ //if this is first and last
		heapTail = NULL; //heap becomes empty
		heapHead = NULL;
		brk(bptr); //move pointer
		return;
	}else{ //if this isn't the first
		heapTail = bptr->prevBlock; //reassign tail
		heapTail->nextBlock = NULL; //delete link to freed block
		brk(bptr); //move pointer
		return;
	}
} //end contract_heap

//finds largest free block. returns null if none found
struct blockHeader* find_block(unsigned int size){
	blockHeader* bptr = heapHead;
	struct blockHeader* largest = NULL;
	while(bptr){
		if(!bptr->used){ //if block is free
			if(bptr->dataSize >= size){ //if size is bigger
				largest = bptr; //set retval to current block
				size = bptr->dataSize; //new size to compare to 
			}
		}
		bptr = bptr->nextBlock;
	}
	return largest;
} //end find_block

//given pointer to unused block, marks it used 
//and returns data pointer. called from my_malloc
void* use_block(struct blockHeader* bptr){
	bptr->used = true;
	return PTR_ADD_BYTES(bptr, sizeof(blockHeader));
}// end use_block

//given ptr to block header, splits block in twain
//first block: dataSize = size. used
//second block: dataSize = original size - sizeof(header) - size
//returns void* to first blocks data
void* split_block(struct blockHeader* bptr, unsigned int size){
	//create new header
	blockHeader* newHeader = PTR_ADD_BYTES(bptr, (sizeof(blockHeader) + size));
	newHeader->used = false;
	newHeader->dataSize = (bptr->dataSize - size - sizeof(blockHeader));
	//link headers
	newHeader->nextBlock = bptr->nextBlock;
	bptr->nextBlock = newHeader;
	newHeader->prevBlock = bptr;
	//adjust size &status of used block
	bptr->used = true;
	bptr->dataSize = size;
	return PTR_ADD_BYTES(bptr, sizeof(blockHeader));
}//end split_block

//given a header*, coalesces it and the next block
//no control here, called from coalesce_control
struct blockHeader* coalesce_block(struct blockHeader* curr){
	blockHeader* next = curr->nextBlock;
	blockHeader* nextNext = next->nextBlock;
	if(nextNext){ //make sure we have a block to connect to
		curr->nextBlock = nextNext;
		nextNext->prevBlock = curr;
	}else{//otherwise set null
		curr->nextBlock = NULL;
	}
	curr->dataSize = curr->dataSize + sizeof(blockHeader) + next->dataSize;
	return curr;
}//end coalesce_block

//determines whether we can coalesce, and calls coalesce_block if so
//returns pointer to coalesced block
struct blockHeader* coalesce_control(struct blockHeader* curr){
	//get prev and next for ez access
	blockHeader* prev = curr->prevBlock;
	blockHeader* next = curr->nextBlock;
	//CASE 1: curr = tail. prev free
	if(curr == heapTail && prev && (!prev->used)){
		heapTail = prev;
		return coalesce_block(prev);
	}
	//CASE 2: curr = tail - 1, next free
	if(next == heapTail && (!next->used)){
		heapTail = curr;
		return coalesce_block(curr);
	}
	//CASE 3: prev free
	if(prev && (!prev->used)){
		coalesce_block(prev);
		return coalesce_control(prev);
	}//recursive control if we have 3 free together
	//i dont feel like getting new prevs and nexts
	//CASE 4: next free
	if(next && (!next->used)){
		return coalesce_block(curr);
	}
	return NULL;
}//end coalesce_control

void print_heap(){ //prints the heap lmao
	struct blockHeader* curr = heapHead;
	while(curr){
		printf("Block of size %d. Status: %d\n", curr->dataSize, curr->used);
		curr = curr->nextBlock;
	}
	return;
} //end print_heap

void* my_malloc(unsigned int size) {
	if(size == 0)
		return NULL;

	size = round_up_size(size);

	// ------- Don't remove anything above this line. -------
	// Here's where you'll put your code!
	//first, try to find an unused block
	blockHeader* bptr = find_block(size);
	//CASE 1: no block found. make a new block 
	if(!bptr){
		return create_new_block(size);
	}
	//CASE 2: found big (splittable) block
	if(bptr->dataSize >= size + 32){
		return split_block(bptr, size);
	}
	//CASE 3: found tight fitting block (no split)
	return use_block(bptr);
}

void my_free(void* ptr) {
	if(ptr == NULL)
		return;
	// and here's where you free stuff.
	//printf("freeing\n");
	//Step 1: given ptr to data, go back to header
	struct blockHeader* bptr = PTR_ADD_BYTES(ptr, -sizeof(blockHeader));
	//Step 2: mark the block free
	bptr->used = false;
	//check and coalesce. reassign bptr b/c in case it coalesces w/prev
	bptr = coalesce_control(bptr);
	//step 4: contract heap if this is last block
	if(heapTail == bptr){
		//printf("contracting\n");
		contract_heap(bptr);
	}
	return;
}
