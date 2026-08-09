#pragma once
#include "types.h"
#include <cstdint>

namespace exchange {

struct RiskConfig {
    Quantity maxOrderQuantity = 10000;
    Price maxOrderValue = 1000000;
};

class RiskEngine {
public:
    explicit RiskEngine(const RiskConfig& config = RiskConfig{});

    bool validateOrder(OrderId id, InstrumentId inst, Price price, Quantity qty, Side side);

private:
    RiskConfig config_;
};

}
