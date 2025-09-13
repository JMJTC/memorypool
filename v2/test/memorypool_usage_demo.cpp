#include "../include/memorypool.h"
#include <iostream>
#include <string>

// A simple test class to demonstrate object allocation
class TestClass {
private:
    int value_;
    std::string name_;

public:
    TestClass() : value_(0), name_("Default") {
        std::cout << "TestClass default constructor called" << std::endl;
    }

    TestClass(int v, const std::string& n) : value_(v), name_(n) {
        std::cout << "TestClass parameterized constructor called: " 
                  << value_ << ", " << name_ << std::endl;
    }

    ~TestClass() {
        std::cout << "TestClass destructor called: " 
                  << value_ << ", " << name_ << std::endl;
    }

    void print() const {
        std::cout << "TestClass: value = " << value_ 
                  << ", name = " << name_ << std::endl;
    }
};

int main() {
    std::cout << "=== MemoryPool Interface Optimization Demo ===\n" << std::endl;

    // ===========================
    // Original void* Interface
    // ===========================
    std::cout << "\n1. Using original void* interface:\n" << std::endl;
    void* rawMemory = MemoryPool::allocate(sizeof(int));
    int* intPtr = static_cast<int*>(rawMemory);
    *intPtr = 42;
    std::cout << "Allocated int value: " << *intPtr << std::endl;
    MemoryPool::deallocate(rawMemory, sizeof(int));

    // ===========================
    // New Type-safe Interface
    // ===========================
    std::cout << "\n2. Using new type-safe single object allocation:\n" << std::endl;
    int* typeSafeInt = MemoryPool::allocate<int>();
    *typeSafeInt = 100;
    std::cout << "Type-safe allocated int value: " << *typeSafeInt << std::endl;
    MemoryPool::deallocate(typeSafeInt);

    // ===========================
    // Array Allocation
    // ===========================
    std::cout << "\n3. Using array allocation interface:\n" << std::endl;
    double* doubleArray = MemoryPool::allocateArray<double>(5);
    for (int i = 0; i < 5; i++) {
        doubleArray[i] = i * 1.1;
        std::cout << "doubleArray[" << i << "] = " << doubleArray[i] << std::endl;
    }
    MemoryPool::deallocateArray(doubleArray);

    // ===========================
    // Object Construction/Destruction
    // ===========================
    std::cout << "\n4. Using newObject/deleteObject for constructor/destructor calls:\n" << std::endl;
    
    // Default constructor
    TestClass* defaultObj = MemoryPool::newObject<TestClass>();
    defaultObj->print();
    
    // Parameterized constructor
    TestClass* paramObj = MemoryPool::newObject<TestClass>(100, "TestObject");
    paramObj->print();
    
    // Delete objects (will call destructors)
    MemoryPool::deleteObject(defaultObj);
    MemoryPool::deleteObject(paramObj);

    std::cout << "\n=== Demo Completed ===" << std::endl;
    return 0;
}