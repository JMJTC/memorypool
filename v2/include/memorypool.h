#ifndef __MEMORYPOOL_MEMORYPOOL_H__
#define __MEMORYPOOL_MEMORYPOOL_H__

#include "threadcache.h"
#include "common.h"
#include <type_traits>
#include <utility>
#include <vector>
#include <new>

using namespace MemoryPool_V2;

class MemoryPool
{
  public:
    // Original void* interface (kept for backward compatibility)
    static void *allocate(size_t size)
    {
        void* ptr = ThreadCache::getInstance()->allocate(size);
        if (!ptr && size > 0) {
            throw std::bad_alloc();
        }
        return ptr;
    }
    
    // Nothrow version of allocate (similar to std::nothrow)
    static void *allocate(size_t size, const std::nothrow_t&)
    {
        try {
            return allocate(size);
        } catch (...) {
            return nullptr;
        }
    }
    
    static void deallocate(void *ptr, size_t size)
    {
        if (ptr) {
            ThreadCache::getInstance()->deallocate(ptr, size);
        }
    }
    
    // 内存池预热接口
    // 预热特定大小的内存块
    static void warmup(size_t size, size_t count = 10)
    {
        std::vector<void*> pointers;
        pointers.reserve(count);
        
        // 预先分配指定大小和数量的内存块
        for (size_t i = 0; i < count; ++i) {
            void* ptr = allocate(size);
            pointers.push_back(ptr);
        }
        
        // 立即释放这些内存块，让它们进入内存池的空闲列表
        for (void* ptr : pointers) {
            deallocate(ptr, size);
        }
    }
    
    // 预热所有大小类别的内存块
    static void warmupAll(size_t countPerSize = 5)
    {
        // 预热所有从8字节到256KB的大小类
        for (size_t i = 0; i < FREE_LIST_SIZE; ++i) {
            size_t size = (i + 1) * ALIGNMENT;
            warmup(size, countPerSize);
        }
    }
    
    // 预热常用大小的内存块
    static void warmupCommon(size_t countPerSize = 10)
    {
        // 预热常用的小内存块（8字节到4KB）
        for (size_t size = 8; size <= 4096; size += 8) {
            warmup(size, countPerSize);
        }
        
        // 预热一些中等大小的内存块
        warmup(8192, countPerSize / 2);
        warmup(16384, countPerSize / 2);
        warmup(32768, countPerSize / 3);
        warmup(65536, countPerSize / 3);
    }
    
    // Type-safe allocation for single objects
    template<typename T>
    static T* allocate()
    {
        T* ptr = static_cast<T*>(allocate(sizeof(T)));
        if (!ptr) {
            throw std::bad_alloc();
        }
        return ptr;
    }
    
    // Type-safe nothrow allocation
    template<typename T>
    static T* allocate(const std::nothrow_t&)
    {
        try {
            return allocate<T>();
        } catch (...) {
            return nullptr;
        }
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
        if (!ptr) {
            throw std::bad_alloc();
        }
        *ptr = count;
        
        // Return a pointer to the actual array data
        return reinterpret_cast<T*>(ptr + 1);
    }
    
    // Type-safe nothrow allocation for arrays
    template<typename T>
    static T* allocateArray(size_t count, const std::nothrow_t&)
    {
        try {
            return allocateArray<T>(count);
        } catch (...) {
            return nullptr;
        }
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
        if (!memory) {
            throw std::bad_alloc();
        }
        return new (memory) T(std::forward<Args>(args)...);
    }
    
    // In-place construction with nothrow
    template<typename T, typename... Args>
    static T* newObject(const std::nothrow_t&, Args&&... args)
    {
        void* memory = allocate(sizeof(T), std::nothrow);
        if (!memory) {
            return nullptr;
        }
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