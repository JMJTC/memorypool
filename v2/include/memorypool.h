#ifndef __MEMORYPOOL_MEMORYPOOL_H__
#define __MEMORYPOOL_MEMORYPOOL_H__

#include "thraedcache.h"

using namespace MemoryPool_V2;

class MemoryPool
{
  public:
    static void *allocate(size_t size)
    {
        return ThreadCache::getInstance()->allocate(size);
    }
    static void deallocate(void *ptr, size_t size)
    {
        ThreadCache::getInstance()->deallocate(ptr, size);
    }
};
#endif //__MEMORYPOOL_MEMORYPOOL_H__