#include <iostream>

#include "../include/memorypool.h"

using namespace MemoryPool;

int main()
{
    HashBucket::initMemoryPool();

    // Test memory allocation
    int* pInt = newElement<int>(42);
    if (pInt)
    {
        std::cout << "Allocated int: " << *pInt << std::endl;
    }

    // Test memory deallocation
    deleteElement(pInt);
    std::cout << "Deallocated int." << std::endl;
    return 0;
}