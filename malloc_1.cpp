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
}