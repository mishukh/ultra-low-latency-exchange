#pragma once
#include <atomic>
#include <vector>
#include <cstddef>
#include <stdexcept>
#include <new>

#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_destructive_interference_size;
#else
    constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

namespace exchange {

template<typename T>
class SPSCQueue {
public:
    explicit SPSCQueue(std::size_t capacity)
        : capacity_(capacity)
        , buffer_(capacity)
        , head_(0)
        , tail_(0) 
    {
        if (capacity < 2) {
            throw std::invalid_argument("Capacity must be at least 2");
        }
    }

    bool push(const T& item) {
        auto current_tail = tail_.load(std::memory_order_relaxed);
        auto next_tail = increment(current_tail);
        if (next_tail != head_.load(std::memory_order_acquire)) {
            buffer_[current_tail] = item;
            tail_.store(next_tail, std::memory_order_release);
            return true;
        }
        return false;
    }

    bool push(T&& item) {
        auto current_tail = tail_.load(std::memory_order_relaxed);
        auto next_tail = increment(current_tail);
        if (next_tail != head_.load(std::memory_order_acquire)) {
            buffer_[current_tail] = std::move(item);
            tail_.store(next_tail, std::memory_order_release);
            return true;
        }
        return false;
    }

    bool pop(T& item) {
        auto current_head = head_.load(std::memory_order_relaxed);
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        item = std::move(buffer_[current_head]);
        head_.store(increment(current_head), std::memory_order_release);
        return true;
    }

private:
    std::size_t increment(std::size_t idx) const {
        return (idx + 1) % capacity_;
    }

    std::size_t capacity_;
    std::vector<T> buffer_;
    
    alignas(hardware_destructive_interference_size) std::atomic<std::size_t> head_;
    alignas(hardware_destructive_interference_size) std::atomic<std::size_t> tail_;
};

}
