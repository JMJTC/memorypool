#ifndef __MEMORYPOOL_PAGECACHE_H__
#define __MEMORYPOOL_PAGECACHE_H__

#include "common.h"
#include <map>
#include <mutex>

namespace MemoryPool_V2
{
class PageCache
{
  public:
    static const size_t PAGE_SIZE = 4096;
    static PageCache *getInstance()
    {
        static PageCache instance;
        return &instance;
    }
    // 分配 && 释放span
    void *allocateSpan(size_t numPages);
    void deallocateSpan(void *ptr, size_t numPages);

  private:
    PageCache() = default;
    PageCache(const PageCache &) = delete;
    PageCache &operator=(const PageCache &) = delete;
    void *systemAlloc(size_t numPages);

  private:
    struct Span
    {
        void *pageAddr;  // 页起始地址
        size_t numPages; // 页数
        Span *next;      // 链表指针
    };

    std::map<size_t, Span *> m_freeSpans;
    std::map<void *, Span *> m_spanMap;
    std::mutex m_mutex;
};
} // namespace MemoryPool_V2

#endif //__MEMORYPOOL_PAGECACHE_H__