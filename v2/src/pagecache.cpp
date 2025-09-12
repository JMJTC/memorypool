#include "../include/pagecache.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <cstring>

namespace MemoryPool_V2
{
void *PageCache::allocateSpan(size_t numPages)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 使用有序容器特有的查找接口
    // auto it1 = m.lower_bound(2); // 第一个 >= 2 的迭代器
    // auto it2 = m.upper_bound(2); // 第一个 > 2 的迭代器
    auto it = m_freeSpans.lower_bound(numPages);
    if (it != m_freeSpans.end())
    {
        Span *span = it->second;
        Span *next = span->next;

        if (span->next != nullptr)
        {
            it->second = next;
        }
        else
        {
            m_freeSpans.erase(it);
        }

        // span太大就分割
        if (span->numPages > numPages)
        {
            char *temp = reinterpret_cast<char *>(span->pageAddr) + numPages * PAGE_SIZE;
            Span *newSpan = new Span{reinterpret_cast<void *>(temp), span->numPages - numPages, nullptr};

            // 放入对应列表头部
            auto &list = m_freeSpans[newSpan->numPages];
            newSpan->next = list;
            list = newSpan;

            span->numPages = numPages;
            // 分割的同样需要记录信息
            m_spanMap[newSpan->pageAddr] = span;
        }
        // 记录信息
        m_spanMap[span->pageAddr] = span;
        return span->pageAddr;
    }

    // 向系统申请
    void *memory = systemAlloc(numPages);
    if (memory == nullptr)
    {
        // todo : 分配失败，检查是否有对应的异常处理
        return nullptr;
    }

    Span *span = new Span{memory, numPages, nullptr};

    m_spanMap[memory] = span;
    return memory;
}

void PageCache::deallocateSpan(void *ptr, size_t numPages)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_spanMap.find(ptr);
    if (it == m_spanMap.end())
    {
        // 不是pagecache分配的内存
        return;
    }
    Span *span = it->second;
    // 尝试合并相邻psan
    void *nextAddr = static_cast<void *>(static_cast<char *>(ptr) + numPages * PAGE_SIZE);
    auto nextIt = m_spanMap.find(nextAddr);
    if (nextIt != m_spanMap.end())
    {
        Span *nextSpan = nextIt->second;

        // 判断是否再空闲链表中
        bool flag = false;
        auto &nextList = m_freeSpans[nextSpan->numPages];

        if (nextList == nextSpan)
        {
            nextList = nextSpan->next;
            flag = true;
        }
        else if (nextList != nullptr)
        {
            Span *prev = nextList;
            while (prev->next != nullptr)
            {
                if (prev->next == nextSpan)
                {
                    // remove
                    prev->next = nextSpan->next;
                    flag = true;
                    break;
                }
                prev = prev->next;
            }
        }

        // 合并
        if (flag)
        {
            span->numPages += nextSpan->numPages;
            m_spanMap.erase(nextAddr);
            delete nextSpan;
        }
    }
    auto &list = m_freeSpans[span->numPages];
    span->next = list;
    list = span;
}

void *PageCache::systemAlloc(size_t numPages)
{
    size_t size = numPages * PAGE_SIZE;
#if defined(_WIN32) || defined(_WIN64)
    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE,                  // 匿名映射
                                    nullptr,                               // 默认安全属性
                                    PAGE_READWRITE,                        // 可读写
                                    static_cast<DWORD>(size >> 32),        // 高 32 位
                                    static_cast<DWORD>(size & 0xFFFFFFFF), // 低 32 位
                                    nullptr);
    if (!hMap)
    {
        return nullptr;
    }

    void *ptr = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, size);
    CloseHandle(hMap); // 关闭句柄，不影响映射
    if (!ptr)
    {
        return nullptr;
    }

    memset(ptr, 0, size); // 初始化为 0
    return ptr;
#else
    void *ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED)
    {
        return nullptr;
    }
    // 置0
    memset(ptr, 0, size);
    return ptr;
#endif
}
} // namespace MemoryPool_V2