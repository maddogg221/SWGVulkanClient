// Tests for ThreadSafeQueue<T> (Phase 14, engine loop redesign) - the shared
// primitive behind every producer/consumer boundary between the network,
// asset-worker, and render threads. Covers the actual cross-thread behavior,
// not just single-threaded push/pop bookkeeping - a queue whose thread-safety
// claim is untested is worse than no claim at all.
#include <doctest/doctest.h>

#include <atomic>
#include <thread>
#include <vector>

#include "soe/ThreadSafeQueue.h"

TEST_CASE("ThreadSafeQueue: single-threaded push/pop preserves FIFO order") {
    soe::ThreadSafeQueue<int> queue;
    queue.push(1);
    queue.push(2);
    queue.push(3);

    CHECK(queue.pop(std::chrono::milliseconds(0)) == 1);
    CHECK(queue.pop(std::chrono::milliseconds(0)) == 2);
    CHECK(queue.pop(std::chrono::milliseconds(0)) == 3);
}

TEST_CASE("ThreadSafeQueue: pop times out with nullopt on an empty queue") {
    soe::ThreadSafeQueue<int> queue;
    auto result = queue.pop(std::chrono::milliseconds(20));
    CHECK(!result.has_value());
}

TEST_CASE("ThreadSafeQueue: tryPop is non-blocking and returns nullopt when empty") {
    soe::ThreadSafeQueue<int> queue;
    CHECK(!queue.tryPop().has_value());
    queue.push(42);
    auto result = queue.tryPop();
    REQUIRE(result.has_value());
    CHECK(*result == 42);
    CHECK(!queue.tryPop().has_value());
}

TEST_CASE("ThreadSafeQueue: empty() reflects current state") {
    soe::ThreadSafeQueue<int> queue;
    CHECK(queue.empty());
    queue.push(1);
    CHECK(!queue.empty());
    queue.pop(std::chrono::milliseconds(0));
    CHECK(queue.empty());
}

TEST_CASE("ThreadSafeQueue: a blocking pop wakes up as soon as another thread pushes") {
    soe::ThreadSafeQueue<int> queue;
    std::atomic<bool> received{false};

    std::thread consumer([&] {
        auto result = queue.pop(std::chrono::seconds(5));
        received = result.has_value() && *result == 99;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    queue.push(99);
    consumer.join();

    CHECK(received.load());
}

TEST_CASE("ThreadSafeQueue: concurrent producers/consumers move every item exactly once") {
    soe::ThreadSafeQueue<int> queue;
    constexpr int kProducers = 4;
    constexpr int kItemsPerProducer = 500;
    constexpr int kTotalItems = kProducers * kItemsPerProducer;

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&queue, p] {
            for (int i = 0; i < kItemsPerProducer; ++i) {
                queue.push(p * kItemsPerProducer + i);
            }
        });
    }

    std::atomic<int> consumedCount{0};
    std::vector<std::thread> consumers;
    for (int c = 0; c < 2; ++c) {
        consumers.emplace_back([&] {
            while (consumedCount.load() < kTotalItems) {
                auto result = queue.pop(std::chrono::milliseconds(50));
                if (result.has_value()) {
                    ++consumedCount;
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    CHECK(consumedCount.load() == kTotalItems);
    CHECK(queue.empty());
}
