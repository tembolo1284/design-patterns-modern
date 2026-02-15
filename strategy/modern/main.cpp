#include <iostream>
#include <memory>
#include <string>
#include <cstdio>
#include <utility>

// ============================================================
// Modern Strategy Pattern — Order Execution Strategies
//
// Same problem: Execute a trade using different algorithms
// selected at runtime.
//
// Modern approach: Type erasure. The strategy is any object
// that has execute() and name() — no base class required.
// Order is a value type (copyable, no raw pointers).
// ============================================================

// --- Type-Erased Strategy ---
class ExecutionStrategy {
    // Internal concept (hidden from users)
    struct Concept {
        virtual ~Concept() = default;
        virtual void execute(const std::string& symbol, int qty, double price) const = 0;
        virtual std::string name() const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
    };

    // Model wraps any type that satisfies the interface
    template <typename T>
    struct Model final : Concept {
        T strategy;

        explicit Model(T s) : strategy(std::move(s)) {}

        void execute(const std::string& symbol, int qty, double price) const override {
            strategy.execute(symbol, qty, price);
        }

        std::string name() const override {
            return strategy.name();
        }

        std::unique_ptr<Concept> clone() const override {
            return std::make_unique<Model>(strategy);
        }
    };

    std::unique_ptr<Concept> pimpl_;

public:
    // Construct from any type with execute() and name()
    template <typename T>
    ExecutionStrategy(T strategy)
        : pimpl_(std::make_unique<Model<T>>(std::move(strategy))) {}

    // Value semantics: deep copy
    ExecutionStrategy(const ExecutionStrategy& other)
        : pimpl_(other.pimpl_->clone()) {}

    ExecutionStrategy& operator=(const ExecutionStrategy& other) {
        pimpl_ = other.pimpl_->clone();
        return *this;
    }

    ExecutionStrategy(ExecutionStrategy&&) noexcept = default;
    ExecutionStrategy& operator=(ExecutionStrategy&&) noexcept = default;

    // Forwarding interface
    void execute(const std::string& symbol, int qty, double price) const {
        pimpl_->execute(symbol, qty, price);
    }

    std::string name() const { return pimpl_->name(); }
};

// --- Concrete Strategies (plain structs, no inheritance!) ---

struct TWAPStrategy {
    int slices;

    void execute(const std::string& symbol, int qty, double price) const {
        int per_slice = qty / slices;
        std::printf("[TWAP] Executing %s: %d shares @ $%.2f across %d time slices (%d/slice)\n",
                    symbol.c_str(), qty, price, slices, per_slice);
    }

    std::string name() const { return "TWAP"; }
};

struct VWAPStrategy {
    double participation_rate;

    void execute(const std::string& symbol, int qty, double price) const {
        std::printf("[VWAP] Executing %s: %d shares @ $%.2f with %.0f%% participation rate\n",
                    symbol.c_str(), qty, price, participation_rate * 100);
    }

    std::string name() const { return "VWAP"; }
};

struct IcebergStrategy {
    int visible_qty;

    void execute(const std::string& symbol, int qty, double price) const {
        std::printf("[Iceberg] Executing %s: %d shares @ $%.2f showing %d at a time\n",
                    symbol.c_str(), qty, price, visible_qty);
    }

    std::string name() const { return "Iceberg"; }
};

// --- Context: Order is now a VALUE TYPE ---
class Order {
    std::string symbol_;
    int quantity_;
    double price_;
    ExecutionStrategy strategy_;  // <-- value, not pointer!

public:
    Order(std::string symbol, int qty, double price, ExecutionStrategy strategy)
        : symbol_(std::move(symbol)), quantity_(qty), price_(price),
          strategy_(std::move(strategy)) {}

    // Copyable! Deep copies the type-erased strategy.
    Order(const Order&) = default;
    Order& operator=(const Order&) = default;
    Order(Order&&) = default;
    Order& operator=(Order&&) = default;

    void set_strategy(ExecutionStrategy s) {
        strategy_ = std::move(s);
    }

    void send() const {
        std::printf("Order: %s %d shares @ $%.2f using %s\n",
                    symbol_.c_str(), quantity_, price_, strategy_.name().c_str());
        strategy_.execute(symbol_, quantity_, price_);
    }
};

int main() {
    std::puts("=== Modern Strategy Pattern: Order Execution ===\n");

    // Create order with TWAP — just pass a plain struct
    Order order("AAPL", 10000, 185.50, TWAPStrategy{5});
    order.send();

    // Swap strategy at runtime — no make_unique needed
    std::puts("\n--- Switching to VWAP ---");
    order.set_strategy(VWAPStrategy{0.15});
    order.send();

    // Swap to Iceberg
    std::puts("\n--- Switching to Iceberg ---");
    order.set_strategy(IcebergStrategy{500});
    order.send();

    // NOW we can copy orders — value semantics!
    std::puts("\n--- Copying order ---");
    Order order2 = order;
    order2.set_strategy(TWAPStrategy{10});

    std::puts("Original:");
    order.send();
    std::puts("Copy (independent):");
    order2.send();

    return 0;
}

