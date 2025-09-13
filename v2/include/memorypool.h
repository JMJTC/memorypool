#ifndef __MEMORYPOOL_MEMORYPOOL_H__
#define __MEMORYPOOL_MEMORYPOOL_H__

#include "threadcache.h"
#include <type_traits>
#include <utility>

using namespace MemoryPool_V2;

class MemoryPool
{
  public:
    // Original void* interface (kept for backward compatibility)
    static void *allocate(size_t size)
    {
        return ThreadCache::getInstance()->allocate(size);
    }
    static void deallocate(void *ptr, size_t size)
    {
        ThreadCache::getInstance()->deallocate(ptr, size);
    }
    
    // Type-safe allocation for single objects
    template<typename T>
    static T* allocate()
    {
        return static_cast<T*>(allocate(sizeof(T)));
    }
    
    // Type-safe deallocation for single objects
    template<typename T>
    static void deallocate(T* ptr)
    {
        deallocate(static_cast<void*>(ptr), sizeof(T));
    }
    
    // Type-safe allocation for arrays
    template<typename T>
    static T* allocateArray(size_t count)
    {
        if (count == 0) return nullptr;
        
        // Store the array size before the actual array memory
        size_t* ptr = static_cast<size_t*>(allocate(sizeof(size_t) + count * sizeof(T)));
        *ptr = count;
        
        // Return a pointer to the actual array data
        return reinterpret_cast<T*>(ptr + 1);
    }
    
    // Type-safe deallocation for arrays
    template<typename T>
    static void deallocateArray(T* arr)
    {
        if (arr == nullptr) return;
        
        // Get the stored array size
        size_t* ptr = reinterpret_cast<size_t*>(arr) - 1;
        size_t count = *ptr;
        
        deallocate(static_cast<void*>(ptr), sizeof(size_t) + count * sizeof(T));
    }
    
    // In-place construction with perfect forwarding
    template<typename T, typename... Args>
    static T* newObject(Args&&... args)
    {
        void* memory = allocate(sizeof(T));
        return new (memory) T(std::forward<Args>(args)...);
    }
    
    // Destruction and deallocation
    template<typename T>
    static void deleteObject(T* obj)
    {
        if (obj == nullptr) return;
        
        obj->~T(); // Call destructor
        deallocate(obj); // Deallocate memory
    }
};
#endif //__MEMORYPOOL_MEMORYPOOL_H__