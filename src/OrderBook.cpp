#include "OrderBook.h"
#include <algorithm>
#include <chrono>

namespace exchange {

OrderBook::OrderBook(InstrumentId instrument, size_t maxOrders) 
    : instrument_(instrument), orderPool_(maxOrders)
{
    orderMap_.resize(maxOrders + 100000, nullptr);
}

bool OrderBook::canFillCompletely(Order* incomingOrder) const {
    Quantity remaining = incomingOrder->quantity;
    if (incomingOrder->side == Side::BUY) {
        Price p = bestAsk_;
        while (p < MAX_PRICE_LEVELS && remaining > 0) {
            const auto& level = asks_[p];
            if (level.head != nullptr) {
                if (incomingOrder->type == OrderType::LIMIT && incomingOrder->price < p) break;
                if (level.totalVolume >= remaining) return true;
                remaining -= level.totalVolume;
            }
            p++;
        }
    } else {
        Price p = bestBid_;
        while (p > 0 && remaining > 0) {
            const auto& level = bids_[p];
            if (level.head != nullptr) {
                if (incomingOrder->type == OrderType::LIMIT && incomingOrder->price > p) break;
                if (level.totalVolume >= remaining) return true;
                remaining -= level.totalVolume;
            }
            p--;
        }
    }
    return remaining == 0;
}

bool OrderBook::addOrder(OrderId id, Price price, Quantity quantity, Side side, OrderType type) {
    stats_.totalOrdersReceived++;

    if (id < orderMap_.size() && orderMap_[id] != nullptr) {
        stats_.totalOrdersRejected++;
        return false;
    }

    Order* order = orderPool_.allocate(id, instrument_, price, quantity, side, type);
    if (!order) {
        stats_.totalOrdersRejected++;
        return false;
    }

    if (type == OrderType::FOK) {
        if (!canFillCompletely(order)) {
            orderPool_.deallocate(order);
            stats_.totalOrdersRejected++;
            return false;
        }
    }

    if (id >= orderMap_.size()) {
        auto resizeStart = std::chrono::high_resolution_clock::now();
        
        orderMap_.resize(id + 100000, nullptr);
        
        auto resizeEnd = std::chrono::high_resolution_clock::now();
        uint64_t resizeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(resizeEnd - resizeStart).count();
        
        stats_.resizeCount++;
        stats_.totalResizeTimeNs += resizeNs;
        if (resizeNs > stats_.maxResizeTimeNs) {
            stats_.maxResizeTimeNs = resizeNs;
        }
    }
    orderMap_[id] = order;

    match(order);

    if (order->quantity > 0 && type != OrderType::IOC && type != OrderType::FOK && type != OrderType::MARKET) {
        if (order->type == OrderType::ICEBERG) {
            order->visibleQuantity = std::min(order->quantity, order->peakSize);
            order->hiddenQuantity = order->quantity - order->visibleQuantity;
        } else {
            order->visibleQuantity = order->quantity;
        }
        
        if (side == Side::BUY) {
            bids_[price].addOrder(order);
            if (price > bestBid_) bestBid_ = price;
        } else {
            asks_[price].addOrder(order);
            if (price < bestAsk_) bestAsk_ = price;
        }
    } else if (order->quantity == 0 || type == OrderType::IOC || type == OrderType::FOK || type == OrderType::MARKET) {
        orderMap_[id] = nullptr;
        orderPool_.deallocate(order);
    }
    return true;
}

void OrderBook::cancelOrder(OrderId id) {
    if (id >= orderMap_.size() || orderMap_[id] == nullptr) return;

    stats_.totalOrdersCancelled++;
    Order* order = orderMap_[id];
    
    if (order->side == Side::BUY) {
        bids_[order->price].removeOrder(order);
        if (bids_[order->price].head == nullptr && order->price == bestBid_) {
            updateBestBid();
        }
    } else {
        asks_[order->price].removeOrder(order);
        if (asks_[order->price].head == nullptr && order->price == bestAsk_) {
            updateBestAsk();
        }
    }

    orderMap_[id] = nullptr;
    orderPool_.deallocate(order);
}

void OrderBook::match(Order* incomingOrder) {
    if (incomingOrder->side == Side::BUY) {
        while (incomingOrder->quantity > 0 && bestAsk_ < MAX_PRICE_LEVELS) {
            Price bestAskPrice = bestAsk_;
            PriceLevel& level = asks_[bestAskPrice];

            if (incomingOrder->type == OrderType::LIMIT && incomingOrder->price < bestAskPrice) {
                break;
            }

            Order* restingOrder = level.head;
            while (restingOrder && incomingOrder->quantity > 0) {
                Quantity tradeQuantity = std::min(incomingOrder->quantity, restingOrder->visibleQuantity);
                
                stats_.totalTradesMatched++;
                stats_.totalVolumeTraded += tradeQuantity;

                incomingOrder->quantity -= tradeQuantity;
                if (incomingOrder->visibleQuantity >= tradeQuantity) {
                    incomingOrder->visibleQuantity -= tradeQuantity;
                } else {
                    incomingOrder->visibleQuantity = 0;
                }
                restingOrder->quantity -= tradeQuantity;
                restingOrder->visibleQuantity -= tradeQuantity;
                level.totalVolume -= tradeQuantity;

                Order* nextOrder = restingOrder->next;

                if (restingOrder->quantity == 0) {
                    level.removeOrder(restingOrder);
                    orderMap_[restingOrder->id] = nullptr;
                    orderPool_.deallocate(restingOrder);
                } else if (restingOrder->visibleQuantity == 0 && restingOrder->hiddenQuantity > 0) {
                    Quantity replenishAmount = std::min(restingOrder->hiddenQuantity, restingOrder->peakSize);
                    restingOrder->hiddenQuantity -= replenishAmount;
                    
                    level.removeOrder(restingOrder);
                    restingOrder->visibleQuantity += replenishAmount;
                    level.addOrder(restingOrder);
                }

                restingOrder = nextOrder;
            }

            if (level.head == nullptr) {
                updateBestAsk();
            }
        }
    } else {
        while (incomingOrder->quantity > 0 && bestBid_ > 0) {
            Price bestBidPrice = bestBid_;
            PriceLevel& level = bids_[bestBidPrice];

            if (incomingOrder->type == OrderType::LIMIT && incomingOrder->price > bestBidPrice) {
                break;
            }

            Order* restingOrder = level.head;
            while (restingOrder && incomingOrder->quantity > 0) {
                Quantity tradeQuantity = std::min(incomingOrder->quantity, restingOrder->visibleQuantity);
                
                stats_.totalTradesMatched++;
                stats_.totalVolumeTraded += tradeQuantity;

                incomingOrder->quantity -= tradeQuantity;
                if (incomingOrder->visibleQuantity >= tradeQuantity) {
                    incomingOrder->visibleQuantity -= tradeQuantity;
                } else {
                    incomingOrder->visibleQuantity = 0;
                }
                restingOrder->quantity -= tradeQuantity;
                restingOrder->visibleQuantity -= tradeQuantity;
                level.totalVolume -= tradeQuantity;

                Order* nextOrder = restingOrder->next;

                if (restingOrder->quantity == 0) {
                    level.removeOrder(restingOrder);
                    orderMap_[restingOrder->id] = nullptr;
                    orderPool_.deallocate(restingOrder);
                } else if (restingOrder->visibleQuantity == 0 && restingOrder->hiddenQuantity > 0) {
                    Quantity replenishAmount = std::min(restingOrder->hiddenQuantity, restingOrder->peakSize);
                    restingOrder->hiddenQuantity -= replenishAmount;
                    
                    level.removeOrder(restingOrder);
                    restingOrder->visibleQuantity += replenishAmount;
                    level.addOrder(restingOrder);
                }

                restingOrder = nextOrder;
            }

            if (level.head == nullptr) {
                updateBestBid();
            }
        }
    }
}

Quantity OrderBook::getBestBidVolume() const {
    if (bestBid_ == 0) return 0;
    return bids_[bestBid_].totalVolume;
}

Quantity OrderBook::getBestAskVolume() const {
    if (bestAsk_ == MAX_PRICE_LEVELS) return 0;
    return asks_[bestAsk_].totalVolume;
}

Price OrderBook::getBestBid() const {
    return bestBid_;
}

Price OrderBook::getBestAsk() const {
    return bestAsk_ == MAX_PRICE_LEVELS ? 0 : bestAsk_;
}

}
