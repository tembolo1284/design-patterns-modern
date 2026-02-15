#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cstdio>
#include <cmath>

// ============================================================
// Classic Visitor Pattern — Financial Instrument Operations
//
// Problem: Perform different operations (pricing, risk calc,
// regulatory reporting) on a set of instrument types without
// modifying the instrument classes.
//
// Classic approach: Double dispatch. Each instrument accepts
// a visitor, which has a visit() overload per instrument type.
// ============================================================

// Forward declarations for visitor interface
class Bond;
class Swap;
class Option;

// --- Visitor Interface ---
class InstrumentVisitor {
public:
    virtual ~InstrumentVisitor() = default;
    virtual void visit(const Bond& b) = 0;
    virtual void visit(const Swap& s) = 0;
    virtual void visit(const Option& o) = 0;
};

// --- Element Interface ---
class Instrument {
public:
    virtual ~Instrument() = default;
    virtual void accept(InstrumentVisitor& v) const = 0;
    virtual std::string description() const = 0;
};

// --- Concrete Elements ---
class Bond : public Instrument {
    std::string issuer_;
    double face_value_;
    double coupon_rate_;
    int maturity_years_;

public:
    Bond(std::string issuer, double face, double coupon, int years)
        : issuer_(std::move(issuer)), face_value_(face),
          coupon_rate_(coupon), maturity_years_(years) {}

    void accept(InstrumentVisitor& v) const override { v.visit(*this); }

    std::string description() const override {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Bond(%s, %.0f face, %.1f%% coupon, %dY)",
                      issuer_.c_str(), face_value_, coupon_rate_ * 100, maturity_years_);
        return buf;
    }

    const std::string& issuer() const { return issuer_; }
    double face_value() const { return face_value_; }
    double coupon_rate() const { return coupon_rate_; }
    int maturity_years() const { return maturity_years_; }
};

class Swap : public Instrument {
    double notional_;
    double fixed_rate_;
    int tenor_years_;

public:
    Swap(double notional, double fixed_rate, int tenor)
        : notional_(notional), fixed_rate_(fixed_rate), tenor_years_(tenor) {}

    void accept(InstrumentVisitor& v) const override { v.visit(*this); }

    std::string description() const override {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "IRS(%.0f notional, %.2f%% fixed, %dY)",
                      notional_, fixed_rate_ * 100, tenor_years_);
        return buf;
    }

    double notional() const { return notional_; }
    double fixed_rate() const { return fixed_rate_; }
    int tenor_years() const { return tenor_years_; }
};

class Option : public Instrument {
    std::string underlying_;
    double strike_;
    double spot_;
    bool is_call_;

public:
    Option(std::string underlying, double strike, double spot, bool is_call)
        : underlying_(std::move(underlying)), strike_(strike),
          spot_(spot), is_call_(is_call) {}

    void accept(InstrumentVisitor& v) const override { v.visit(*this); }

    std::string description() const override {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s %s(K=%.2f, S=%.2f)",
                      underlying_.c_str(), is_call_ ? "Call" : "Put",
                      strike_, spot_);
        return buf;
    }

    const std::string& underlying() const { return underlying_; }
    double strike() const { return strike_; }
    double spot() const { return spot_; }
    bool is_call() const { return is_call_; }
};

// --- Concrete Visitors (operations) ---

class PricingVisitor : public InstrumentVisitor {
public:
    void visit(const Bond& b) override {
        double pv = 0;
        for (int i = 1; i <= b.maturity_years(); ++i) {
            pv += (b.face_value() * b.coupon_rate()) / std::pow(1.05, i);
        }
        pv += b.face_value() / std::pow(1.05, b.maturity_years());
        std::printf("  Price %-40s = $%.2f\n", b.description().c_str(), pv);
    }

    void visit(const Swap& s) override {
        double market_rate = 0.04;
        double npv = s.notional() * (s.fixed_rate() - market_rate) * s.tenor_years();
        std::printf("  Price %-40s = $%.2f NPV\n", s.description().c_str(), npv);
    }

    void visit(const Option& o) override {
        double intrinsic = o.is_call()
            ? fmax(0.0, o.spot() - o.strike())
            : fmax(0.0, o.strike() - o.spot());
        std::printf("  Price %-40s = $%.2f intrinsic\n", o.description().c_str(), intrinsic);
    }
};

class RiskVisitor : public InstrumentVisitor {
public:
    void visit(const Bond& b) override {
        double duration = b.maturity_years() * 0.9;
        std::printf("  Risk  %-40s   duration=%.1f, DV01=$%.2f\n",
                    b.description().c_str(), duration,
                    b.face_value() * duration * 0.0001);
    }

    void visit(const Swap& s) override {
        double dv01 = s.notional() * s.tenor_years() * 0.0001;
        std::printf("  Risk  %-40s   DV01=$%.2f\n",
                    s.description().c_str(), dv01);
    }

    void visit(const Option& o) override {
        double delta = o.is_call() ? 0.55 : -0.45;
        std::printf("  Risk  %-40s   delta=%.2f\n",
                    o.description().c_str(), delta);
    }
};

class RegulatoryVisitor : public InstrumentVisitor {
public:
    void visit(const Bond& b) override {
        double charge = b.face_value() * 0.08;
        std::printf("  Reg   %-40s   capital charge=$%.2f\n",
                    b.description().c_str(), charge);
    }

    void visit(const Swap& s) override {
        double charge = s.notional() * 0.05 * s.tenor_years();
        std::printf("  Reg   %-40s   capital charge=$%.2f\n",
                    s.description().c_str(), charge);
    }

    void visit(const Option& o) override {
        double charge = o.spot() * 100 * 0.10;
        std::printf("  Reg   %-40s   capital charge=$%.2f\n",
                    o.description().c_str(), charge);
    }
};

int main() {
    std::puts("=== Classic Visitor Pattern: Financial Instruments ===\n");

    std::vector<std::unique_ptr<Instrument>> portfolio;
    portfolio.push_back(std::make_unique<Bond>("US-TREASURY", 1000000, 0.045, 10));
    portfolio.push_back(std::make_unique<Swap>(5000000, 0.0375, 5));
    portfolio.push_back(std::make_unique<Option>("SPX", 4500.0, 4550.0, true));
    portfolio.push_back(std::make_unique<Option>("AAPL", 190.0, 185.0, false));

    PricingVisitor pricer;
    RiskVisitor risk;
    RegulatoryVisitor reg;

    std::puts("--- Pricing ---");
    for (const auto& inst : portfolio) inst->accept(pricer);

    std::puts("\n--- Risk ---");
    for (const auto& inst : portfolio) inst->accept(risk);

    std::puts("\n--- Regulatory ---");
    for (const auto& inst : portfolio) inst->accept(reg);

    return 0;
}
