#include "memorypool.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>

// 测试基本的分配和释放功能
void testBasicAllocation() {
    std::cout << "===== 测试基本分配释放功能 ======" << std::endl;
    
    // 测试不同大小的内存分配
    void* p1 = MemoryPool::allocate(16);
    void* p2 = MemoryPool::allocate(32);
    void* p3 = MemoryPool::allocate(128);
    
    assert(p1 != nullptr);
    assert(p2 != nullptr);
    assert(p3 != nullptr);
    
    // 写入数据测试
    memset(p1, 1, 16);
    memset(p2, 2, 32);
    memset(p3, 3, 128);
    
    // 验证数据正确性
    assert(*static_cast<char*>(p1) == 1);
    assert(*static_cast<char*>(p2) == 2);
    assert(*static_cast<char*>(p3) == 3);
    
    // 释放内存
    MemoryPool::deallocate(p1, 16);
    MemoryPool::deallocate(p2, 32);
    MemoryPool::deallocate(p3, 128);
    
    std::cout << "基本分配释放测试通过！" << std::endl;
}

// 测试类型安全的分配功能
void testTypeSafeAllocation() {
    std::cout << "\n===== 测试类型安全分配功能 ======" << std::endl;
    
    // 测试类型安全的单对象分配
    int* pInt = MemoryPool::allocate<int>();
    *pInt = 42;
    assert(*pInt == 42);
    MemoryPool::deallocate(pInt);
    
    // 测试数组分配
    int* pArray = MemoryPool::allocateArray<int>(10);
    for (int i = 0; i < 10; ++i) {
        pArray[i] = i * 10;
    }
    
    for (int i = 0; i < 10; ++i) {
        assert(pArray[i] == i * 10);
    }
    
    MemoryPool::deallocateArray(pArray);
    
    std::cout << "类型安全分配测试通过！" << std::endl;
}

// 测试对象的构造和析构
class TestObject {
private:
    int m_value;
    static int s_count;
public:
    TestObject() : m_value(0) { s_count++; }
    TestObject(int value) : m_value(value) { s_count++; }
    ~TestObject() { s_count--; }
    
    int getValue() const { return m_value; }
    static int getCount() { return s_count; }
};

int TestObject::s_count = 0;

void testObjectConstruction() {
    std::cout << "\n===== 测试对象构造析构功能 ======" << std::endl;
    
    // 测试无参构造
    TestObject* obj1 = MemoryPool::newObject<TestObject>();
    assert(obj1->getValue() == 0);
    assert(TestObject::getCount() == 1);
    
    // 测试有参构造
    TestObject* obj2 = MemoryPool::newObject<TestObject>(100);
    assert(obj2->getValue() == 100);
    assert(TestObject::getCount() == 2);
    
    // 测试对象删除
    MemoryPool::deleteObject(obj1);
    assert(TestObject::getCount() == 1);
    
    MemoryPool::deleteObject(obj2);
    assert(TestObject::getCount() == 0);
    
    std::cout << "对象构造析构测试通过！" << std::endl;
}

// 测试多线程环境下的内存分配
void testMultithreadedAllocation() {
    std::cout << "\n===== 测试多线程分配功能 ======" << std::endl;
    
    const int numThreads = 8;
    const int allocationsPerThread = 1000;
    
    std::vector<std::thread> threads;
    
    auto threadFunc = [allocationsPerThread]() {
        std::vector<void*> pointers;
        pointers.reserve(allocationsPerThread);
        
        // 分配内存
        for (int i = 0; i < allocationsPerThread; ++i) {
            size_t size = (i % 100 + 1) * 8; // 8字节到800字节的随机大小
            void* ptr = MemoryPool::allocate(size);
            pointers.push_back(ptr);
            
            // 写入数据
            memset(ptr, i % 256, size);
        }
        
        // 释放所有内存
        for (int i = 0; i < allocationsPerThread; ++i) {
            size_t size = (i % 100 + 1) * 8;
            MemoryPool::deallocate(pointers[i], size);
        }
    };
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 创建多个线程
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(threadFunc);
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    std::cout << "多线程测试通过！共" << numThreads << "个线程，每个线程" << allocationsPerThread << "次分配，耗时: " 
              << duration.count() << "ms" << std::endl;
}

int main() {
    try {
        std::cout << "开始内存池单元测试..." << std::endl;
        
        testBasicAllocation();
        testTypeSafeAllocation();
        testObjectConstruction();
        testMultithreadedAllocation();
        
        std::cout << "\n所有单元测试通过！" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "测试失败: " << e.what() << std::endl;
        return 1;
    }
}