#pragma once
#include "types.h"
#include <unordered_map>
#include <cstdint>

namespace exchange {

struct RiskConfig {
    Quantity maxOrderQuantity = 10000;
    Price maxOrderValue = 1000000;
    Quantity maxPosition = 50000;
};

struct Position {
    int64_t netQuantity = 0;
};

class RiskEngine {
public:
    explicit RiskEngine(const RiskConfig& config = RiskConfig{});

    bool validateOrder(OrderId id, InstrumentId inst, Price price, Quantity qty, Side side);

    void onTrade(OrderId buyerId, OrderId sellerId, InstrumentId inst, Price price, Quantity qty);

private:
    RiskConfig config_;
    
    
    std::unordered_map<uint32_t, Position> positions_;
};

}
