#ifndef __MEMORYPOOL_THREADCACHE__H_
#define __MEMORYPOOL_THREADCACHE__H_

#include "common.h"

#include <array>

namespace MemoryPool_V2
{
// 本地线程缓存
class ThreadCache
{
  public:
    // 单例
    static ThreadCache *getInstance()
    {
        static thread_local ThreadCache instance;
        return &instance;
    }

    // 分配 && 释放
    void *allocate(size_t size);
    void deallocate(void *ptr, size_t size);

  private:
    ThreadCache()
    {
    }
    ThreadCache(const ThreadCache &) = delete;
    ThreadCache &operator=(const ThreadCache &) = delete;
    // 从中心缓存获取/归还内存
    void *fetchFromCentralCache(size_t index);
    void returnToCentralCache(void *start, size_t size);
    bool shouldReturnToCentralCache(size_t index);

  private:
    std::array<void *, FREE_LIST_SIZE> m_freeList{nullptr};
    std::array<size_t, FREE_LIST_SIZE> m_freeListSize{0};
};
} // namespace MemoryPool_V2

#endif //__MEMORYPOOL_THREADCACHE__H_