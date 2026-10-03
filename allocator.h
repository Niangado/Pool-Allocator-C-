#pragma once
#define NOMINMAX
#include <algorithm>
#include <new>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <windows.h>w
#include <cstdint>
#include <iostream>





template <typename T>
class PoolAllocator {
private:
    struct Node {
        Node* next;
    };

    static constexpr std::size_t alignment{ std::max(alignof(T), alignof(Node)) };
    static constexpr std::size_t blockSize{ ((std::max(sizeof(T), sizeof(Node)) + alignment - 1) / alignment) * alignment };
    std::size_t blockCount;
    void* poolStart;
    Node* head;

    bool owns(T* p) const{
        if (p == nullptr || poolStart == nullptr) { return false; }
        std::uintptr_t start{ reinterpret_cast<std::uintptr_t>(poolStart) };
        std::uintptr_t addr{ reinterpret_cast<std::uintptr_t>(p) };
        if ((start <= addr && addr < start + blockSize * blockCount) && ((addr - start) % blockSize == 0)) {
            return true;
        }
            
        return false;
    }

    void deallocate(T* p) {
        if (!owns(p)) { throw std::invalid_argument{ "Pool does not own object" }; }
            Node* temp{ reinterpret_cast<Node*>(p) };
            temp->next = head;
            head = temp;
            std::cout << "Deallocated" << '\n';      
    }

    T* allocate() {
        if (head == nullptr) { throw std::bad_alloc{}; }
        T* temp{ reinterpret_cast<T*>(head) };
        head = head->next;

        std::cout << "Allocated" << '\n';
        return temp;
    }

public:
    PoolAllocator(std::size_t bc) :blockCount{ bc } {
        static_assert((blockSize % alignment) == 0, "blockSize is not aligned");
        static_assert(blockSize >= sizeof(Node), "block too small to hold a free-list node");
        if (blockCount == 0) { throw std::invalid_argument{ "blockCount zero" }; }
        poolStart = VirtualAlloc(nullptr, blockSize * blockCount, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (poolStart == nullptr) { throw std::bad_alloc{}; }
        head = static_cast<Node*>(poolStart);

        char* start{ static_cast<char*>(poolStart) };
        Node* temp{ head };
        for (std::size_t i{ 0 }; i < blockCount - 1; ++i) {
            temp->next = reinterpret_cast<Node*>(start + (i + 1) * blockSize);
            temp = temp->next;
        }
        temp->next = nullptr;
    }

    ~PoolAllocator() {
        if (poolStart == nullptr) { return; }
        VirtualFree(poolStart, 0, MEM_RELEASE);
    }

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;
    PoolAllocator(PoolAllocator&& temp) noexcept {
        this->poolStart = temp.poolStart;
        this->head = temp.head;
        this->blockCount = temp.blockCount;
        temp.poolStart = nullptr;
        temp.head = nullptr;
        temp.blockCount = 0;

        std::cout << "Moved" << '\n';
       
    }

    PoolAllocator& operator=(PoolAllocator&& temp) noexcept {
        if (this == &temp) { return *this; }
        if (this->poolStart != nullptr) { VirtualFree(this->poolStart, 0, MEM_RELEASE); }
        this->poolStart = temp.poolStart;
        this->head = temp.head;
        this->blockCount = temp.blockCount;
        temp.poolStart = nullptr;
        temp.head = nullptr;
        temp.blockCount = 0;
        std::cout << "Moved" << '\n';
        return *this;
    }
   
    

    template <typename... Args>
    T* create(Args&&... args) {
        T* alloc = allocate();
        try {
            new(alloc) T(std::forward<Args>(args)...);
        }
        catch (...) {
            deallocate(alloc);
            throw;
        }

        return alloc;
    }

 
   

    void destroy(T* p) {
        if (p == nullptr) { return; }
        if (!owns(p)) { throw std::invalid_argument{ "Pool does not own object" }; }
            p-> ~T();
            std::cout << "Destroyed" << '\n';
            deallocate(p);
            
    }
}; 
