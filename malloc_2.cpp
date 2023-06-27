#include <unistd.h>
#include <cstring>

int numTags = 0;
int TagSize = sizeof(MallocMetadata);
int freeBytes = 0;
int freeBlocks = 0;
int allocatedBlocks = 0;
int allocatedBytes = 0;

Tag* blockListHead = NULL;
Tag* blockListTail = NULL;

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
                currTag->is_free = false;
                freeBlocks--;
                freeBytes -= size;
                return (char*)currTag + TagSize;
            }
            currTag = currTag->next;
        }
    void* res = realAlloc(size);
    if(res == NULL){
        return NULL;
    }
    ((Tag*)res)->is_free = false;
    ((Tag*)res)->size = size;
    ((Tag*)res)->next = NULL;
    ((Tag*)res)->prev = blockListTail;
    if(blockListHead == NULL){
        blockListHead = (Tag*)res;
    } 
    blockListTail = (Tag*)res;
    numTags++;
    allocatedBlocks++;  
    allocatedBytes += (size+TagSize);
    return (char*)res+TagSize;
}
void* scalloc(size_t num, size_t size){
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
    if(p != NULL){
        Tag* tag = (Tag*)((char*)p-TagSize);
        tag->is_free = true;
    }
}
void* srealloc(void* oldp, size_t size){
    if(size == 0 || size > 1e8){
        return NULL;
    }
    Tag* oldTag = (Tag*)((char*)oldp - TagSize);
    if(size <= oldTag->size){
        return oldp;
    }
    void* res = smalloc(size);
    if(res == NULL){
        return NULL;
    }
    std::memmove(res, oldp, oldTag->size);
    oldTag->is_free = true;
    freeBytes += oldTag->size;
    freeBlocks += 1; 
}

typedef struct MallocMetadata {
 size_t size;
 bool is_free;
 MallocMetadata* next;
 MallocMetadata* prev;
}Tag;


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
//hello world
