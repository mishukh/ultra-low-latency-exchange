#include "RiskEngine.h"

namespace exchange {

RiskEngine::RiskEngine(const RiskConfig& config) : config_(config) {}

bool RiskEngine::validateOrder(OrderId id, InstrumentId inst, Price price, Quantity qty, Side side) {
    (void)id;
    (void)inst;
    (void)side;

    if (qty > config_.maxOrderQuantity) {
        return false;
    }

    Price orderValue = price * qty;
    if (orderValue > config_.maxOrderValue) {
        return false;
    }

    return true;
}

}
