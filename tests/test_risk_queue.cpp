#include <gtest/gtest.h>
#include "RiskEngine.h"
#include "LockFreeQueue.h"
#include <thread>

using namespace exchange;

TEST(RiskEngineTest, FatFingerQuantity) {
    RiskConfig config;
    config.maxOrderQuantity = 1000;
    RiskEngine risk(config);
    
    // Valid order
    EXPECT_TRUE(risk.validateOrder(1, 1, 100, 500, Side::BUY));
    
    // Fat finger quantity
    EXPECT_FALSE(risk.validateOrder(2, 1, 100, 1500, Side::BUY));
}

TEST(RiskEngineTest, FatFingerValue) {
    RiskConfig config;
    config.maxOrderValue = 10000; // 10k max value
    RiskEngine risk(config);
    
    // Valid order (value = 5000)
    EXPECT_TRUE(risk.validateOrder(1, 1, 100, 50, Side::BUY));
    
    // Fat finger value (value = 20000)
    EXPECT_FALSE(risk.validateOrder(2, 1, 100, 200, Side::BUY));
}

TEST(LockFreeQueueTest, BasicPushPop) {
    SPSCQueue<int> queue(16);
    
    EXPECT_TRUE(queue.push(1));
    EXPECT_TRUE(queue.push(2));
    
    int val = 0;
    EXPECT_TRUE(queue.pop(val));
    EXPECT_EQ(val, 1);
    
    EXPECT_TRUE(queue.pop(val));
    EXPECT_EQ(val, 2);
    
    EXPECT_FALSE(queue.pop(val)); // Empty
}

TEST(LockFreeQueueTest, CapacityLimit) {
    SPSCQueue<int> queue(4); // Capacity 4 means it can hold 3 items
    
    EXPECT_TRUE(queue.push(1));
    EXPECT_TRUE(queue.push(2));
    EXPECT_TRUE(queue.push(3));
    EXPECT_FALSE(queue.push(4)); // Full
}

TEST(LockFreeQueueTest, MultiThreadedProducerConsumer) {
    SPSCQueue<int> queue(1024);
    int num_items = 100000;
    
    std::thread producer([&]() {
        for (int i = 0; i < num_items; ++i) {
            while (!queue.push(i)) {
                std::this_thread::yield();
            }
        }
    });
    
    std::thread consumer([&]() {
        int expected = 0;
        int val = 0;
        while (expected < num_items) {
            if (queue.pop(val)) {
                EXPECT_EQ(val, expected);
                expected++;
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
}
