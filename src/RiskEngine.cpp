#include "RiskEngine.h"
#include <cmath>

namespace exchange {

RiskEngine::RiskEngine(const RiskConfig& config) : config_(config) {}

bool RiskEngine::validateOrder(OrderId id, InstrumentId inst, Price price, Quantity qty, Side side) {
    (void)id;
    (void)inst;
    
    if (qty > config_.maxOrderQuantity) {
        return false;
    }

    Price orderValue = price * qty;
    if (orderValue > config_.maxOrderValue) {
        return false;
    }


    return true;
}

void RiskEngine::onTrade(OrderId buyerId, OrderId sellerId, InstrumentId inst, Price price, Quantity qty) {
    (void)inst;
    (void)price;
    
    positions_[buyerId].netQuantity += qty;
    positions_[sellerId].netQuantity -= qty;
}

}
