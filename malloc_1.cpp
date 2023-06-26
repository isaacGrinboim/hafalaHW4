#include <unistd.h>
void* smalloc(size_t size) {
    if(size == 0 || size > 1e8){
        return nullptr;
    }
    void* pointerRes = sbrk(size);
    if (pointerRes == (void*)-1){
        return nullptr;
    }
    else{
        return pointerRes;
    }
}/*
discussion:
We never exploit the possibility that there could be a sequence that was freed.
we might not have enough space at the 'break point', but there is enough space 
if we would use some of the heap's other regions.
*/