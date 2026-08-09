#pragma once
#include "Order.h"
#include "MemoryPool.h"
#include <vector>
#include <array>

namespace exchange {

struct PriceLevel {
    Order* head = nullptr;
    Order* tail = nullptr;
    Quantity totalVolume = 0;

    void addOrder(Order* order) {
        if (!head) {
            head = tail = order;
        } else {
            tail->next = order;
            order->prev = tail;
            tail = order;
        }
        totalVolume += order->visibleQuantity;
    }

    void removeOrder(Order* order) {
        totalVolume -= order->visibleQuantity;
        if (order->prev) {
            order->prev->next = order->next;
        } else {
            head = order->next;
        }
        if (order->next) {
            order->next->prev = order->prev;
        } else {
            tail = order->prev;
        }
        order->prev = nullptr;
        order->next = nullptr;
    }
};

class OrderBook {
public:
    explicit OrderBook(InstrumentId instrument, size_t maxOrders = 15000000);

    bool addOrder(OrderId id, Price price, Quantity quantity, Side side, OrderType type);
    void cancelOrder(OrderId id);
    
    Quantity getBestBidVolume() const;
    Quantity getBestAskVolume() const;

    Price getBestBid() const;
    Price getBestAsk() const;

    struct Statistics {
        uint64_t totalOrdersReceived = 0;
        uint64_t totalTradesMatched = 0;
        uint64_t totalVolumeTraded = 0;
        uint64_t totalOrdersCancelled = 0;
        uint64_t totalOrdersRejected = 0;
        uint64_t resizeCount = 0;
        uint64_t totalResizeTimeNs = 0;
        uint64_t maxResizeTimeNs = 0;
    };
    
    const Statistics& getStatistics() const { return stats_; }

private:
    void match(Order* incomingOrder);
    bool canFillCompletely(Order* incomingOrder) const;

    InstrumentId instrument_;
    MemoryPool<Order> orderPool_;
    Statistics stats_;

    std::vector<Order*> orderMap_;

    static constexpr size_t MAX_PRICE_LEVELS = 10000;
    std::array<PriceLevel, MAX_PRICE_LEVELS> bids_;
    std::array<PriceLevel, MAX_PRICE_LEVELS> asks_;
    
    Price bestBid_{0};
    Price bestAsk_{MAX_PRICE_LEVELS};
    
    void updateBestBid() {
        while (bestBid_ > 0 && bids_[bestBid_].head == nullptr) {
            bestBid_--;
        }
    }
    
    void updateBestAsk() {
        while (bestAsk_ < MAX_PRICE_LEVELS && asks_[bestAsk_].head == nullptr) {
            bestAsk_++;
        }
    }
};

}
