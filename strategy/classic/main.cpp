#include <iostream>
#include <memory>
#include <string>
#include <cstdio>

// ============================================================
// Classic Strategy Pattern — Order Execution Strategies
//
// Problem: Execute a trade using different algorithms (TWAP,
// VWAP, Iceberg) selected at runtime.
//
// Classic approach: Abstract base class + virtual dispatch.
// Client holds a pointer to the strategy interface.
// ============================================================

// --- Strategy Interface ---
class ExecutionStrategy {
public:
    virtual ~ExecutionStrategy() = default;
    virtual void execute(const std::string& symbol, int quantity, double price) const = 0;
    virtual std::string name() const = 0;
};

// --- Concrete Strategies ---
class TWAPStrategy : public ExecutionStrategy {
    int slices_;
public:
    explicit TWAPStrategy(int slices) : slices_(slices) {}

    void execute(const std::string& symbol, int quantity, double price) const override {
        int per_slice = quantity / slices_;
        std::printf("[TWAP] Executing %s: %d shares @ $%.2f across %d time slices (%d/slice)\n",
                    symbol.c_str(), quantity, price, slices_, per_slice);
    }

    std::string name() const override { return "TWAP"; }
};

class VWAPStrategy : public ExecutionStrategy {
    double participation_rate_;
public:
    explicit VWAPStrategy(double rate) : participation_rate_(rate) {}

    void execute(const std::string& symbol, int quantity, double price) const override {
        std::printf("[VWAP] Executing %s: %d shares @ $%.2f with %.0f%% participation rate\n",
                    symbol.c_str(), quantity, price, participation_rate_ * 100);
    }

    std::string name() const override { return "VWAP"; }
};

class IcebergStrategy : public ExecutionStrategy {
    int visible_qty_;
public:
    explicit IcebergStrategy(int visible) : visible_qty_(visible) {}

    void execute(const std::string& symbol, int quantity, double price) const override {
        std::printf("[Iceberg] Executing %s: %d shares @ $%.2f showing %d at a time\n",
                    symbol.c_str(), quantity, price, visible_qty_);
    }

    std::string name() const override { return "Iceberg"; }
};

// --- Context: Order that uses a strategy ---
class Order {
    std::string symbol_;
    int quantity_;
    double price_;
    std::unique_ptr<ExecutionStrategy> strategy_;  // <-- ownership via pointer
public:
    Order(std::string symbol, int qty, double price, std::unique_ptr<ExecutionStrategy> strategy)
        : symbol_(std::move(symbol)), quantity_(qty), price_(price),
          strategy_(std::move(strategy)) {}

    // Cannot copy — unique_ptr is move-only
    Order(const Order&) = delete;
    Order& operator=(const Order&) = delete;
    Order(Order&&) = default;
    Order& operator=(Order&&) = default;

    void set_strategy(std::unique_ptr<ExecutionStrategy> s) {
        strategy_ = std::move(s);
    }

    void send() const {
        std::printf("Order: %s %d shares @ $%.2f using %s\n",
                    symbol_.c_str(), quantity_, price_, strategy_->name().c_str());
        strategy_->execute(symbol_, quantity_, price_);
    }
};

int main() {
    std::puts("=== Classic Strategy Pattern: Order Execution ===\n");

    // Create order with TWAP
    Order order("AAPL", 10000, 185.50,
                std::make_unique<TWAPStrategy>(5));
    order.send();

    // Swap strategy at runtime to VWAP
    std::puts("\n--- Switching to VWAP ---");
    order.set_strategy(std::make_unique<VWAPStrategy>(0.15));
    order.send();

    // Swap to Iceberg
    std::puts("\n--- Switching to Iceberg ---");
    order.set_strategy(std::make_unique<IcebergStrategy>(500));
    order.send();

    // Note: Order is not copyable due to unique_ptr
    // Order order2 = order;  // COMPILE ERROR

    return 0;
}
