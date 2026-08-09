#pragma once
#include "types.h"
#include "OrderBook.h"
#include "RiskEngine.h"
#include "LockFreeQueue.h"
#include <thread>
#include <atomic>

namespace exchange {

class Exchange {
public:
    explicit Exchange(size_t maxOrders = 15000000, int engineCore = -1);
    ~Exchange();

    void start();
    void stop();

    bool sendOrder(const OrderRequest& request);

    SPSCQueue<MarketDataEvent>& getMarketDataQueue() { return matchingToMarketDataQueue_; }

    const OrderBook::Statistics& getStatistics() const { return orderBook_.getStatistics(); }
    uint64_t getBackpressureBlockedCount() const { return backpressureBlockedCount_; }

private:
    void engineThreadLoop();

    std::atomic<bool> running_{false};
    std::atomic<uint64_t> backpressureBlockedCount_{0};

    SPSCQueue<OrderRequest> gatewayToEngineQueue_;
    SPSCQueue<MarketDataEvent> matchingToMarketDataQueue_;

    RiskEngine riskEngine_;
    OrderBook orderBook_;

    std::thread engineThread_;
    int engineCore_;
};

}
