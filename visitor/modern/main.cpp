#include <iostream>
#include <string>
#include <variant>
#include <vector>
#include <cmath>
#include <cstdio>
#include <algorithm>

// ============================================================
// Modern Visitor Pattern — Financial Instrument Operations
//
// Same problem: Perform different operations on instrument types.
//
// Modern approach: std::variant + std::visit.
// No base class, no accept(), no double dispatch.
// Instruments are plain value types. The portfolio is copyable.
// ============================================================

// --- Instrument types: plain structs, no base class ---

struct Bond {
    std::string issuer;
    double face_value;
    double coupon_rate;
    int maturity_years;

    std::string description() const {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Bond(%s, %.0f face, %.1f%% coupon, %dY)",
                      issuer.c_str(), face_value, coupon_rate * 100, maturity_years);
        return buf;
    }
};

struct Swap {
    double notional;
    double fixed_rate;
    int tenor_years;

    std::string description() const {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "IRS(%.0f notional, %.2f%% fixed, %dY)",
                      notional, fixed_rate * 100, tenor_years);
        return buf;
    }
};

struct Option {
    std::string underlying;
    double strike;
    double spot;
    bool is_call;

    std::string description() const {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s %s(K=%.2f, S=%.2f)",
                      underlying.c_str(), is_call ? "Call" : "Put",
                      strike, spot);
        return buf;
    }
};

// --- The variant IS the polymorphic type ---
using Instrument = std::variant<Bond, Swap, Option>;

// --- Operations as free functions or function objects ---

// Pricing
struct PriceCalculator {
    double operator()(const Bond& b) const {
        double pv = 0;
        for (int i = 1; i <= b.maturity_years; ++i) {
            pv += (b.face_value * b.coupon_rate) / std::pow(1.05, i);
        }
        pv += b.face_value / std::pow(1.05, b.maturity_years);
        return pv;
    }

    double operator()(const Swap& s) const {
        double market_rate = 0.04;
        return s.notional * (s.fixed_rate - market_rate) * s.tenor_years;
    }

    double operator()(const Option& o) const {
        return o.is_call
            ? std::max(0.0, o.spot - o.strike)
            : std::max(0.0, o.strike - o.spot);
    }
};

// Overloaded lambdas pattern
template <typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template <typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

auto make_risk_visitor() {
    return overloaded{
        [](const Bond& b) {
            double duration = b.maturity_years * 0.9;
            std::printf("  Risk  %-40s   duration=%.1f, DV01=$%.2f\n",
                        b.description().c_str(), duration,
                        b.face_value * duration * 0.0001);
        },
        [](const Swap& s) {
            double dv01 = s.notional * s.tenor_years * 0.0001;
            std::printf("  Risk  %-40s   DV01=$%.2f\n",
                        s.description().c_str(), dv01);
        },
        [](const Option& o) {
            double delta = o.is_call ? 0.55 : -0.45;
            std::printf("  Risk  %-40s   delta=%.2f\n",
                        o.description().c_str(), delta);
        }
    };
}

auto make_regulatory_visitor() {
    return overloaded{
        [](const Bond& b) {
            double charge = b.face_value * 0.08;
            std::printf("  Reg   %-40s   capital charge=$%.2f\n",
                        b.description().c_str(), charge);
        },
        [](const Swap& s) {
            double charge = s.notional * 0.05 * s.tenor_years;
            std::printf("  Reg   %-40s   capital charge=$%.2f\n",
                        s.description().c_str(), charge);
        },
        [](const Option& o) {
            double charge = o.spot * 100 * 0.10;
            std::printf("  Reg   %-40s   capital charge=$%.2f\n",
                        o.description().c_str(), charge);
        }
    };
}

int main() {
    std::puts("=== Modern Visitor Pattern: Financial Instruments ===\n");

    std::vector<Instrument> portfolio = {
        Bond{"US-TREASURY", 1000000, 0.045, 10},
        Swap{5000000, 0.0375, 5},
        Option{"SPX", 4500.0, 4550.0, true},
        Option{"AAPL", 190.0, 185.0, false},
    };

    // --- Pricing (using a callable struct) ---
    std::puts("--- Pricing ---");
    PriceCalculator pricer;
    for (const auto& inst : portfolio) {
        double px = std::visit(pricer, inst);
        auto desc = std::visit([](const auto& i) { return i.description(); }, inst);
        std::printf("  Price %-40s = $%.2f\n", desc.c_str(), px);
    }

    // --- Risk (using overloaded lambdas) ---
    std::puts("\n--- Risk ---");
    auto risk = make_risk_visitor();
    for (const auto& inst : portfolio) std::visit(risk, inst);

    // --- Regulatory ---
    std::puts("\n--- Regulatory ---");
    auto reg = make_regulatory_visitor();
    for (const auto& inst : portfolio) std::visit(reg, inst);

    // --- Portfolio is copyable! ---
    std::puts("\n--- Copying portfolio ---");
    auto portfolio2 = portfolio;
    portfolio2.push_back(Bond{"UK-GILT", 500000, 0.04, 5});
    std::printf("  Original size: %zu\n", portfolio.size());
    std::printf("  Copy size:     %zu\n", portfolio2.size());

    return 0;
}
