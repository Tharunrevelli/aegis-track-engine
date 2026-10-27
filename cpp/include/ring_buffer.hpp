#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <utility>

namespace aegis {

/**
 * @brief High-throughput Bounded Circular Ring Buffer with Atomic Spinlock
 * Designed for sub-microsecond radar strobe ingestion with zero runtime heap allocation.
 */
template <typename T, size_t Capacity>
class CircularRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2 for fast modulo indexing");

public:
    CircularRingBuffer() : head_(0), tail_(0), size_(0) {
        lock_.clear();
    }

    bool push(const T& item) {
        acquire();
        if (size_ >= Capacity) {
            release();
            return false; // Buffer overflow / drop
        }
        buffer_[head_] = item;
        head_ = (head_ + 1) & (Capacity - 1);
        ++size_;
        release();
        return true;
    }

    bool push(T&& item) {
        acquire();
        if (size_ >= Capacity) {
            release();
            return false;
        }
        buffer_[head_] = std::move(item);
        head_ = (head_ + 1) & (Capacity - 1);
        ++size_;
        release();
        return true;
    }

    bool pop(T& item) {
        acquire();
        if (size_ == 0) {
            release();
            return false; // Buffer empty
        }
        item = std::move(buffer_[tail_]);
        tail_ = (tail_ + 1) & (Capacity - 1);
        --size_;
        release();
        return true;
    }

    size_t size() const {
        acquire();
        size_t s = size_;
        release();
        return s;
    }

    bool empty() const {
        acquire();
        bool e = (size_ == 0);
        release();
        return e;
    }

    bool full() const {
        acquire();
        bool f = (size_ >= Capacity);
        release();
        return f;
    }

    void clear() {
        acquire();
        head_ = 0;
        tail_ = 0;
        size_ = 0;
        release();
    }

    constexpr size_t capacity() const {
        return Capacity;
    }

private:
    void acquire() const {
        while (lock_.test_and_set(std::memory_order_acquire)) {
            // Spin wait
        }
    }

    void release() const {
        lock_.clear(std::memory_order_release);
    }

    std::array<T, Capacity> buffer_;
    size_t head_;
    size_t tail_;
    size_t size_;
    mutable std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
};

} // namespace aegis
