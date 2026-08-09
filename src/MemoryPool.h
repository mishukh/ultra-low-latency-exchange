#pragma once
#include <vector>
#include <utility>
#include <cassert>

namespace exchange {

template <typename T>
class MemoryPool {
public:
    explicit MemoryPool(size_t capacity) {
        pool_.resize(capacity);
        freeIndices_.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            freeIndices_.push_back(capacity - 1 - i);
        }
    }

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    template <typename... Args>
    T* allocate(Args&&... args) {
        if (freeIndices_.empty()) {
            return nullptr; 
        }
        size_t index = freeIndices_.back();
        freeIndices_.pop_back();
        
        T* obj = &pool_[index];
        obj->reset(std::forward<Args>(args)...);
        return obj;
    }

    void deallocate(T* ptr) {
        if (!ptr) return;
        size_t index = ptr - pool_.data();
        assert(index < pool_.size());
        freeIndices_.push_back(index);
    }

private:
    std::vector<T> pool_;
    std::vector<size_t> freeIndices_;
};

}
