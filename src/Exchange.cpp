#include "Exchange.h"
#include <iostream>

namespace exchange {

Exchange::Exchange(size_t maxOrders)
    : gatewayToEngineQueue_(1024 * 1024),
      matchingToMarketDataQueue_(1024 * 1024),
      orderBook_(1, maxOrders)
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
