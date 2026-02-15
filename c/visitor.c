#include <stdio.h>
#include <string.h>
#include <math.h>

/* ============================================================
 * Visitor Pattern in Pure C — Financial Instrument Operations
 *
 * Tagged union approach (C's std::variant):
 *   - Instruments are a tagged union of plain data structs
 *   - "Visitors" are just functions that switch on the tag
 *   - No double dispatch, no accept(), no base class
 *   - Portfolio is an array of values — fully copyable
 *
 * This is arguably cleaner than both classic AND modern C++
 * for a closed set of types. The switch statement IS the visit.
 * ============================================================ */

/* --- Instrument types: plain data --- */

typedef struct {
    char issuer[32];
    double face_value;
    double coupon_rate;
    int maturity_years;
} Bond;

typedef struct {
    double notional;
    double fixed_rate;
    int tenor_years;
} Swap;

typedef struct {
    char underlying[16];
    double strike;
    double spot;
    int is_call;  /* 1 = call, 0 = put */
} Option;

/* --- Tagged union --- */

typedef enum {
    INST_BOND,
    INST_SWAP,
    INST_OPTION
} InstrumentType;

typedef struct {
    InstrumentType type;
    union {
        Bond bond;
        Swap swap;
        Option option;
    } data;
} Instrument;

/* "Constructors" — return by value */
static Instrument make_bond(const char *issuer, double face,
                             double coupon, int years) {
    Instrument inst;
    inst.type = INST_BOND;
    strncpy(inst.data.bond.issuer, issuer, sizeof(inst.data.bond.issuer) - 1);
    inst.data.bond.issuer[sizeof(inst.data.bond.issuer) - 1] = '\0';
    inst.data.bond.face_value = face;
    inst.data.bond.coupon_rate = coupon;
    inst.data.bond.maturity_years = years;
    return inst;
}

static Instrument make_swap(double notional, double fixed_rate, int tenor) {
    Instrument inst;
    inst.type = INST_SWAP;
    inst.data.swap.notional = notional;
    inst.data.swap.fixed_rate = fixed_rate;
    inst.data.swap.tenor_years = tenor;
    return inst;
}

static Instrument make_option(const char *underlying, double strike,
                               double spot, int is_call) {
    Instrument inst;
    inst.type = INST_OPTION;
    strncpy(inst.data.option.underlying, underlying,
            sizeof(inst.data.option.underlying) - 1);
    inst.data.option.underlying[sizeof(inst.data.option.underlying) - 1] = '\0';
    inst.data.option.strike = strike;
    inst.data.option.spot = spot;
    inst.data.option.is_call = is_call;
    return inst;
}

/* Description helper */
static void instrument_describe(const Instrument *inst, char *buf, size_t len) {
    switch (inst->type) {
    case INST_BOND: {
        const Bond *b = &inst->data.bond;
        snprintf(buf, len, "Bond(%s, %.0f face, %.1f%% coupon, %dY)",
                 b->issuer, b->face_value, b->coupon_rate * 100, b->maturity_years);
        break;
    }
    case INST_SWAP: {
        const Swap *s = &inst->data.swap;
        snprintf(buf, len, "IRS(%.0f notional, %.2f%% fixed, %dY)",
                 s->notional, s->fixed_rate * 100, s->tenor_years);
        break;
    }
    case INST_OPTION: {
        const Option *o = &inst->data.option;
        snprintf(buf, len, "%s %s(K=%.2f, S=%.2f)",
                 o->underlying, o->is_call ? "Call" : "Put",
                 o->strike, o->spot);
        break;
    }
    }
}

/* ============================================================
 * "Visitors" — just functions that switch on the tag.
 * This is the C equivalent of std::visit with overloaded lambdas.
 * ============================================================ */

static void visit_pricing(const Instrument *inst) {
    char desc[128];
    instrument_describe(inst, desc, sizeof(desc));

    switch (inst->type) {
    case INST_BOND: {
        const Bond *b = &inst->data.bond;
        double pv = 0;
        for (int i = 1; i <= b->maturity_years; ++i) {
            pv += (b->face_value * b->coupon_rate) / pow(1.05, i);
        }
        pv += b->face_value / pow(1.05, b->maturity_years);
        printf("  Price %-45s = $%.2f\n", desc, pv);
        break;
    }
    case INST_SWAP: {
        const Swap *s = &inst->data.swap;
        double market_rate = 0.04;
        double npv = s->notional * (s->fixed_rate - market_rate) * s->tenor_years;
        printf("  Price %-45s = $%.2f NPV\n", desc, npv);
        break;
    }
    case INST_OPTION: {
        const Option *o = &inst->data.option;
        double intrinsic = o->is_call
            ? fmax(0.0, o->spot - o->strike)
            : fmax(0.0, o->strike - o->spot);
        printf("  Price %-45s = $%.2f intrinsic\n", desc, intrinsic);
        break;
    }
    }
}

static void visit_risk(const Instrument *inst) {
    char desc[128];
    instrument_describe(inst, desc, sizeof(desc));

    switch (inst->type) {
    case INST_BOND: {
        const Bond *b = &inst->data.bond;
        double duration = b->maturity_years * 0.9;
        printf("  Risk  %-45s   duration=%.1f, DV01=$%.2f\n",
               desc, duration, b->face_value * duration * 0.0001);
        break;
    }
    case INST_SWAP: {
        const Swap *s = &inst->data.swap;
        double dv01 = s->notional * s->tenor_years * 0.0001;
        printf("  Risk  %-45s   DV01=$%.2f\n", desc, dv01);
        break;
    }
    case INST_OPTION: {
        const Option *o = &inst->data.option;
        double delta = o->is_call ? 0.55 : -0.45;
        printf("  Risk  %-45s   delta=%.2f\n", desc, delta);
        break;
    }
    }
}

static void visit_regulatory(const Instrument *inst) {
    char desc[128];
    instrument_describe(inst, desc, sizeof(desc));

    switch (inst->type) {
    case INST_BOND: {
        double charge = inst->data.bond.face_value * 0.08;
        printf("  Reg   %-45s   capital charge=$%.2f\n", desc, charge);
        break;
    }
    case INST_SWAP: {
        const Swap *s = &inst->data.swap;
        double charge = s->notional * 0.05 * s->tenor_years;
        printf("  Reg   %-45s   capital charge=$%.2f\n", desc, charge);
        break;
    }
    case INST_OPTION: {
        double charge = inst->data.option.spot * 100 * 0.10;
        printf("  Reg   %-45s   capital charge=$%.2f\n", desc, charge);
        break;
    }
    }
}

/* Generic "visit all" — takes a function pointer to any visitor */
typedef void (*VisitorFn)(const Instrument *);

static void visit_portfolio(const Instrument *portfolio, size_t count,
                             VisitorFn visitor) {
    for (size_t i = 0; i < count; ++i) {
        visitor(&portfolio[i]);
    }
}

/* ============================================================ */

#define PORTFOLIO_SIZE 4

int main(void) {
    puts("=== C Visitor Pattern: Financial Instruments ===\n");

    /* Portfolio: array of values on the stack. No heap. */
    Instrument portfolio[PORTFOLIO_SIZE] = {
        make_bond("US-TREASURY", 1000000, 0.045, 10),
        make_swap(5000000, 0.0375, 5),
        make_option("SPX", 4500.0, 4550.0, 1),
        make_option("AAPL", 190.0, 185.0, 0),
    };

    /* Apply different "visitors" — just function pointers */
    puts("--- Pricing ---");
    visit_portfolio(portfolio, PORTFOLIO_SIZE, visit_pricing);

    puts("\n--- Risk ---");
    visit_portfolio(portfolio, PORTFOLIO_SIZE, visit_risk);

    puts("\n--- Regulatory ---");
    visit_portfolio(portfolio, PORTFOLIO_SIZE, visit_regulatory);

    /* Copy the entire portfolio — just memcpy under the hood */
    puts("\n--- Copying portfolio (value semantics for free!) ---");
    Instrument portfolio2[PORTFOLIO_SIZE];
    memcpy(portfolio2, portfolio, sizeof(portfolio));

    /* Modify the copy independently */
    portfolio2[0] = make_bond("UK-GILT", 500000, 0.04, 5);

    puts("Original [0]:");
    visit_pricing(&portfolio[0]);
    puts("Copy [0] (modified independently):");
    visit_pricing(&portfolio2[0]);

    printf("\n  sizeof(Instrument) = %zu bytes\n", sizeof(Instrument));
    printf("  sizeof(portfolio)  = %zu bytes (all on stack)\n", sizeof(portfolio));

    return 0;
}
