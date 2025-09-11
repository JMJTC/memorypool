#include "../include/centralcache.h"
#include "../include/thraedcache.h"

#include <cstdlib>

namespace MemoryPool_V2
{
void *ThreadCache::allocate(size_t size)
{
    if (size == 0)
    {
        size = ALIGNMENT; // 至少分配一个对齐大小
    }

    if (size > MAX_BYTES)
    {
        // 大对象直接从系统分配
        return malloc(size);
    }

    size_t index = SizeClass::getIndex(size);

    // 检查线程本地自由链表
    // 如果 m_freeList[index] 不为空，表示该链表中有可用内存块
    if (m_freeList[index] != nullptr)
    {
        // 更新对应自由链表的长度计数
        m_freeListSize[index]--;
        void *ptr = m_freeList[index];
        m_freeList[index] =
            *reinterpret_cast<void **>(ptr); // 将m_freeList[index]指向的内存块的下一个内存块地址（取决于内存块的实现）
        return ptr;
    }

    return fetchFromCentralCache(index);
}

void ThreadCache::deallocate(void *ptr, size_t size)
{
    if (size > MAX_BYTES)
    {
        free(ptr);
        return;
    }

    size_t index = SizeClass::getIndex(size);
    *reinterpret_cast<void **>(ptr) = m_freeList[index];
    m_freeList[index] = ptr;

    m_freeListSize[index]++;
    if (shouldReturnToCentralCache(index))
    {
        returnToCentralCache(m_freeList[index], size);
    }
}

bool ThreadCache::shouldReturnToCentralCache(size_t index)
{
    // 简单策略：当自由链表大小超过一定阈值时，归还部分内存给中心缓存
    const size_t THRESHOLD = 64; // 可以根据需要调整阈值
    return m_freeListSize[index] > THRESHOLD;
}

void ThreadCache::returnToCentralCache(void *start, size_t size)
{
    size_t index = SizeClass::getIndex(size);
    size_t alignedSize = SizeClass::roundUp(size);
    size_t totalNum = m_freeListSize[index];

    if (totalNum <= 1)
        return;

    // 保留四分之一在 ThreadCache 中，其他归还给 CentralCache
    size_t keepNum = std::max(totalNum / 4, size_t(1));
    size_t returnNum = totalNum - keepNum;

    // 保留的串联起来
    char *current = static_cast<char *>(start);
    char *splitNode = current;
    for (size_t i = 0; i < keepNum - 1; i++)
    {
        splitNode = reinterpret_cast<char *>(*reinterpret_cast<void **>(splitNode));
        if (splitNode == nullptr)
        {
            returnNum = totalNum - i - 1;
            break;
        }
    }

    if (splitNode != nullptr)
    {
        // 将链表断开
        void *nextNode = *reinterpret_cast<void **>(splitNode);
        *reinterpret_cast<void **>(splitNode) = nullptr;

        m_freeList[index] = start;
        m_freeListSize[index] = keepNum;

        if (returnNum > 0 && nextNode != nullptr)
        {
            CentralCache::getInstance()->returnRange(nextNode, returnNum * alignedSize, index);
        }
    }
}

void *ThreadCache::fetchFromCentralCache(size_t index)
{
    void *start = CentralCache::getInstance()->fetchRange(index);
    if (start == nullptr)
    {
        return nullptr;
    }

    // 取一个返回，其余放入自由链表
    void *res = start;
    m_freeList[index] = *reinterpret_cast<void **>(start);
    size_t totalNum = 0;
    void *current = start;
    while (current != nullptr)
    {
        totalNum++;
        current = *reinterpret_cast<void **>(current);
    }
    m_freeListSize[index] += (totalNum - 1); // 减去一个返回的
    return res;
}

} // namespace MemoryPool_V2