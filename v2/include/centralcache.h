#ifndef __MEMORYPOOL_CENTRALCACHE_H__
#define __MEMORYPOOL_CENTRALCACHE_H__

#include "common.h"
#include <array>
#include <atomic>
#include <chrono>

namespace MemoryPool_V2
{
// 使用无锁的span信息存储
struct SpanTracker
{
    std::atomic<void *> spanAddr{nullptr}; // span的起始地址
    std::atomic<size_t> numPages{0};       // span包含的页数
    std::atomic<size_t> blockCount{0};     // span 包含的block数
    std::atomic<size_t> freeCount{0};      // 记录span中的空闲块数
};

class CentralCache
{
  public:
    static CentralCache *getInstance()
    {
        static CentralCache instance;
        return &instance;
    }

    void *fetchRange(size_t index);
    void returnRange(void *start, size_t size, size_t bytes);

  private:
    CentralCache();
    CentralCache(const CentralCache &) = delete;
    CentralCache &operator=(const CentralCache &) = delete;
    void init();
    // 从页缓存获取内存
    void *fetchFromPageCache(size_t size);
    SpanTracker *getSpanTracker(void *blockAddr);
    void updateSpanFreeCount(SpanTracker *tracker, size_t newFreeBlocks, size_t index);

  private:
    std::array<std::atomic<void *>, FREE_LIST_SIZE> m_centralFreeList;
    std::array<std::atomic_flag, FREE_LIST_SIZE> m_locks;
    std::array<SpanTracker, 1024> m_spanTrackers;
    std::atomic<size_t> m_spanCount{0};

    // 延迟机制
    static const size_t MAX_DELAY_COUNT = 48;                                            // 最大延迟计数
    std::array<std::atomic<size_t>, FREE_LIST_SIZE> m_delayCounts;                       // 每个大小类的延迟计数
    std::array<std::chrono::steady_clock::time_point, FREE_LIST_SIZE> m_lastReturnTimes; // 上次归还时间
    static const std::chrono::milliseconds DELAY_INTERVAL;                               // 延迟间隔

    bool isDelayReturn(size_t index, size_t currentCount, std::chrono::steady_clock::time_point currentTime);
    void performDelayReturn(size_t index);
};
}; // namespace MemoryPool_V2
#endif //__MEMORYPOOL_CENTRALCACHE_H__