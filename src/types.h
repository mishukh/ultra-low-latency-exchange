#pragma once
#include <cstdint>
#include <limits>
#include <string>

namespace exchange {

using OrderId = uint64_t;
using Price = uint64_t;
using Quantity = uint32_t;
using InstrumentId = uint32_t;

enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

enum class OrderType : uint8_t {
    LIMIT = 0,
    MARKET = 1,
    IOC = 2,
    FOK = 3,
    STOP = 4,
    ICEBERG = 5
};

constexpr Price MAX_PRICE = std::numeric_limits<Price>::max();
constexpr Price MIN_PRICE = 0;

struct OrderRequest {
    OrderId id;
    InstrumentId instrument;
    Price price;
    Quantity quantity;
    Side side;
    OrderType type;
    uint64_t sentTime;
};

enum class MarketDataType : uint8_t {
    TRADE,
    BBO_UPDATE,
    ORDER_REJECT
};

struct MarketDataEvent {
    MarketDataType type;
    InstrumentId instrument;
    Price price;
    Quantity quantity; 
    Side side;         
    uint64_t sentTime;
};

}
