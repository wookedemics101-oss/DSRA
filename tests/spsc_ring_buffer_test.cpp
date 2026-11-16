#include "titan/spsc_ring_buffer.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>

namespace allocation_probe {
std::atomic<bool> enabled{false};
std::atomic<std::size_t> allocations{0};
}

void* operator new(std::size_t size) {
    if (allocation_probe::enabled.load(std::memory_order_relaxed)) {
        allocation_probe::allocations.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* ptr = std::malloc(size)) {
        return ptr;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (allocation_probe::enabled.load(std::memory_order_relaxed)) {
        allocation_probe::allocations.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* ptr = std::malloc(size)) {
        return ptr;
    }
    throw std::bad_alloc();
}

void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }

namespace {
struct Record {
    std::uint64_t sequence;
    std::uint32_t payload;
};

using Queue = SPSCRingBuffer<Record, 4>;

bool test_empty_full_and_fifo() {
    Queue queue;
    Record out{};
    if (!queue.empty_approx() || queue.pop(out)) return false;

    for (std::uint64_t i = 0; i < Queue::capacity(); ++i) {
        if (!queue.push(Record{i, static_cast<std::uint32_t>(i + 10)})) return false;
    }
    if (queue.push(Record{99, 99})) return false;
    if (queue.size_approx() != Queue::capacity()) return false;

    for (std::uint64_t i = 0; i < Queue::capacity(); ++i) {
        if (!queue.pop(out)) return false;
        if (out.sequence != i || out.payload != i + 10) return false;
    }
    return queue.empty_approx() && !queue.pop(out);
}

bool test_slot_reuse_and_counter_wrap() {
    // A small unsigned counter makes wraparound reachable in a normal test.
    using WrappingQueue = SPSCRingBuffer<Record, 4, std::uint8_t>;
    WrappingQueue queue;
    Record out{};
    for (std::uint64_t i = 0; i < 600; ++i) {
        if (!queue.push(Record{i, static_cast<std::uint32_t>(i ^ 0x55U)})) return false;
        if (!queue.pop(out)) return false;
        if (out.sequence != i || out.payload != (i ^ 0x55U)) return false;
        if (queue.size_approx() != 0) return false;
    }
    return true;
}

bool test_queue_operations_do_not_allocate() {
    Queue queue;
    Record out{};
    allocation_probe::allocations.store(0, std::memory_order_relaxed);
    allocation_probe::enabled.store(true, std::memory_order_relaxed);

    const bool pushed = queue.push(Record{7, 11});
    const bool popped = queue.pop(out);
    const bool full_cycle_ok = pushed && popped && out.sequence == 7 && out.payload == 11;

    allocation_probe::enabled.store(false, std::memory_order_relaxed);
    return full_cycle_ok &&
           allocation_probe::allocations.load(std::memory_order_relaxed) == 0;
}

bool test_producer_consumer_stress() {
    constexpr std::uint64_t item_count = 500'000;
    constexpr std::size_t stress_capacity = 1024;
    SPSCRingBuffer<std::uint64_t, stress_capacity> queue;
    std::atomic<bool> failed{false};

    std::thread producer([&] {
        for (std::uint64_t i = 0; i < item_count; ++i) {
            while (!queue.push(i)) {
                if (failed.load(std::memory_order_relaxed)) return;
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        std::uint64_t expected = 0;
        while (expected < item_count) {
            std::uint64_t value = 0;
            if (queue.pop(value)) {
                if (value != expected) {
                    failed.store(true, std::memory_order_relaxed);
                    return;
                }
                ++expected;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();
    return !failed.load(std::memory_order_relaxed) && queue.empty_approx();
}

bool run(const char* name, bool (*test)()) {
    const bool passed = test();
    std::cout << (passed ? "[PASS] " : "[FAIL] ") << name << '\n';
    return passed;
}
} // namespace

int main() {
    bool passed = true;
    passed &= run("empty/full/FIFO", test_empty_full_and_fifo);
    passed &= run("slot reuse and counter wraparound", test_slot_reuse_and_counter_wrap);
    passed &= run("no allocations in push/pop", test_queue_operations_do_not_allocate);
    passed &= run("producer/consumer stress", test_producer_consumer_stress);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
