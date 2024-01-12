#ifndef __FlexAlloc_h
#define __FlexAlloc_h

int   init_heap(int sizeOfRegion);
void  disp_heap();

void* balloc(int size);
int   bfree(void *ptr);

void* malloc(size_t size) {
    return NULL;
}

#endif // __FlexAlloc_h__

