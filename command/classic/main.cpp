#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdio>
#include <cassert>

// ============================================================
// Classic Command Pattern — Trade Management with Undo/Redo
//
// Problem: Record trade actions as objects so we can execute,
// undo, redo, and replay them. Support a full audit trail.
//
// Classic approach: Abstract Command base class with virtual
// execute() and undo(). CommandHistory manages a stack.
// ============================================================

// --- Receiver: Portfolio position tracker ---
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

    void cancel(const std::string& symbol, int qty, double price, bool was_buy) {
        if (was_buy) {
            positions_[symbol] -= qty;
            cash_ += qty * price;
        } else {
            positions_[symbol] += qty;
            cash_ -= qty * price;
        }
        std::printf("  [UNDO] %s %d %s @ $%.2f reversed  (cash: $%.2f)\n",
                    was_buy ? "BUY" : "SELL", qty, symbol.c_str(), price, cash_);
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

// --- Command Interface ---
class TradeCommand {
public:
    virtual ~TradeCommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual std::string description() const = 0;
};

// --- Concrete Commands ---
class BuyCommand : public TradeCommand {
    Portfolio& portfolio_;
    std::string symbol_;
    int quantity_;
    double price_;

public:
    BuyCommand(Portfolio& p, std::string sym, int qty, double price)
        : portfolio_(p), symbol_(std::move(sym)), quantity_(qty), price_(price) {}

    void execute() override {
        portfolio_.buy(symbol_, quantity_, price_);
    }

    void undo() override {
        portfolio_.cancel(symbol_, quantity_, price_, true);
    }

    std::string description() const override {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "BUY %d %s @ $%.2f",
                      quantity_, symbol_.c_str(), price_);
        return buf;
    }
};

class SellCommand : public TradeCommand {
    Portfolio& portfolio_;
    std::string symbol_;
    int quantity_;
    double price_;

public:
    SellCommand(Portfolio& p, std::string sym, int qty, double price)
        : portfolio_(p), symbol_(std::move(sym)), quantity_(qty), price_(price) {}

    void execute() override {
        portfolio_.sell(symbol_, quantity_, price_);
    }

    void undo() override {
        portfolio_.cancel(symbol_, quantity_, price_, false);
    }

    std::string description() const override {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "SELL %d %s @ $%.2f",
                      quantity_, symbol_.c_str(), price_);
        return buf;
    }
};

// --- Command History (Invoker) ---
class CommandHistory {
    std::vector<std::unique_ptr<TradeCommand>> executed_;
    std::vector<std::unique_ptr<TradeCommand>> undone_;

public:
    void execute(std::unique_ptr<TradeCommand> cmd) {
        cmd->execute();
        executed_.push_back(std::move(cmd));
        undone_.clear();
    }

    bool undo() {
        if (executed_.empty()) return false;
        auto cmd = std::move(executed_.back());
        executed_.pop_back();
        cmd->undo();
        undone_.push_back(std::move(cmd));
        return true;
    }

    bool redo() {
        if (undone_.empty()) return false;
        auto cmd = std::move(undone_.back());
        undone_.pop_back();
        cmd->execute();
        executed_.push_back(std::move(cmd));
        return true;
    }

    void print_history() const {
        std::puts("  Trade History:");
        for (size_t i = 0; i < executed_.size(); ++i) {
            std::printf("    %zu. %s\n", i + 1, executed_[i]->description().c_str());
        }
        if (executed_.empty()) std::puts("    (empty)");
    }

    CommandHistory() = default;
    CommandHistory(const CommandHistory&) = delete;
    CommandHistory& operator=(const CommandHistory&) = delete;
};

int main() {
    std::puts("=== Classic Command Pattern: Trade Management ===\n");

    Portfolio portfolio(1000000.0);
    CommandHistory history;

    std::puts("--- Executing trades ---");
    history.execute(std::make_unique<BuyCommand>(portfolio, "AAPL", 100, 185.50));
    history.execute(std::make_unique<BuyCommand>(portfolio, "GOOGL", 50, 140.25));
    history.execute(std::make_unique<SellCommand>(portfolio, "MSFT", 75, 420.00));

    std::puts("");
    portfolio.print_positions();
    std::puts("");
    history.print_history();

    std::puts("\n--- Undo last trade ---");
    history.undo();
    portfolio.print_positions();

    std::puts("\n--- Undo another ---");
    history.undo();
    portfolio.print_positions();

    std::puts("\n--- Redo ---");
    history.redo();
    portfolio.print_positions();

    std::puts("");
    history.print_history();

    return 0;
}
