// ============================================================
// Visitor Pattern in Rust — Financial Instrument Operations
//
// Rust makes the classic Visitor pattern almost entirely
// unnecessary. The enum + match combination gives you:
//
//   - Exhaustive dispatch (compiler error if you miss a variant)
//   - No double dispatch indirection
//   - No accept() boilerplate
//   - Value semantics via Clone/Copy
//   - Pattern matching with destructuring (access fields directly)
//
// The enum IS the variant. match IS the visit. That's it.
//
// We also show a trait-based approach for when you want open
// extension on the operation side (new instruments without
// modifying existing operations).
// ============================================================

use std::fmt;

// --- Instrument types: plain structs ---

#[derive(Debug, Clone)]
struct Bond {
    issuer: String,
    face_value: f64,
    coupon_rate: f64,
    maturity_years: u32,
}

#[derive(Debug, Clone)]
struct Swap {
    notional: f64,
    fixed_rate: f64,
    tenor_years: u32,
}

#[derive(Debug, Clone)]
struct Option {
    underlying: String,
    strike: f64,
    spot: f64,
    is_call: bool,
}

// --- The enum IS the polymorphic type ---

#[derive(Debug, Clone)]
enum Instrument {
    Bond(Bond),
    Swap(Swap),
    Option(Option),
}

impl fmt::Display for Instrument {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Bond(b) => write!(
                f,
                "Bond({}, {:.0} face, {:.1}% coupon, {}Y)",
                b.issuer,
                b.face_value,
                b.coupon_rate * 100.0,
                b.maturity_years
            ),
            Self::Swap(s) => write!(
                f,
                "IRS({:.0} notional, {:.2}% fixed, {}Y)",
                s.notional,
                s.fixed_rate * 100.0,
                s.tenor_years
            ),
            Self::Option(o) => write!(
                f,
                "{} {}(K={:.2}, S={:.2})",
                o.underlying,
                if o.is_call { "Call" } else { "Put" },
                o.strike,
                o.spot
            ),
        }
    }
}

// ============================================================
// "Visitors" are just functions that match on the enum.
// No Visitor trait, no accept(), no double dispatch.
// ============================================================

fn price(inst: &Instrument) -> f64 {
    match inst {
        Instrument::Bond(b) => {
            let mut pv = 0.0;
            for i in 1..=b.maturity_years {
                pv += (b.face_value * b.coupon_rate) / 1.05_f64.powi(i as i32);
            }
            pv += b.face_value / 1.05_f64.powi(b.maturity_years as i32);
            pv
        }
        Instrument::Swap(s) => {
            let market_rate = 0.04;
            s.notional * (s.fixed_rate - market_rate) * s.tenor_years as f64
        }
        Instrument::Option(o) => {
            if o.is_call {
                (o.spot - o.strike).max(0.0)
            } else {
                (o.strike - o.spot).max(0.0)
            }
        }
    }
}

fn risk_report(inst: &Instrument) {
    match inst {
        Instrument::Bond(b) => {
            let duration = b.maturity_years as f64 * 0.9;
            let dv01 = b.face_value * duration * 0.0001;
            println!("  Risk  {:<45}   duration={:.1}, DV01=${:.2}", inst, duration, dv01);
        }
        Instrument::Swap(s) => {
            let dv01 = s.notional * s.tenor_years as f64 * 0.0001;
            println!("  Risk  {:<45}   DV01=${:.2}", inst, dv01);
        }
        Instrument::Option(o) => {
            let delta = if o.is_call { 0.55 } else { -0.45 };
            println!("  Risk  {:<45}   delta={:.2}", inst, delta);
        }
    }
}

fn regulatory_report(inst: &Instrument) {
    let charge = match inst {
        Instrument::Bond(b) => b.face_value * 0.08,
        Instrument::Swap(s) => s.notional * 0.05 * s.tenor_years as f64,
        Instrument::Option(o) => o.spot * 100.0 * 0.10,
    };
    println!("  Reg   {:<45}   capital charge=${:.2}", inst, charge);
}

// ============================================================
// Trait-based visitor: useful when you want to pass different
// operations as values (first-class visitors).
// ============================================================

trait InstrumentVisitor {
    fn visit_bond(&self, b: &Bond);
    fn visit_swap(&self, s: &Swap);
    fn visit_option(&self, o: &Option);
}

// A single dispatch function replaces accept() on every type
fn visit(inst: &Instrument, visitor: &dyn InstrumentVisitor) {
    match inst {
        Instrument::Bond(b) => visitor.visit_bond(b),
        Instrument::Swap(s) => visitor.visit_swap(s),
        Instrument::Option(o) => visitor.visit_option(o),
    }
}

struct PricePrinter;

impl InstrumentVisitor for PricePrinter {
    fn visit_bond(&self, b: &Bond) {
        let mut pv = 0.0;
        for i in 1..=b.maturity_years {
            pv += (b.face_value * b.coupon_rate) / 1.05_f64.powi(i as i32);
        }
        pv += b.face_value / 1.05_f64.powi(b.maturity_years as i32);
        println!(
            "  [trait] Bond({}, {:.0} face) = ${:.2}",
            b.issuer, b.face_value, pv
        );
    }

    fn visit_swap(&self, s: &Swap) {
        let npv = s.notional * (s.fixed_rate - 0.04) * s.tenor_years as f64;
        println!(
            "  [trait] IRS({:.0} notional, {}Y) = ${:.2} NPV",
            s.notional, s.tenor_years, npv
        );
    }

    fn visit_option(&self, o: &Option) {
        let intrinsic = if o.is_call {
            (o.spot - o.strike).max(0.0)
        } else {
            (o.strike - o.spot).max(0.0)
        };
        println!(
            "  [trait] {} {}(K={:.2}) = ${:.2} intrinsic",
            o.underlying,
            if o.is_call { "Call" } else { "Put" },
            o.strike,
            intrinsic
        );
    }
}

// ============================================================

fn main() {
    println!("=== Rust Visitor Pattern: Financial Instruments ===\n");

    // Portfolio: Vec of values, fully cloneable
    let portfolio = vec![
        Instrument::Bond(Bond {
            issuer: "US-TREASURY".to_string(),
            face_value: 1_000_000.0,
            coupon_rate: 0.045,
            maturity_years: 10,
        }),
        Instrument::Swap(Swap {
            notional: 5_000_000.0,
            fixed_rate: 0.0375,
            tenor_years: 5,
        }),
        Instrument::Option(Option {
            underlying: "SPX".to_string(),
            strike: 4500.0,
            spot: 4550.0,
            is_call: true,
        }),
        Instrument::Option(Option {
            underlying: "AAPL".to_string(),
            strike: 190.0,
            spot: 185.0,
            is_call: false,
        }),
    ];

    // --- Pricing (function that returns a value) ---
    println!("--- Pricing ---");
    for inst in &portfolio {
        let px = price(inst);
        println!("  Price {:<45} = ${:.2}", inst, px);
    }

    // --- Risk (function with side effects) ---
    println!("\n--- Risk ---");
    for inst in &portfolio {
        risk_report(inst);
    }

    // --- Regulatory ---
    println!("\n--- Regulatory ---");
    for inst in &portfolio {
        regulatory_report(inst);
    }

    // --- Portfolio is cloneable ---
    println!("\n--- Cloning portfolio ---");
    let mut portfolio2 = portfolio.clone();
    portfolio2.push(Instrument::Bond(Bond {
        issuer: "UK-GILT".to_string(),
        face_value: 500_000.0,
        coupon_rate: 0.04,
        maturity_years: 5,
    }));
    println!("  Original size: {}", portfolio.len());
    println!("  Clone size:    {}", portfolio2.len());

    // --- Trait-based visitor ---
    println!("\n--- Trait-based visitor ---");
    let pricer = PricePrinter;
    for inst in &portfolio {
        visit(inst, &pricer);
    }

    // --- Exhaustiveness ---
    // If you add a new variant to the Instrument enum (e.g., FRA)
    // and forget to handle it in ANY match, the compiler emits:
    //   error[E0004]: non-exhaustive patterns: `Instrument::FRA(_)` not covered
    // This is a hard error, not a warning. You cannot ship the code.

    println!("\n  sizeof Instrument: {} bytes", std::mem::size_of::<Instrument>());
    println!(
        "  sizeof Vec<Instrument> (4 items): {} bytes on stack + {} bytes on heap",
        std::mem::size_of::<Vec<Instrument>>(),
        std::mem::size_of::<Instrument>() * portfolio.len()
    );
}
