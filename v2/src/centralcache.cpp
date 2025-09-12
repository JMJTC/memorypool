#include "../include/centralcache.h"
#include "../include/pagecache.h"

#include <iostream>
#include <thread>
#include <unordered_map>

namespace MemoryPool_V2
{
const std::chrono::milliseconds DELAY_INTERVAL{1000};

static const size_t SPAN_PAGES = 8; // 每次从PageCache获取span大小（以页为单位）

CentralCache::CentralCache()
{
    init();
}

void CentralCache::init()
{
    for (auto &ptr : m_centralFreeList)
    {
        ptr.store(nullptr, std::memory_order_relaxed);
    }
    for (auto &lock : m_locks)
    {
        lock.clear();
    }
    for (auto &count : m_delayCounts)
    {
        count.store(0, std::memory_order_relaxed);
    }
    for (auto &time : m_lastReturnTimes)
    {
        time = std::chrono::steady_clock::now();
    }
    m_spanCount.store(0, std::memory_order_relaxed);
}

void *CentralCache::fetchRange(size_t index)
{
    // 大内存直接上操作系统申请
    if (index >= FREE_LIST_SIZE)
    {
        return nullptr;
    }

    // 自旋锁
    while (m_locks[index].test_and_set(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }

    void *res = nullptr;
    try
    {
        res = m_centralFreeList[index].load(std::memory_order_relaxed);
        if (res == nullptr)
        {
            // 从PageCache中获取新的内存块
            size_t size = (index + 1) * ALIGNMENT;
            res = fetchFromPageCache(size);

            if (res == nullptr)
            {
                // 获取失败
                m_locks[index].clear(std::memory_order_release);
                return nullptr;
            }

            // 从 PageCache获取成功
            char *start = static_cast<char *>(res);
            size_t numPages = (size <= SPAN_PAGES * PageCache::PAGE_SIZE)
                                  ? SPAN_PAGES
                                  : (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;
            size_t blockNum = (numPages * PageCache::PAGE_SIZE) / size;
            if (blockNum > 1)
            {
                // 构建链表
                for (size_t i = 1; i < blockNum; i++)
                {
                    void *current = start + (i - 1) * size;
                    void *next = start + i * size;
                    *reinterpret_cast<void **>(current) = next;
                }
                *reinterpret_cast<void **>(start + (blockNum - 1) * size) = nullptr;

                // 获取对应内存 将res断开
                void *next = *reinterpret_cast<void **>(res);
                *reinterpret_cast<void **>(res) = nullptr;
                m_centralFreeList[index].store(next, std::memory_order_release);
            }
            // 记录span信息，为将CentralCache 多余内存块归还PageCache做准备
            // 1.CentralCache管理小块内存，这些内存可能不连续
            // 2.PageCache 的 deallocateSpan 要求归还连续的内存
            // 这里也需要原子操作 why? 因为 m_spanCount 是全局共享的
            // ，而前面使用的自旋锁加锁是按index分段的，所以需要使用原子操作保证安全
            size_t trackerIndex = m_spanCount.fetch_add(1, std::memory_order_relaxed);
            if (trackerIndex < m_spanTrackers.size())
            {
                m_spanTrackers[trackerIndex].spanAddr.store(start, std::memory_order_release);
                m_spanTrackers[trackerIndex].numPages.store(numPages, std::memory_order_release);
                m_spanTrackers[trackerIndex].blockCount.store(blockNum, std::memory_order_release);
                m_spanTrackers[trackerIndex].freeCount.store(blockNum - 1, std::memory_order_release);
            }
        }
        else
        {
            void *next = *reinterpret_cast<void **>(res);
            *reinterpret_cast<void **>(res) = nullptr;
            m_centralFreeList[index].store(next, std::memory_order_release);

            // 更新span 对应的freeCount减1
            SpanTracker *tracker = getSpanTracker(res);
            if (tracker != nullptr)
            {
                tracker->freeCount.fetch_sub(1, std::memory_order_release);
            }
            else
            {
                std::cout << "[CentralCache::fetchRange]: getSpanTracker异常" << std::endl;
            }
        }
    }
    catch (...)
    {
        m_locks[index].clear(std::memory_order_release);
        throw;
    }
    m_locks[index].clear(std::memory_order_release);
    return res;
}

void CentralCache::returnRange(void *start, size_t size, size_t index)
{
    if (start == nullptr || index >= FREE_LIST_SIZE)
    {
        return;
    }
    size_t blockSize = (index + 1) * ALIGNMENT;
    size_t blockNum = size / blockSize;

    // 自旋锁
    while (m_locks[index].test_and_set(std::memory_order_acquire))
    {
        std::this_thread::yield(); // 当前线程主动让出CPU时间片
    }

    try
    {
        // 1. 将内存块串成链表
        void *end = start;
        size_t count = 1;
        while (*reinterpret_cast<void **>(end) != nullptr && count < blockNum)
        {
            end = *reinterpret_cast<void **>(end);
            count++;
        }
        // 2. 将链表连接到中心缓存
        void *current = m_centralFreeList[index].load(std::memory_order_relaxed);
        *reinterpret_cast<void **>(end) = current;
        m_centralFreeList[index].store(start, std::memory_order_release);

        // 3. 更新延迟计数
        size_t currentCount = m_delayCounts[index].fetch_add(1, std::memory_order_relaxed) + 1;
        auto currentTime = std::chrono::steady_clock::now();

        // 4. 检查是否需要执行延迟归还
        if (isDelayReturn(index, currentCount, currentTime))
        {
            performDelayReturn(index);
        }
    }
    catch (...)
    {
        m_locks[index].clear(std::memory_order_release);
        throw;
    }
    m_locks[index].clear(std::memory_order_release);
}

bool CentralCache::isDelayReturn(size_t index, size_t currentCount, std::chrono::steady_clock::time_point currentTime)
{
    if (currentCount >= MAX_DELAY_COUNT)
    {
        return true;
    }
    auto lastTime = m_lastReturnTimes[index];
    return (currentTime - lastTime) >= DELAY_INTERVAL;
}

void CentralCache::performDelayReturn(size_t index)
{
    m_delayCounts[index].store(0, std::memory_order_relaxed);
    m_lastReturnTimes[index] = std::chrono::steady_clock::now();

    // 统计每个span的空闲块数
    std::unordered_map<SpanTracker *, size_t> spanFreeCounts;
    void *currentBlock = m_centralFreeList[index].load(std::memory_order_relaxed);
    while (currentBlock != nullptr)
    {
        SpanTracker *tracker = getSpanTracker(currentBlock);
        if (tracker != nullptr)
        {
            spanFreeCounts[tracker]++;
        }
        else
        {
            std::cout << "[CentralCache::performDelayReturn]: getSpanTracker异常" << std::endl;
        }
        currentBlock = *reinterpret_cast<void **>(currentBlock);
    }

    for (const auto &[tracker, newFreeBlocks] : spanFreeCounts)
    {
        updateSpanFreeCount(tracker, newFreeBlocks, index);
    }
}

void CentralCache::updateSpanFreeCount(SpanTracker *tracker, size_t newFreeBlocks, size_t index)
{
    // 这里的tracker是m_spanTrackers中的某个值
    // 直接获取就好了，在前面是已经记录过了的
    // todo : 为验证可靠性，后续测试单步调试验证下
    if (tracker->freeCount.load(std::memory_order_relaxed) == newFreeBlocks)
    {
        std::cout << "[CentralCache::updateSpanFreeCount] : 直接获取就好了，在前面是已经记录过了的 OK" << std::endl;
    }
    else
    {
        std::cout << "[CentralCache::updateSpanFreeCount] : 记录的值和获取的值不一致" << std::endl;
    }
    tracker->freeCount.store(newFreeBlocks, std::memory_order_relaxed);

    // 所有块都空闲，归还span
    if (newFreeBlocks == tracker->blockCount.load(std::memory_order_relaxed))
    {
        void *spanAddr = tracker->spanAddr.load(std::memory_order_relaxed);
        size_t numPages = tracker->numPages.load(std::memory_order_relaxed);

        void *head = m_centralFreeList[index].load(std::memory_order_relaxed);
        void *newHead = nullptr;
        void *tail = nullptr;
        void *current = head;

        char *spanStart = reinterpret_cast<char *>(spanAddr);
        char *spanEnd = spanStart + numPages * PageCache::PAGE_SIZE;

        while (current != nullptr)
        {
            void *next = *reinterpret_cast<void **>(current);
            char *cur = reinterpret_cast<char *>(current);

            if (cur >= spanStart && cur < spanEnd)
            {
                // skip
            }
            else
            {
                // 对于不属于该span的 进行连接
                if (!newHead)
                {
                    newHead = current;
                    tail = current;
                }
                else
                {
                    *reinterpret_cast<void **>(tail) = current;
                    tail = current;
                }
            }
            current = next;
        }

        if (tail != nullptr)
        {
            *reinterpret_cast<void **>(tail) = nullptr;
        }

        m_centralFreeList[index].store(newHead, std::memory_order_relaxed);
        PageCache::getInstance()->deallocateSpan(spanAddr, numPages);
    }
}

void *CentralCache::fetchFromPageCache(size_t size)
{
    // 1. 计算需要的页数
    size_t numPages = (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;

    // 2. 根据大小决定分配策略
    if (size <= SPAN_PAGES * PageCache::PAGE_SIZE)
    {
        return PageCache::getInstance()->allocateSpan(SPAN_PAGES);
    }
    else
    {
        return PageCache::getInstance()->allocateSpan(numPages);
    }
}

SpanTracker *CentralCache::getSpanTracker(void *blockAddr)
{
    // 不能直接使用void* 进行比较，可能会出现UB
    // todo: 这里时间复杂度是O(n),后续可考虑优化为O(1),在block头部增加相应信息
    char *blk = reinterpret_cast<char *>(blockAddr);
    // 找到blockAddr 所属的span
    for (size_t i = 0; i < m_spanCount.load(std::memory_order_relaxed); i++)
    {
        char *startAddr = reinterpret_cast<char *>(m_spanTrackers[i].spanAddr.load(std::memory_order_relaxed));
        size_t numPages = m_spanTrackers[i].numPages.load(std::memory_order_relaxed);
        char *endAddr = startAddr + numPages * PageCache::PAGE_SIZE;
        if (blk >= startAddr && blk <= endAddr)
        {
            return &m_spanTrackers[i];
        }
    }
    return nullptr;
}
} // namespace MemoryPool_V2