#ifndef __mem_h__
#define __mem_h__

int initialize(int sizeOfRegion);
void *pseudo_malloc(int size);
int pseudo_free(void *ptr);
void memory_dump();

#endif // __mem_h__


