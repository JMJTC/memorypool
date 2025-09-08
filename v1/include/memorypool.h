#ifndef __MEMORYPOOL_H__
#define __MEMORYPOOL_H__

#include <atomic>
#include <cstddef>
#include <mutex>

namespace MemoryPool
{
constexpr size_t DEFAULT_BLOCK_SIZE = 4096; // default block size
constexpr size_t DEFAULT_SLOT_SIZE = 32;    // default slot size
constexpr size_t MAX_SLOT_SIZE = 1024;      // maximum slot size
constexpr size_t MEMORY_POOL_NUM = 32;      // number of memory pools

struct Slot
{
	std::atomic<Slot*> next; // pointer to the next slot
};

class MemoryPool
{
public:
	MemoryPool(size_t blockSize = DEFAULT_BLOCK_SIZE, size_t slotSize = DEFAULT_SLOT_SIZE);
	~MemoryPool();

	void init(size_t slotSize);

	void* allocate();
	void deallocate(void* p);
private:
	void allocateNewBlock();
	size_t padPointer(char* p, size_t align);

private:
	size_t m_blockSize = 0; // size of each block
	size_t m_slotSize = 0;  // size of each slot
	Slot* m_pFirstBlock = nullptr; // pointer to the first block
	Slot* m_pCurSlot = nullptr;   // pointer to the current slot
	Slot* m_pLastSlot = nullptr;  // pointer to the last slot
	std::atomic<Slot*> m_freeSlots {nullptr}; // pointer to the free slots
	std::mutex m_mutexForBlock; // mutex for block allocation
};

class HashBucket
{
public:
	static void initMemoryPool();
	static MemoryPool& getMemoryPool(int index);

	static void* useMemory(size_t size);
	static void freeMemory(void* p, size_t size);

	template <typename T, typename... Args>
	friend T* newElement(Args&&... args);

	template <typename T>
	friend void deleteElement(T* p);
};

template <typename T, typename... Args>
T* newElement(Args&&... args)
{
	T* p = nullptr;
	void* mem = HashBucket::useMemory(sizeof(T));
	if (mem)
	{
		p = new (mem) T(std::forward<Args>(args)...);
	}
	return p;
}

template <typename T>
void deleteElement(T* p)
{
	if (p)
	{
		p->~T();
		HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T));
	}
}
} // namespace MemoryPool

#endif // __MEMORYPOOL_H__