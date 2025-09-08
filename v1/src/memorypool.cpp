#include "../include/memorypool.h"

namespace MemoryPool
{
MemoryPool::MemoryPool(size_t blockSize, size_t slotSize)
    : m_blockSize(blockSize)
    , m_slotSize(slotSize)
{
}

MemoryPool::~MemoryPool()
{
    Slot* cur = m_pFirstBlock;
    while (cur)
    {
        Slot* next = cur->next.load();
        operator delete(reinterpret_cast<void*>(cur));
        cur = next;
    }
}

void MemoryPool::init(size_t slotSize)
{
    if (slotSize > MAX_SLOT_SIZE)
    {
        slotSize = MAX_SLOT_SIZE;
    }
    m_slotSize = (slotSize < sizeof(Slot*)) ? sizeof(Slot*) : slotSize;
    m_pCurSlot = nullptr;
    m_pLastSlot = nullptr;
    m_freeSlots.store(nullptr);
}

void* MemoryPool::allocate()
{
    Slot* pFreeSlot = m_freeSlots.load();
    while (pFreeSlot)
    {
        if (m_freeSlots.compare_exchange_weak(pFreeSlot, pFreeSlot->next.load()))
        {
            return reinterpret_cast<void*>(pFreeSlot);
        }
    }

    std::lock_guard<std::mutex> lock(m_mutexForBlock);
    if (!m_pCurSlot || m_pCurSlot > m_pLastSlot)
    {
        // Need to allocate a new block
        allocateNewBlock();
    }
    return reinterpret_cast<void*>(m_pCurSlot++);
}

void MemoryPool::deallocate(void* p)
{
    if (!p)
    {
        return;
    }
    Slot* slot = reinterpret_cast<Slot*>(p);
    Slot* oldHead = m_freeSlots.load();
    do
    {
        slot->next.store(oldHead);
    } while (!m_freeSlots.compare_exchange_weak(oldHead, slot));
}

void MemoryPool::allocateNewBlock()
{
    // Allocate a new block of memory
    size_t blockSize = m_blockSize;
    if (blockSize < m_slotSize)
    {
        blockSize = m_slotSize;
    }
    // Align block size to be multiple of slot size
    void* newBlock = operator new(blockSize);
    Slot* newSlot = reinterpret_cast<Slot*>(newBlock);
    newSlot->next.store(m_pFirstBlock);
    m_pFirstBlock = newSlot;

    // Calculate number of slots that can fit in the block
    size_t numSlots = (blockSize - sizeof(Slot*)) / m_slotSize;
    m_pCurSlot = newSlot + 1; // First slot after the block header
    m_pLastSlot = m_pCurSlot + numSlots - 1;
}

void HashBucket::initMemoryPool()
{
    for (int i = 0; i < MEMORY_POOL_NUM; ++i)
    {
        getMemoryPool(i).init((i + 1) * DEFAULT_SLOT_SIZE);
    }
}

MemoryPool& HashBucket::getMemoryPool(int index)
{
    static MemoryPool memoryPools[MEMORY_POOL_NUM];
    if (index < 0 || index >= MEMORY_POOL_NUM)
    {
        throw std::out_of_range("MemoryPool index out of range");
    }
    return memoryPools[index];
}

void* HashBucket::useMemory(size_t size)
{
    if (size == 0)
    {
        return nullptr;
    }
    if (size > MAX_SLOT_SIZE)
    {
        return operator new(size);
    }
    // 向上取整
    int index = (size + DEFAULT_SLOT_SIZE - 1) / DEFAULT_SLOT_SIZE - 1;
    return getMemoryPool(index).allocate();
}

void HashBucket::freeMemory(void* p, size_t size)
{
    if (!p)
    {
        return;
    }
    if (size > MAX_SLOT_SIZE)
    {
        operator delete(p);
        return;
    }
    int index = (size + DEFAULT_SLOT_SIZE - 1) / DEFAULT_SLOT_SIZE - 1;
    getMemoryPool(index).deallocate(p);
}
} // namespace MemoryPool
