#include "Exchange.h"
#include <iostream>
#if defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>
#include <sched.h>
#endif
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>

using namespace exchange;

inline uint64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

int main() {
    std::cout << "Starting Ultra-Low Latency Exchange Simulator Benchmarks..." << std::endl;

    constexpr int NUM_ORDERS = 20000000;
    std::vector<OrderRequest> orders(NUM_ORDERS);

    std::cout << "Pre-generating orders..." << std::endl;
    std::mt19937 gen(42);
    std::uniform_int_distribution<Price> priceDist(490, 510);
    std::uniform_int_distribution<Quantity> qtyDist(10, 1000);
    std::uniform_int_distribution<int> sideDist(0, 1);

    for (int i = 0; i < NUM_ORDERS; ++i) {
        orders[i].id = i + 1;
        orders[i].instrument = 1;
        orders[i].price = priceDist(gen);
        orders[i].quantity = qtyDist(gen);
        orders[i].side = static_cast<Side>(sideDist(gen));
        orders[i].type = OrderType::LIMIT;
    }

    std::cout << "Starting Exchange..." << std::endl;
    constexpr size_t MAX_ORDERS = 20000000;
    Exchange exchange(MAX_ORDERS, 1); // Pin engine thread to core 1
    exchange.start();

    std::vector<uint64_t> latencies;
    latencies.reserve(NUM_ORDERS);

    std::cout << "Sending " << NUM_ORDERS << " orders..." << std::endl;

    auto startTime = std::chrono::high_resolution_clock::now();
    std::atomic<int> receivedCount{0};
    std::atomic<uint64_t> producerBlockedCount{0};
    
    std::thread consumer([&]() {
#if defined(__linux__)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(2, &cpuset); // Pin consumer thread to core 2
        int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            std::cerr << "Warning: Failed to set thread affinity for consumer thread to core 2\n";
        }
#endif
        
        auto& mdQueue = exchange.getMarketDataQueue();
        MarketDataEvent event;
        auto consumerStart = std::chrono::high_resolution_clock::now();
        while (receivedCount < NUM_ORDERS) {
            if (mdQueue.pop(event)) {
                uint64_t recvTime = nowNs();
                uint64_t latency = recvTime - event.sentTime;
                latencies.push_back(latency);
                receivedCount++;
            }
            
            auto now = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - consumerStart).count() > 5) {
                std::cout << "Consumer timeout! Received: " << receivedCount << " / " << NUM_ORDERS << std::endl;
                break;
            }
        }
    });

    // Pin producer (main thread) to core 0
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        std::cerr << "Warning: Failed to set thread affinity for producer thread to core 0\n";
    }
#endif

    for (int i = 0; i < NUM_ORDERS; ++i) {
        orders[i].sentTime = nowNs();
        auto sendStart = std::chrono::high_resolution_clock::now();
        bool wasBlocked = false;
        while (!exchange.sendOrder(orders[i])) {
            if (!wasBlocked) {
                producerBlockedCount++;
                wasBlocked = true;
            }
            auto now = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - sendStart).count() > 5) {
                std::cout << "Producer timeout at order " << i << std::endl;
                break;
            }
        }
        
        uint64_t targetTime = orders[i].sentTime + 200; 
        while (nowNs() < targetTime) {
        }
    }

    consumer.join();

    auto endTime = std::chrono::high_resolution_clock::now();
    double totalSeconds = std::chrono::duration<double>(endTime - startTime).count();

    exchange.stop();

    std::cout << "\n================ LATENCY METRICS ================\n";
    std::cout << "Orders Processed: " << receivedCount << "\n";
    if (receivedCount > 0) {
        std::sort(latencies.begin(), latencies.end());
        std::cout << "Throughput:       " << static_cast<int>(receivedCount / totalSeconds) << " orders/sec\n";
        std::cout << "P50 Latency:      " << latencies[static_cast<size_t>(receivedCount * 0.50)] / 1000.0 << " us\n";
        std::cout << "P90 Latency:      " << latencies[static_cast<size_t>(receivedCount * 0.90)] / 1000.0 << " us\n";
        std::cout << "P95 Latency:      " << latencies[static_cast<size_t>(receivedCount * 0.95)] / 1000.0 << " us\n";
        std::cout << "P99 Latency:      " << latencies[static_cast<size_t>(receivedCount * 0.99)] / 1000.0 << " us\n";
        std::cout << "Max Latency:      " << latencies.back() / 1000.0 << " us\n";
    }
    std::cout << "=================================================\n";

    std::cout << "\n================ HFT METRICS ================\n";
    auto stats = exchange.getStatistics();
    std::cout << "Orders Received:  " << stats.totalOrdersReceived << "\n";
    std::cout << "Orders Rejected:  " << stats.totalOrdersRejected << "\n";
    std::cout << "Orders Accepted:  " << stats.totalOrdersReceived - stats.totalOrdersRejected << "\n";
    std::cout << "Trades Matched:   " << stats.totalTradesMatched << "\n";
    std::cout << "Volume Traded:    " << stats.totalVolumeTraded << "\n";
    std::cout << "Orders Cancelled: " << stats.totalOrdersCancelled << "\n";
    std::cout << "Engine Blocked:   " << exchange.getBackpressureBlockedCount() << "\n";
    std::cout << "Producer Blocked: " << producerBlockedCount << "\n";
    std::cout << "Map Resize Count: " << stats.resizeCount << "\n";
    std::cout << "Total Resize Time:" << stats.totalResizeTimeNs / 1000.0 << " us\n";
    std::cout << "Max Resize Time:  " << stats.maxResizeTimeNs / 1000.0 << " us\n";
    if (stats.totalOrdersReceived > 0) {
        std::cout << "Order-to-Trade Ratio: " << static_cast<double>(stats.totalOrdersReceived) / (stats.totalTradesMatched == 0 ? 1 : stats.totalTradesMatched) << ":1\n";
    }
    std::cout << "=============================================\n";

    return 0;
}
