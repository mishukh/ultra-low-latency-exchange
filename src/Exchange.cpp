#include "Exchange.h"
#include <iostream>
#if defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>
#include <sched.h>
#endif

namespace exchange {

Exchange::Exchange(size_t maxOrders, int engineCore)
    : gatewayToEngineQueue_(1024 * 1024),
      matchingToMarketDataQueue_(1024 * 1024),
      orderBook_(1, maxOrders),
      engineCore_(engineCore)
{
}

Exchange::~Exchange() {
    stop();
}

void Exchange::start() {
    if (running_) return;
    running_ = true;

    engineThread_ = std::thread(&Exchange::engineThreadLoop, this);
}

void Exchange::stop() {
    if (!running_) return;
    running_ = false;

    if (engineThread_.joinable()) engineThread_.join();
}

bool Exchange::sendOrder(const OrderRequest& request) {
    if (!running_) return false;
    return gatewayToEngineQueue_.push(request);
}

void Exchange::engineThreadLoop() {
#if defined(__linux__)
    if (engineCore_ >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(engineCore_, &cpuset);
        int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            std::cerr << "Warning: Failed to set thread affinity for engine thread to core " << engineCore_ << "\n";
        }
        // Note: WSL2 doesn't support isolcpus, so this pinning reduces
        // but doesn't eliminate scheduler contention with the Windows host.
    }
#endif

    OrderRequest req;
    while (running_) {
        if (gatewayToEngineQueue_.pop(req)) {
            if (riskEngine_.validateOrder(req.id, req.instrument, req.price, req.quantity, req.side)) {
                bool accepted = orderBook_.addOrder(req.id, req.price, req.quantity, req.side, req.type);
                
                MarketDataEvent mdEvent;
                mdEvent.instrument = req.instrument;
                mdEvent.sentTime = req.sentTime;
                
                if (accepted) {
                    mdEvent.type = MarketDataType::BBO_UPDATE;
                    mdEvent.side = req.side;
                    if (req.side == Side::BUY) {
                        mdEvent.price = orderBook_.getBestBid();
                        mdEvent.quantity = orderBook_.getBestBidVolume();
                    } else {
                        mdEvent.price = orderBook_.getBestAsk();
                        mdEvent.quantity = orderBook_.getBestAskVolume();
                    }
                } else {
                    mdEvent.type = MarketDataType::ORDER_REJECT;
                    mdEvent.price = req.price;
                    mdEvent.quantity = req.quantity;
                    mdEvent.side = req.side;
                }
                
                bool wasBlocked = false;
                while (running_ && !matchingToMarketDataQueue_.push(mdEvent)) {
                    if (!wasBlocked) {
                        backpressureBlockedCount_++;
                        wasBlocked = true;
                    }
                }
            }
        }
    }
}

}
