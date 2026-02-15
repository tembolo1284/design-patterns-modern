#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>
#include <cstdio>
#include <functional>

// ============================================================
// Modern Command Pattern — Trade Management with Undo/Redo
//
// Same problem: Record, execute, undo, redo trade actions.
//
// Modern approach: Two techniques shown:
//   1. std::variant for a closed set of trade commands
//   2. Type-erased Command wrapper for open extension
//
// Both give value semantics — history is copyable/snapshottable.
// ============================================================

// --- Receiver: same Portfolio, but passed explicitly ---
class Portfolio {
    std::unordered_map<std::string, int> positions_;
    double cash_;

public:
    explicit Portfolio(double cash) : cash_(cash) {}

    void buy(const std::string& symbol, int qty, double price) {
        positions_[symbol] += qty;
        cash_ -= qty * price;
        std::printf("  [EXEC] BUY  %d %s @ $%.2f  (cash: $%.2f)\n",
                    qty, symbol.c_str(), price, cash_);
    }

    void sell(const std::string& symbol, int qty, double price) {
        positions_[symbol] -= qty;
        cash_ += qty * price;
        std::printf("  [EXEC] SELL %d %s @ $%.2f  (cash: $%.2f)\n",
                    qty, symbol.c_str(), price, cash_);
    }

    void reverse_buy(const std::string& symbol, int qty, double price) {
        positions_[symbol] -= qty;
        cash_ += qty * price;
        std::printf("  [UNDO] BUY  %d %s @ $%.2f reversed  (cash: $%.2f)\n",
                    qty, symbol.c_str(), price, cash_);
    }

    void reverse_sell(const std::string& symbol, int qty, double price) {
        positions_[symbol] += qty;
        cash_ -= qty * price;
        std::printf("  [UNDO] SELL %d %s @ $%.2f reversed  (cash: $%.2f)\n",
                    qty, symbol.c_str(), price, cash_);
    }

    void print_positions() const {
        std::puts("  Portfolio:");
        std::printf("    Cash: $%.2f\n", cash_);
        for (const auto& [sym, qty] : positions_) {
            if (qty != 0)
                std::printf("    %s: %d shares\n", sym.c_str(), qty);
        }
    }
};

// ============================================================
// APPROACH 1: std::variant commands (closed set)
// ============================================================

struct BuyTrade {
    std::string symbol;
    int quantity;
    double price;

    std::string description() const {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "BUY %d %s @ $%.2f",
                      quantity, symbol.c_str(), price);
        return buf;
    }
};

struct SellTrade {
    std::string symbol;
    int quantity;
    double price;

    std::string description() const {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "SELL %d %s @ $%.2f",
                      quantity, symbol.c_str(), price);
        return buf;
    }
};

using TradeAction = std::variant<BuyTrade, SellTrade>;

template <typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template <typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

void execute_action(const TradeAction& action, Portfolio& p) {
    std::visit(overloaded{
        [&](const BuyTrade& t)  { p.buy(t.symbol, t.quantity, t.price); },
        [&](const SellTrade& t) { p.sell(t.symbol, t.quantity, t.price); },
    }, action);
}

void undo_action(const TradeAction& action, Portfolio& p) {
    std::visit(overloaded{
        [&](const BuyTrade& t)  { p.reverse_buy(t.symbol, t.quantity, t.price); },
        [&](const SellTrade& t) { p.reverse_sell(t.symbol, t.quantity, t.price); },
    }, action);
}

std::string action_description(const TradeAction& action) {
    return std::visit([](const auto& t) { return t.description(); }, action);
}

class TradeHistory {
    std::vector<TradeAction> executed_;
    std::vector<TradeAction> undone_;

public:
    void execute(TradeAction action, Portfolio& p) {
        execute_action(action, p);
        executed_.push_back(std::move(action));
        undone_.clear();
    }

    bool undo(Portfolio& p) {
        if (executed_.empty()) return false;
        auto action = std::move(executed_.back());
        executed_.pop_back();
        undo_action(action, p);
        undone_.push_back(std::move(action));
        return true;
    }

    bool redo(Portfolio& p) {
        if (undone_.empty()) return false;
        auto action = std::move(undone_.back());
        undone_.pop_back();
        execute_action(action, p);
        executed_.push_back(std::move(action));
        return true;
    }

    void print_history() const {
        std::puts("  Trade History:");
        for (size_t i = 0; i < executed_.size(); ++i) {
            std::printf("    %zu. %s\n", i + 1,
                        action_description(executed_[i]).c_str());
        }
        if (executed_.empty()) std::puts("    (empty)");
    }

    size_t size() const { return executed_.size(); }
};

// ============================================================
// APPROACH 2: Type-erased Command (open extension)
// ============================================================

class Command {
    struct Concept {
        virtual ~Concept() = default;
        virtual void execute(Portfolio& p) const = 0;
        virtual void undo(Portfolio& p) const = 0;
        virtual std::string description() const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
    };

    template <typename T>
    struct Model final : Concept {
        T cmd;
        explicit Model(T c) : cmd(std::move(c)) {}
        void execute(Portfolio& p) const override { cmd.execute(p); }
        void undo(Portfolio& p) const override { cmd.undo(p); }
        std::string description() const override { return cmd.description(); }
        std::unique_ptr<Concept> clone() const override {
            return std::make_unique<Model>(cmd);
        }
    };

    std::unique_ptr<Concept> pimpl_;

public:
    template <typename T>
    Command(T cmd) : pimpl_(std::make_unique<Model<T>>(std::move(cmd))) {}

    Command(const Command& other) : pimpl_(other.pimpl_->clone()) {}
    Command& operator=(const Command& other) { pimpl_ = other.pimpl_->clone(); return *this; }
    Command(Command&&) noexcept = default;
    Command& operator=(Command&&) noexcept = default;

    void execute(Portfolio& p) const { pimpl_->execute(p); }
    void undo(Portfolio& p) const { pimpl_->undo(p); }
    std::string description() const { return pimpl_->description(); }
};

struct MarketBuy {
    std::string symbol;
    int quantity;
    double price;

    void execute(Portfolio& p) const { p.buy(symbol, quantity, price); }
    void undo(Portfolio& p) const { p.reverse_buy(symbol, quantity, price); }

    std::string description() const {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "MARKET BUY %d %s @ $%.2f",
                      quantity, symbol.c_str(), price);
        return buf;
    }
};

struct LimitSell {
    std::string symbol;
    int quantity;
    double limit_price;

    void execute(Portfolio& p) const { p.sell(symbol, quantity, limit_price); }
    void undo(Portfolio& p) const { p.reverse_sell(symbol, quantity, limit_price); }

    std::string description() const {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "LIMIT SELL %d %s @ $%.2f",
                      quantity, symbol.c_str(), limit_price);
        return buf;
    }
};

int main() {
    std::puts("=== Modern Command Pattern: Trade Management ===");
    std::puts("========== Part 1: std::variant approach ==========\n");

    Portfolio portfolio(1000000.0);
    TradeHistory history;

    std::puts("--- Executing trades ---");
    history.execute(BuyTrade{"AAPL", 100, 185.50}, portfolio);
    history.execute(BuyTrade{"GOOGL", 50, 140.25}, portfolio);
    history.execute(SellTrade{"MSFT", 75, 420.00}, portfolio);

    std::puts("");
    portfolio.print_positions();
    std::puts("");
    history.print_history();

    std::puts("\n--- Undo last trade ---");
    history.undo(portfolio);
    portfolio.print_positions();

    std::puts("\n--- Snapshot history ---");
    auto snapshot = history;
    std::printf("  Snapshot has %zu trades\n", snapshot.size());

    std::puts("\n--- Continue trading (original) ---");
    history.execute(SellTrade{"AAPL", 50, 190.00}, portfolio);
    history.print_history();

    std::puts("\n--- Snapshot is unchanged ---");
    snapshot.print_history();

    std::puts("\n--- Redo ---");
    history.redo(portfolio);
    portfolio.print_positions();

    // ============================================================
    std::puts("\n========== Part 2: Type-erased approach ==========\n");

    Portfolio portfolio2(500000.0);
    std::vector<Command> commands;

    commands.emplace_back(MarketBuy{"TSLA", 200, 175.00});
    commands.emplace_back(LimitSell{"NVDA", 30, 890.50});

    std::puts("--- Executing type-erased commands ---");
    for (const auto& cmd : commands) {
        cmd.execute(portfolio2);
    }

    std::puts("\n--- Undoing all ---");
    for (auto it = commands.rbegin(); it != commands.rend(); ++it) {
        it->undo(portfolio2);
    }

    portfolio2.print_positions();

    std::puts("\n--- Commands are copyable ---");
    auto commands_copy = commands;
    std::printf("  Original: %zu commands\n", commands.size());
    std::printf("  Copy:     %zu commands\n", commands_copy.size());

    return 0;
}#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>
#include <cstdio>
#include <functional>

// ============================================================
// Modern Command Pattern — Trade Management with Undo/Redo
//
// Same problem: Record, execute, undo, redo trade actions.
//
// Modern approach: Two techniques shown:
//   1. std::variant for a closed set of trade commands
//   2. Type-erased Command wrapper for open extension
//
// Both give value semantics — history is copyable/snapshottable.
// ============================================================

// --- Receiver: same Portfolio, but passed explicitly ---
class Portfolio {
    std::unordered_map<std::string, int> positions_;
    double cash_;

public:
    explicit Portfolio(double cash) : cash_(cash) {}

    void buy(const std::string& symbol, int qty, double price) {
        positions_[symbol] += qty;
        cash_ -= qty * price;
        std::printf("  [EXEC] BUY  %d %s @ $%.2f  (cash: $%.2f)\n",
                    qty, symbol.c_str(), price, cash_);
    }

    void sell(const std::string& symbol, int qty, double price) {
        positions_[symbol] -= qty;
        cash_ += qty * price;
        std::printf("  [EXEC] SELL %d %s @ $%.2f  (cash: $%.2f)\n",
                    qty, symbol.c_str(), price, cash_);
    }

    void reverse_buy(const std::string& symbol, int qty, double price) {
        positions_[symbol] -= qty;
        cash_ += qty * price;
        std::printf("  [UNDO] BUY  %d %s @ $%.2f reversed  (cash: $%.2f)\n",
                    qty, symbol.c_str(), price, cash_);
    }

    void reverse_sell(const std::string& symbol, int qty, double price) {
        positions_[symbol] += qty;
        cash_ -= qty * price;
        std::printf("  [UNDO] SELL %d %s @ $%.2f reversed  (cash: $%.2f)\n",
                    qty, symbol.c_str(), price, cash_);
    }

    void print_positions() const {
        std::puts("  Portfolio:");
        std::printf("    Cash: $%.2f\n", cash_);
        for (const auto& [sym, qty] : positions_) {
            if (qty != 0)
                std::printf("    %s: %d shares\n", sym.c_str(), qty);
        }
    }
};

// ============================================================
// APPROACH 1: std::variant commands (closed set)
// ============================================================

struct BuyTrade {
    std::string symbol;
    int quantity;
    double price;

    std::string description() const {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "BUY %d %s @ $%.2f",
                      quantity, symbol.c_str(), price);
        return buf;
    }
};

struct SellTrade {
    std::string symbol;
    int quantity;
    double price;

    std::string description() const {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "SELL %d %s @ $%.2f",
                      quantity, symbol.c_str(), price);
        return buf;
    }
};

using TradeAction = std::variant<BuyTrade, SellTrade>;

template <typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template <typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

void execute_action(const TradeAction& action, Portfolio& p) {
    std::visit(overloaded{
        [&](const BuyTrade& t)  { p.buy(t.symbol, t.quantity, t.price); },
        [&](const SellTrade& t) { p.sell(t.symbol, t.quantity, t.price); },
    }, action);
}

void undo_action(const TradeAction& action, Portfolio& p) {
    std::visit(overloaded{
        [&](const BuyTrade& t)  { p.reverse_buy(t.symbol, t.quantity, t.price); },
        [&](const SellTrade& t) { p.reverse_sell(t.symbol, t.quantity, t.price); },
    }, action);
}

std::string action_description(const TradeAction& action) {
    return std::visit([](const auto& t) { return t.description(); }, action);
}

class TradeHistory {
    std::vector<TradeAction> executed_;
    std::vector<TradeAction> undone_;

public:
    void execute(TradeAction action, Portfolio& p) {
        execute_action(action, p);
        executed_.push_back(std::move(action));
        undone_.clear();
    }

    bool undo(Portfolio& p) {
        if (executed_.empty()) return false;
        auto action = std::move(executed_.back());
        executed_.pop_back();
        undo_action(action, p);
        undone_.push_back(std::move(action));
        return true;
    }

    bool redo(Portfolio& p) {
        if (undone_.empty()) return false;
        auto action = std::move(undone_.back());
        undone_.pop_back();
        execute_action(action, p);
        executed_.push_back(std::move(action));
        return true;
    }

    void print_history() const {
        std::puts("  Trade History:");
        for (size_t i = 0; i < executed_.size(); ++i) {
            std::printf("    %zu. %s\n", i + 1,
                        action_description(executed_[i]).c_str());
        }
        if (executed_.empty()) std::puts("    (empty)");
    }

    size_t size() const { return executed_.size(); }
};

// ============================================================
// APPROACH 2: Type-erased Command (open extension)
// ============================================================

class Command {
    struct Concept {
        virtual ~Concept() = default;
        virtual void execute(Portfolio& p) const = 0;
        virtual void undo(Portfolio& p) const = 0;
        virtual std::string description() const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
    };

    template <typename T>
    struct Model final : Concept {
        T cmd;
        explicit Model(T c) : cmd(std::move(c)) {}
        void execute(Portfolio& p) const override { cmd.execute(p); }
        void undo(Portfolio& p) const override { cmd.undo(p); }
        std::string description() const override { return cmd.description(); }
        std::unique_ptr<Concept> clone() const override {
            return std::make_unique<Model>(cmd);
        }
    };

    std::unique_ptr<Concept> pimpl_;

public:
    template <typename T>
    Command(T cmd) : pimpl_(std::make_unique<Model<T>>(std::move(cmd))) {}

    Command(const Command& other) : pimpl_(other.pimpl_->clone()) {}
    Command& operator=(const Command& other) { pimpl_ = other.pimpl_->clone(); return *this; }
    Command(Command&&) noexcept = default;
    Command& operator=(Command&&) noexcept = default;

    void execute(Portfolio& p) const { pimpl_->execute(p); }
    void undo(Portfolio& p) const { pimpl_->undo(p); }
    std::string description() const { return pimpl_->description(); }
};

struct MarketBuy {
    std::string symbol;
    int quantity;
    double price;

    void execute(Portfolio& p) const { p.buy(symbol, quantity, price); }
    void undo(Portfolio& p) const { p.reverse_buy(symbol, quantity, price); }

    std::string description() const {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "MARKET BUY %d %s @ $%.2f",
                      quantity, symbol.c_str(), price);
        return buf;
    }
};

struct LimitSell {
    std::string symbol;
    int quantity;
    double limit_price;

    void execute(Portfolio& p) const { p.sell(symbol, quantity, limit_price); }
    void undo(Portfolio& p) const { p.reverse_sell(symbol, quantity, limit_price); }

    std::string description() const {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "LIMIT SELL %d %s @ $%.2f",
                      quantity, symbol.c_str(), limit_price);
        return buf;
    }
};

int main() {
    std::puts("=== Modern Command Pattern: Trade Management ===");
    std::puts("========== Part 1: std::variant approach ==========\n");

    Portfolio portfolio(1000000.0);
    TradeHistory history;

    std::puts("--- Executing trades ---");
    history.execute(BuyTrade{"AAPL", 100, 185.50}, portfolio);
    history.execute(BuyTrade{"GOOGL", 50, 140.25}, portfolio);
    history.execute(SellTrade{"MSFT", 75, 420.00}, portfolio);

    std::puts("");
    portfolio.print_positions();
    std::puts("");
    history.print_history();

    std::puts("\n--- Undo last trade ---");
    history.undo(portfolio);
    portfolio.print_positions();

    std::puts("\n--- Snapshot history ---");
    auto snapshot = history;
    std::printf("  Snapshot has %zu trades\n", snapshot.size());

    std::puts("\n--- Continue trading (original) ---");
    history.execute(SellTrade{"AAPL", 50, 190.00}, portfolio);
    history.print_history();

    std::puts("\n--- Snapshot is unchanged ---");
    snapshot.print_history();

    std::puts("\n--- Redo ---");
    history.redo(portfolio);
    portfolio.print_positions();

    // ============================================================
    std::puts("\n========== Part 2: Type-erased approach ==========\n");

    Portfolio portfolio2(500000.0);
    std::vector<Command> commands;

    commands.emplace_back(MarketBuy{"TSLA", 200, 175.00});
    commands.emplace_back(LimitSell{"NVDA", 30, 890.50});

    std::puts("--- Executing type-erased commands ---");
    for (const auto& cmd : commands) {
        cmd.execute(portfolio2);
    }

    std::puts("\n--- Undoing all ---");
    for (auto it = commands.rbegin(); it != commands.rend(); ++it) {
        it->undo(portfolio2);
    }

    portfolio2.print_positions();

    std::puts("\n--- Commands are copyable ---");
    auto commands_copy = commands;
    std::printf("  Original: %zu commands\n", commands.size());
    std::printf("  Copy:     %zu commands\n", commands_copy.size());

    return 0;
}
