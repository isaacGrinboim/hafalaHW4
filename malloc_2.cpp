#include <unistd.h>

int numTags = 0;
int TagSize = sizeof(MallocMetadata);
int freeBytes = 0;
int freeBlocks = 0;
int allocatedBlocks = 0;
int allocatedBytes = 0;
Tag* blockListHead = NULL;

void* realAlloc(size_t size){
    if(size == 0 || size > 1e8){
        return NULL;
    }
    void* pointerRes = sbrk(size + TagSize);
    if (pointerRes == (void*)-1){
        return NULL;
    }
    else{
        return pointerRes;
    }
}
void* smalloc(size_t size){
    if(size == 0 || size > 1e8){
        return NULL;
    }
        Tag* currTag = blockListHead;
        while(currTag != NULL){
            if(currTag->size >= size && currTag->is_free){
                //int realSize = currTag->size;
                currTag->is_free = false;
                //currTag->size = size;//
                freeBlocks--;
                freeBytes -= (size+TagSize);
                return currTag + TagSize;
            }
            currTag = currTag->next;
        }

    void* res = realAlloc(size);
    if(res == NULL){
        return NULL;
    }
    ((Tag*)res)->size = size;
    ((Tag*)res)->next = blockListHead;
    if(blockListHead != nullptr){blockListHead->prev = res;}
    blockListHead = res;
    ((Tag*)res)->is_free = false;

    numTags++;
    allocatedBlocks++;  
    allocatedBytes += (size+TagSize);

    return res+TagSize;
}

typedef struct MallocMetadata {
 size_t size;
 bool is_free;
 MallocMetadata* next;
 MallocMetadata* prev;
}Tag;