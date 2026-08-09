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
        , cached_tail_(0)
        , tail_(0) 
        , cached_head_(0)
    {
        if (capacity < 2 || (capacity & (capacity - 1)) != 0) {
            throw std::invalid_argument("Capacity must be a power of 2 and at least 2");
        }
    }

    bool push(const T& item) {
        auto current_tail = tail_.load(std::memory_order_relaxed);
        auto next_tail = increment(current_tail);
        if (next_tail == cached_head_) {
            cached_head_ = head_.load(std::memory_order_acquire);
            if (next_tail == cached_head_) {
                return false;
            }
        }
        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    bool push(T&& item) {
        auto current_tail = tail_.load(std::memory_order_relaxed);
        auto next_tail = increment(current_tail);
        if (next_tail == cached_head_) {
            cached_head_ = head_.load(std::memory_order_acquire);
            if (next_tail == cached_head_) {
                return false;
            }
        }
        buffer_[current_tail] = std::move(item);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        auto current_head = head_.load(std::memory_order_relaxed);
        if (current_head == cached_tail_) {
            cached_tail_ = tail_.load(std::memory_order_acquire);
            if (current_head == cached_tail_) {
                return false;
            }
        }
        item = std::move(buffer_[current_head]);
        head_.store(increment(current_head), std::memory_order_release);
        return true;
    }

private:
    std::size_t increment(std::size_t idx) const {
        return (idx + 1) & (capacity_ - 1);
    }

    std::size_t capacity_;
    std::vector<T> buffer_;
    
    // SPSC "cached cursor" pattern:
    // We cache the other thread's atomic index to reduce cache-line contention.
    // The producer only needs to load the atomic head_ when its cached_head_
    // indicates the queue is full. The consumer only needs to load the atomic
    // tail_ when its cached_tail_ indicates the queue is empty.
    alignas(hardware_destructive_interference_size) std::atomic<std::size_t> head_;
    std::size_t cached_tail_;
    
    alignas(hardware_destructive_interference_size) std::atomic<std::size_t> tail_;
    std::size_t cached_head_;
};

}
