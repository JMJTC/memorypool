#ifndef __MEMORYPOOL_COMMON_H__
#define __MEMORYPOOL_COMMON_H__

#include <atomic>
#include <cstddef>

namespace MemoryPool_V2
{
// 对其数和大小定义
constexpr size_t ALIGNMENT = 8;
constexpr size_t MAX_BYTES = 256 * 1024; // 256KB
constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT;

// 内存块头部信息
struct BlockHeader
{
    size_t size;       // 内存块大小
    bool isFree;       // 是否空闲
    BlockHeader *next; // 指向下一个内存块
};

// 大小类管理
class SizeClass
{
  public:
    // 内存对齐函数，返回最小 ALIGNMENT 整数倍
    static size_t roundUp(size_t bytes)
    {
        return (bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    }

    static size_t getIndex(size_t bytes)
    {
        if (bytes < ALIGNMENT)
        {
            bytes = ALIGNMENT;
        }
        // 向上取整
        return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
    }
};

} // namespace MemoryPool_V2

#endif // __MEMORYPOOL_COMMON_H__