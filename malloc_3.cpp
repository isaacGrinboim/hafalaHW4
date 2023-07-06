#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
typedef struct MallocMetadata {
 int cookie;
 size_t size;
 bool is_free;
 MallocMetadata* next;
 MallocMetadata* prev;
}Tag;
const int MEMSIZE = 0x0400000;
const int BLOCKSIZE = 0x20000;
int numTags = 0;
int TagSize = sizeof(MallocMetadata);
int freeBytes = 0;
int freeBlocks = 0;
int allocatedBlocks = 0;
int allocatedBytes = 0;

int Cookie = rand();
bool wasMemoryInitialized = false;

/********FUNCTION DEFINITIONS************/
void trimmBlock(Tag* block, int size);
Tag* myBuddy(Tag* tag, int size);
Tag* smallestFits(int size);
bool checkMyBuddies(Tag* tag, int destSize);
void mmapFree(void* address);
Tag* uniteBuddies(Tag* tag);
void* mmapAlloc(int size);
void initializeMem();
void* allign();
void initialMetaData(Tag* tag);
/***************************************/

Tag* blockListHead = NULL;
Tag* blockListTail = NULL;


void* smalloc(size_t size){
    initializeMem();
    if(size == 0 || size > 1e8){
        return NULL;
    }
    if(size >= BLOCKSIZE){
        return mmapAlloc(size);
    }
    //(currTag->size + TagSize)/2 - TagSize >= size
    // <=>
    //(currTag->size + TagSize) >= (size + TagSize)*2
        Tag* bestFit = smallestFits(size);
        if(bestFit->size >= size && bestFit->is_free){
            if(bestFit->size > 2*(size +TagSize)- TagSize){ // newwwwww
                trimmBlock(bestFit, size); 
            }
            bestFit->is_free = false;
            freeBlocks--;
            freeBytes -= size;
            return (char*)bestFit + TagSize;
        }
    return NULL;
}
void* scalloc(size_t num, size_t size){
    initializeMem();
    if(size == 0 || num == 0 || size*num > 1e8){
        return NULL;
    }
    void* res = smalloc(size*num);
    if(res == NULL){
        return NULL;
    }
    memset(res, 0, size*num);
    return res;
}
void sfree(void* p){
    initializeMem();
    if(p==NULL){return;}
    Tag* pTag = (Tag*)((char*)p-TagSize);
    if(pTag->size >= BLOCKSIZE){
        mmapFree(pTag);
        return;                     // munmap
    }
    pTag->is_free = true;
    freeBytes += pTag->size;
    freeBlocks++;
    uniteBuddies(pTag);

}
void* srealloc(void* oldp, size_t size){
    initializeMem();
    if(size == 0 || size > 1e8){
        return NULL;
    }
    if(oldp == NULL){return smalloc(size);}

    Tag* oldTag = (Tag*)((char*)oldp - TagSize);
    if(size <= oldTag->size){
        return oldp;
    }
    if(checkMyBuddies(oldTag, size)){
        Tag* oldData = oldTag;
        int oldSize = oldTag->size;
        Tag* startOfBlock = uniteBuddies(oldTag);
        std::memmove(startOfBlock, (char*)oldData + TagSize, oldSize);
        return startOfBlock;
    }
    
    void* res = smalloc(size);
    if(res == NULL){
        return NULL;
    }

    std::memmove(res, oldp, oldTag->size);
    oldTag->is_free = true;
    freeBytes += oldTag->size;
    freeBlocks += 1;
    return res;
}



size_t _num_free_blocks(){
    return freeBlocks;
}
size_t _num_free_bytes(){
    return freeBytes;
}
size_t _num_allocated_blocks(){
    return allocatedBlocks;
}
size_t _num_allocated_bytes(){
    return allocatedBytes;
}
size_t _num_meta_data_bytes(){
    return TagSize*numTags;
}
size_t _size_meta_data(){
    return TagSize;
}






void initialMetaData(Tag* tag){
 tag->cookie = Cookie;
 tag->size = BLOCKSIZE - TagSize;
 tag ->is_free = true;
 tag->prev = NULL;
 tag->next = NULL;
 }






void* allign(){
    void* start = sbrk(0);
    int inv = MEMSIZE - (unsigned long)start%MEMSIZE;
    return sbrk(inv);
}
void initializeMem(){
    if(wasMemoryInitialized)return;
    void* start = allign();
    start = sbrk(MEMSIZE);

    for(int i=0; i<32; ++i){
        Tag* toInit = (Tag*)((char*)start+i*BLOCKSIZE);
        initialMetaData(toInit);
        if(i==0){
            blockListHead = toInit;
            toInit->next = (Tag*)((char*)start+(i+1)*BLOCKSIZE);
            continue;
            }
        if(i==31){
            toInit->prev = (Tag*)((char*)start+(i-1)*BLOCKSIZE);
            toInit->next = NULL;
            continue;
            }
        toInit->next = (Tag*)((char*)start+(i+1)*BLOCKSIZE);
        toInit->prev = (Tag*)((char*)start+(i-1)*BLOCKSIZE);
    }
    wasMemoryInitialized = true;
    numTags = 32;
    freeBytes = 32*(BLOCKSIZE-TagSize);
    freeBlocks = 32;
    allocatedBlocks = 32;
    allocatedBytes = freeBytes;
}


void trimmBlock(Tag* block, int size){
    while((unsigned int)block->size > 2*((unsigned int)size +(unsigned int)TagSize)- (unsigned int)TagSize){
        int newSize = block->size/2;// assuming a whole number 
        Tag* rightTag = (Tag*)((char*)block + newSize);
        if(block == blockListTail){
            blockListTail = rightTag;
        }
        rightTag->next = block->next;
        block->next = rightTag;
        block->size = newSize;
        rightTag->is_free = true;
        rightTag->prev = block;
        rightTag->size = newSize;
        rightTag->cookie = Cookie;
        
        freeBlocks++;
        freeBytes-=TagSize;
        numTags++;
        allocatedBlocks++;
        allocatedBytes-=TagSize;
    }
}


Tag* smallestFits(int size){
    Tag* current = blockListHead;
    Tag* bestFit = blockListHead;
    while(current != NULL){
        if((unsigned int)current->size >= (unsigned int)size && ((unsigned int)current->size < (unsigned int)bestFit->size)){
            bestFit = current;
        }
        current = current->next;
    }
    return bestFit;
}

void* mmapAlloc(int size){
    void* block = mmap(NULL,size+TagSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (block == MAP_FAILED) {
        return NULL;
    }
    ((Tag*)block)->prev = NULL;
    ((Tag*)block)->next = NULL;
    ((Tag*)block)->cookie = Cookie;
    ((Tag*)block)->is_free = false;
    ((Tag*)block)->size = size;
    return (Tag*)block + TagSize;
}

Tag* uniteBuddies(Tag* tag){
    Tag* buddy = myBuddy(tag, tag->size);
    while((unsigned int)tag->size < (unsigned int)(BLOCKSIZE-TagSize) && buddy->is_free){
        Tag* left = buddy<tag ? buddy:tag ;
        Tag* right = buddy==left ? tag : buddy ;
        left->size += TagSize+right->size;
        left->next = right->next;
        if(right->next != NULL)
            (right->next)->prev = left;
        tag = left;
        buddy = myBuddy(tag, tag->size);

        numTags--;
        freeBytes += TagSize;
        freeBlocks--;
        allocatedBlocks--;
        allocatedBytes += TagSize;
    }
    return tag;
}

void mmapFree(void* address){
    munmap(address, ((Tag*)address)->size+TagSize);
}

bool checkMyBuddies(Tag* tag, int destSize){
    Tag* curr = tag;
    Tag* buddy = myBuddy(tag, curr->size);
    int size = tag->size;
    while(size < (BLOCKSIZE-TagSize) && buddy->is_free){
        Tag* left = buddy<tag ? buddy:tag ;
        //Tag* right = buddy==left ? tag : buddy ;
        size = (size+buddy->size + TagSize);
        if(size >= destSize){return true;}
        curr = left;
        buddy = myBuddy(curr, size);
    }
    return false;
}

Tag* myBuddy(Tag* tag, int size){
    return (Tag*)((unsigned long)tag ^ (unsigned long)size);
}