#pragma once
#include "types.h"

namespace exchange {

struct Order {
    OrderId id;
    InstrumentId instrument;
    Price price;
    Quantity quantity;
    Quantity visibleQuantity;
    Quantity hiddenQuantity;
    Quantity peakSize;

    Side side;
    OrderType type;

    Order* prev;
    Order* next;

    void reset(OrderId oId, InstrumentId inst, Price p, Quantity q, Side s, OrderType t) {
        id = oId;
        instrument = inst;
        price = p;
        quantity = q;
        if (t == OrderType::ICEBERG) {
            peakSize = std::min(q, (Quantity)100); 
            visibleQuantity = peakSize;
            hiddenQuantity = q - peakSize;
        } else {
            peakSize = q;
            visibleQuantity = q;
            hiddenQuantity = 0;
        }
        side = s;
        type = t;
        prev = nullptr;
        next = nullptr;
    }
};

}
