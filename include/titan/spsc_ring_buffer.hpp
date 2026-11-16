#pragma once

#include <atomic>
#include <cstddef>
#include <limits>
#include <type_traits>

// Fixed-capacity SPSC queue for bounded telemetry records.
// Exactly one thread may call push(), and exactly one other thread may call
// pop(). The queue does not provide multi-producer or multi-consumer safety.
template <typename T, std::size_t Capacity, typename Counter = std::size_t>
class SPSCRingBuffer final {
    static_assert(Capacity > 0, "Capacity must be greater than zero");
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");
    static_assert(std::is_integral<Counter>::value &&
                      std::is_unsigned<Counter>::value,
                  "Counter must be an unsigned integer type");
    static_assert(Capacity <=
                      static_cast<std::size_t>(
                          std::numeric_limits<Counter>::max() / 2),
                  "Capacity exceeds the safe counter range");
    static_assert(std::atomic<Counter>::is_always_lock_free,
                  "Target must support always-lock-free queue counters");
    static_assert(std::is_trivially_copyable<T>::value,
                  "Records must be trivially copyable");
    static_assert(std::is_trivially_default_constructible<T>::value,
                  "Records must be trivially default constructible");

public:
    SPSCRingBuffer() noexcept = default;

    SPSCRingBuffer(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer(SPSCRingBuffer&&) = delete;
    SPSCRingBuffer& operator=(SPSCRingBuffer&&) = delete;

    // Producer thread only. Returns false immediately when full.
    bool push(const T& item) noexcept {
        const Counter head = m_head.load(std::memory_order_relaxed);
        const Counter tail = m_tail.load(std::memory_order_acquire);

        // Unsigned modular subtraction is intentional: it remains correct
        // across counter wrap while the queue occupancy is bounded by Capacity.
        if (static_cast<Counter>(head - tail) >= Capacity) {
            return false;
        }

        m_buffer[static_cast<std::size_t>(head) & MASK] = item;
        m_head.store(static_cast<Counter>(head + 1), std::memory_order_release);
        return true;
    }

    // Consumer thread only. Returns false immediately when empty.
    bool pop(T& item) noexcept {
        const Counter tail = m_tail.load(std::memory_order_relaxed);
        const Counter head = m_head.load(std::memory_order_acquire);

        if (tail == head) {
            return false;
        }

        item = m_buffer[static_cast<std::size_t>(tail) & MASK];
        m_tail.store(static_cast<Counter>(tail + 1), std::memory_order_release);
        return true;
    }

    // Approximate concurrent diagnostic only; not a synchronization primitive.
    // Inconsistent snapshots are reported as zero rather than an invalid count.
    std::size_t size_approx() const noexcept {
        const Counter head = m_head.load(std::memory_order_acquire);
        const Counter tail = m_tail.load(std::memory_order_acquire);
        const Counter count = static_cast<Counter>(head - tail);
        return count <= Capacity ? static_cast<std::size_t>(count) : 0;
    }

    bool empty_approx() const noexcept {
        return size_approx() == 0;
    }

    static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

private:
    static constexpr std::size_t MASK = Capacity - 1;

    // 64 bytes is a target baseline, not a universal cache-line guarantee.
    alignas(64) std::atomic<Counter> m_head{0};
    alignas(64) std::atomic<Counter> m_tail{0};
    alignas(64) T m_buffer[Capacity]{};
};
