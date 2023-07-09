#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <iostream>

typedef struct MallocMetadata {
    size_t cookie;
    size_t size;
    bool is_free;
    MallocMetadata *next;
    MallocMetadata *prev;

    explicit MallocMetadata() = default;
} Tag;
const size_t MEMSIZE = 0x0400000;
const size_t BLOCKSIZE = 0x20000;
size_t numTags = 0;
size_t TagSize = sizeof(MallocMetadata);
size_t freeBytes = 0;
size_t freeBlocks = 0;
size_t allocatedBlocks = 0;
size_t allocatedBytes = 0;

size_t Cookie = rand();
bool wasMemoryInitialized = false;

/********FUNCTION DEFINITIONS************/
void trimmBlock(Tag *block, size_t size);

Tag *myBuddy(Tag *tag, size_t size);

Tag *smallestFits(size_t size);

bool checkMyBuddies(Tag *tag, size_t destSize, int *i);

void mmapFree(void *address);

Tag *uniteBuddies(Tag *tag);

Tag *uniteBuddies2(Tag *tag, int *i);

void *mmapAlloc(size_t size);

void initializeMem();

void *allign();

void initialMetaData(Tag *tag);

/***************************************/

Tag *blockListHead = NULL;
Tag *blockListTail = NULL;


void *smalloc(size_t size) {
    initializeMem();
    if (size == 0 || size > 1e8) {
        return NULL;
    }
    if (size >= BLOCKSIZE) {
        return mmapAlloc(size);
    }
    //(currTag->size + TagSize)/2 - TagSize >= size
    // <=>
    //(currTag->size + TagSize) >= (size + TagSize)*2


    Tag *bestFit = smallestFits(size);

    if(bestFit == NULL){return NULL;}


    trimmBlock(bestFit, size);
    if(bestFit->cookie!=Cookie){
        exit(0xdeadbeef);
    }
    bestFit->is_free = false;
    freeBlocks--;
    freeBytes -= bestFit->size;
    return (char*)bestFit + TagSize;
}

void *scalloc(size_t num, size_t size) {

    if (size == 0 || num == 0 || size * num > 1e8) {
        return NULL;
    }


    void *res = smalloc(size * num);

    if (res == NULL) {
        return NULL;
    }
    memset(res, 0, size * num);
    return res;
}

void sfree(void *p) {
    if (p == NULL) { return; }
    Tag *pTag = (Tag *) ((char *) p - TagSize);
    if(pTag->cookie!=Cookie){
        exit(0xdeadbeef);
    }
    if(pTag == NULL){ return; }
    if (pTag->is_free)
        return;
    if (pTag->size >= BLOCKSIZE) {
        mmapFree(pTag);
        return;                     // munmap
    }
    pTag->is_free = true;
    freeBytes += pTag->size;
    freeBlocks++;
    uniteBuddies(pTag);

}

void *srealloc(void *oldp, size_t size) {
    if (size == 0 || size > 1e8) {
        return NULL;
    }
    if (oldp == NULL) { return smalloc(size); }

    Tag *oldTag = (Tag *) ((char *) oldp - TagSize);
    if(oldTag->cookie!=Cookie){
        exit(0xdeadbeef);
    }
    if (size <= oldTag->size) {
        return oldp;
    }
    int i = 0;
    if (checkMyBuddies(oldTag, size,  &i)) {
        Tag *oldData = oldTag;
        size_t oldSize = oldTag->size;
        Tag *startOfBlock = uniteBuddies2(oldTag, &i);
        //Tag *startOfBlock = uniteBuddies2(oldTag, size);
        std::memmove((char *)startOfBlock + TagSize, (char *) oldData + TagSize, oldSize);
        return (char *)startOfBlock + TagSize;
    }

    void *res = smalloc(size);
    if (res == NULL) {
        return NULL;
    }

    std::memmove(res, oldp, oldTag->size);
    oldTag->is_free = true;
    freeBytes += oldTag->size;
    freeBlocks += 1;
    return res;
}


size_t _num_free_blocks() {
    return freeBlocks;
}

size_t _num_free_bytes() {
    return freeBytes;
}

size_t _num_allocated_blocks() {
    return allocatedBlocks;
}

size_t _num_allocated_bytes() {
    return allocatedBytes;
}

size_t _num_meta_data_bytes() {
    return TagSize * numTags;
}

size_t _size_meta_data() {
    return TagSize;
}


void initialMetaData(Tag *tag) {
    tag->cookie = Cookie;
    tag->size = BLOCKSIZE - TagSize;
    tag->is_free = true;
    tag->prev = NULL;
    tag->next = NULL;
}


void *allign() {
    void *start = sbrk(0);
    size_t inv = MEMSIZE - (unsigned long) start % MEMSIZE;
    return sbrk(inv);
}

void initializeMem() {
    if (wasMemoryInitialized)return;
    void *start = allign();
    start = sbrk(MEMSIZE);

    for (size_t i = 0; i < 32; ++i) {
        Tag *toInit = (Tag *) ((char *) start + i * BLOCKSIZE);
        initialMetaData(toInit);
        if (i == 0) {
            blockListHead = toInit;
            toInit->next = (Tag *) ((char *) start + (i + 1) * BLOCKSIZE);
            continue;
        }
        if (i == 31) {
            toInit->prev = (Tag *) ((char *) start + (i - 1) * BLOCKSIZE);
            toInit->next = NULL;
            continue;
        }
        toInit->next = (Tag *) ((char *) start + (i + 1) * BLOCKSIZE);
        toInit->prev = (Tag *) ((char *) start + (i - 1) * BLOCKSIZE);
    }
    wasMemoryInitialized = true;
    numTags = 32;
    freeBytes = 32 * (BLOCKSIZE - TagSize);
    freeBlocks = 32;
    allocatedBlocks = 32;
    allocatedBytes = freeBytes;
}


void trimmBlock(Tag *block, size_t size) {

    while (block->size  >= (size+TagSize)*2-TagSize) {
        size_t newSize = ((block->size + TagSize) / 2) - TagSize;// assuming a whole number

       Tag *rightTag = (Tag *) ((char *)block + newSize + TagSize);
       //Tag *rightTag = (Tag *) ((char*)block + (block->size + TagSize)/2);
        if (block == blockListTail) {
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
        freeBytes -= TagSize;
        allocatedBytes -= TagSize;
        numTags++;
        allocatedBlocks++;
    }
}


Tag *smallestFits(size_t size) {

    Tag *current = blockListHead;
    Tag *bestFit = NULL;

    while (current != NULL) {

        if (current->size >= size && current->is_free) {
            if (bestFit == NULL || (current->size < bestFit->size)) {
                bestFit = current;
            }
        }

        current = current->next;
    }

    return bestFit;
}

void *mmapAlloc(size_t size) {
    void *block = mmap(NULL, size + TagSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (block == MAP_FAILED) {
        return NULL;
    }

    Tag * tag=(Tag *) block;
    tag->prev = NULL;
    tag->next = NULL;
    tag->cookie = Cookie;
    tag->is_free = false;
    tag->size = size;

    allocatedBlocks++;
    allocatedBytes += size;
    numTags++;

    return (char *) block + TagSize;
}

Tag *uniteBuddies(Tag *tag) {
    Tag* current = tag;
    Tag *buddy = myBuddy(current, current->size + TagSize);
    while ( (current->size + TagSize < BLOCKSIZE) && buddy->is_free && buddy->size==current->size) {
        Tag *left = buddy < current ? buddy : current;
        Tag *right = buddy == left ? current : buddy;
        left->size += TagSize + right->size;
        left->next = right->next;
        if (right->next != NULL)
            (right->next)->prev = left;
        current = left;
        buddy = myBuddy(current, current->size + TagSize);

        numTags--;
        freeBytes += TagSize;
        freeBlocks--;
        allocatedBlocks--;
        allocatedBytes += TagSize;
    }
    return current;
}

Tag *uniteBuddies2(Tag *tag, int* i) {
    int j = 0;
    Tag *current = tag;
    Tag *buddy = myBuddy(current, current->size + TagSize);
    while ((current->size + TagSize < BLOCKSIZE) && buddy->is_free && buddy->size == current->size && j < *i) {
        Tag *left = buddy < current ? buddy : current;
        Tag *right = buddy == left ? current : buddy;
        freeBytes -= (buddy->size);

        left->size += TagSize + right->size;
        left->next = right->next;
        if (right->next != NULL)
            (right->next)->prev = left;
        current = left;
        buddy = myBuddy(current, current->size + TagSize);

        numTags--;
        freeBlocks--;
        allocatedBlocks--;
        allocatedBytes += (TagSize);
        




        ++j;
    }
    return current;
}


void mmapFree(void *address) {
    allocatedBlocks--;
    numTags--;
    allocatedBytes -= ((Tag *) address)->size;


    munmap(address, ((Tag *) address)->size + TagSize);
}

bool checkMyBuddies(Tag *tag, size_t destSize, int *i) {
    Tag *curr = tag;
    Tag *buddy = myBuddy(tag, curr->size + TagSize);
    size_t size = tag->size;
    while (size < (BLOCKSIZE - TagSize) && buddy->is_free) {
        Tag *left = buddy < tag ? buddy : tag;
        //Tag* right = buddy==left ? tag : buddy ;
        size = (size + buddy->size + TagSize);
        ++(*i);
        if (size  >= destSize) { return true; }
        curr = left;
        buddy = myBuddy(curr, size + TagSize);

    }
    return false;
}

Tag *myBuddy(Tag *tag, size_t size) {
    return (Tag *) ((size_t) (tag) ^ size);
}
