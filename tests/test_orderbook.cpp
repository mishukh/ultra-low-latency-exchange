#include <gtest/gtest.h>
#include "OrderBook.h"

using namespace exchange;

TEST(OrderBookTest, BasicInsertAndBestPrices) {
    OrderBook book(1);
    
    book.addOrder(1, 100, 10, Side::BUY, OrderType::LIMIT);
    EXPECT_EQ(book.getBestBid(), 100);
    EXPECT_EQ(book.getBestBidVolume(), 10);
    
    book.addOrder(2, 101, 15, Side::BUY, OrderType::LIMIT);
    EXPECT_EQ(book.getBestBid(), 101);
    EXPECT_EQ(book.getBestBidVolume(), 15);
    
    book.addOrder(3, 102, 5, Side::SELL, OrderType::LIMIT);
    EXPECT_EQ(book.getBestAsk(), 102);
    EXPECT_EQ(book.getBestAskVolume(), 5);
}

TEST(OrderBookTest, BasicMatching) {
    OrderBook book(1);
    
    book.addOrder(1, 100, 10, Side::BUY, OrderType::LIMIT);
    book.addOrder(2, 101, 20, Side::BUY, OrderType::LIMIT);
    
    book.addOrder(3, 100, 15, Side::SELL, OrderType::LIMIT);
    
    EXPECT_EQ(book.getBestBid(), 101); 
    EXPECT_EQ(book.getBestBidVolume(), 5);
}

TEST(OrderBookTest, CancelOrder) {
    OrderBook book(1);
    book.addOrder(1, 100, 10, Side::BUY, OrderType::LIMIT);
    book.addOrder(2, 100, 20, Side::BUY, OrderType::LIMIT);
    
    EXPECT_EQ(book.getBestBidVolume(), 30);
    
    book.cancelOrder(1);
    
    EXPECT_EQ(book.getBestBidVolume(), 20);
    
    book.cancelOrder(2);
    
    EXPECT_EQ(book.getBestBid(), 0);
    EXPECT_EQ(book.getBestBidVolume(), 0);
}

TEST(OrderBookTest, AdvancedOrderTypes_IOC) {
    OrderBook book(1);
    book.addOrder(1, 100, 10, Side::BUY, OrderType::LIMIT);
    
    // IOC Sell for 15. Should match 10 and cancel 5.
    book.addOrder(2, 100, 15, Side::SELL, OrderType::IOC);
    
    EXPECT_EQ(book.getBestBid(), 0); // Book is empty
    EXPECT_EQ(book.getBestBidVolume(), 0);
    EXPECT_EQ(book.getBestAsk(), 0); // IOC should not rest on book
}

TEST(OrderBookTest, AdvancedOrderTypes_FOK) {
    OrderBook book(1);
    book.addOrder(1, 100, 10, Side::BUY, OrderType::LIMIT);
    
    // FOK Sell for 15. Cannot be fully filled, so it should be killed entirely.
    book.addOrder(2, 100, 15, Side::SELL, OrderType::FOK);
    
    // Bid should still be there, untouched
    EXPECT_EQ(book.getBestBidVolume(), 10);
    
    // FOK Sell for 5. Can be fully filled.
    book.addOrder(3, 100, 5, Side::SELL, OrderType::FOK);
    EXPECT_EQ(book.getBestBidVolume(), 5);
}

TEST(OrderBookTest, AdvancedOrderTypes_Iceberg) {
    OrderBook book(1);
    
    // Iceberg order for 500, with peak 100 (based on 100 max peak logic we added)
    book.addOrder(1, 100, 500, Side::BUY, OrderType::ICEBERG);
    
    // Only 100 should be visible initially.
    EXPECT_EQ(book.getBestBidVolume(), 100);
    
    // Sell 50. Visible volume should drop to 50.
    book.addOrder(2, 100, 50, Side::SELL, OrderType::LIMIT);
    EXPECT_EQ(book.getBestBidVolume(), 50);
    
    // Sell another 50. This hits 0, causing replenishment of next 100.
    book.addOrder(3, 100, 50, Side::SELL, OrderType::LIMIT);
    EXPECT_EQ(book.getBestBidVolume(), 100);
    
    // Sell 350 to drain the rest of the 500 total (we've filled 100, so 400 left. 350 leaves 50).
    book.addOrder(4, 100, 350, Side::SELL, OrderType::LIMIT);
    EXPECT_EQ(book.getBestBidVolume(), 50);
}
