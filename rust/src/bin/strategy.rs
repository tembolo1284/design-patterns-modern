// ============================================================
// Strategy Pattern in Rust — Order Execution Strategies
//
// Rust gives you three natural approaches:
//
// 1. Enum dispatch (like std::variant / C tagged union)
//    - Closed set, zero overhead, exhaustive match
//    - Clone/Copy for free via derive
//
// 2. Trait objects (like C++ type erasure / C function pointers)
//    - Open set, dynamic dispatch via vtable
//    - Box<dyn Trait> for owned, &dyn Trait for borrowed
//    - Clone requires a workaround (CloneStrategy supertrait)
//
// 3. Closures (Rust's killer feature for Strategy)
//    - Any Fn closure IS a strategy — no boilerplate at all
//    - The most idiomatic Rust approach for simple strategies
//
// All three shown below.
// ============================================================

use std::fmt;

// ============================================================
// APPROACH 1: Enum Dispatch
// ============================================================

#[derive(Debug, Clone)]
enum ExecutionStrategy {
    Twap { slices: u32 },
    Vwap { participation_rate: f64 },
    Iceberg { visible_qty: u32 },
}

impl ExecutionStrategy {
    fn execute(&self, symbol: &str, quantity: u32, price: f64) {
        match self {
            Self::Twap { slices } => {
                let per_slice = quantity / slices;
                println!(
                    "[TWAP] Executing {}: {} shares @ ${:.2} across {} slices ({}/slice)",
                    symbol, quantity, price, slices, per_slice
                );
            }
            Self::Vwap { participation_rate } => {
                println!(
                    "[VWAP] Executing {}: {} shares @ ${:.2} with {:.0}% participation",
                    symbol, quantity, price, participation_rate * 100.0
                );
            }
            Self::Iceberg { visible_qty } => {
                println!(
                    "[Iceberg] Executing {}: {} shares @ ${:.2} showing {} at a time",
                    symbol, quantity, price, visible_qty
                );
            }
        }
    }

    fn name(&self) -> &'static str {
        match self {
            Self::Twap { .. } => "TWAP",
            Self::Vwap { .. } => "VWAP",
            Self::Iceberg { .. } => "Iceberg",
        }
    }
}

#[derive(Debug, Clone)]
struct Order {
    symbol: String,
    quantity: u32,
    price: f64,
    strategy: ExecutionStrategy, // value, not a pointer
}

impl Order {
    fn new(symbol: &str, quantity: u32, price: f64, strategy: ExecutionStrategy) -> Self {
        Self {
            symbol: symbol.to_string(),
            quantity,
            price,
            strategy,
        }
    }

    fn set_strategy(&mut self, strategy: ExecutionStrategy) {
        self.strategy = strategy;
    }

    fn send(&self) {
        println!(
            "Order: {} {} shares @ ${:.2} using {}",
            self.symbol,
            self.quantity,
            self.price,
            self.strategy.name()
        );
        self.strategy.execute(&self.symbol, self.quantity, self.price);
    }
}

// ============================================================
// APPROACH 2: Trait Objects (open extension)
// ============================================================

// The base trait — Rust's equivalent of an abstract interface.
// We add a clone_box method to enable cloning of trait objects.
trait ExecutionStrategyTrait: fmt::Debug {
    fn execute(&self, symbol: &str, quantity: u32, price: f64);
    fn name(&self) -> &str;
    fn clone_box(&self) -> Box<dyn ExecutionStrategyTrait>;
}

impl Clone for Box<dyn ExecutionStrategyTrait> {
    fn clone(&self) -> Self {
        self.clone_box()
    }
}

// Concrete strategies — plain structs, opt into the trait explicitly
#[derive(Debug, Clone)]
struct TwapStrategy {
    slices: u32,
}

impl ExecutionStrategyTrait for TwapStrategy {
    fn execute(&self, symbol: &str, quantity: u32, price: f64) {
        let per_slice = quantity / self.slices;
        println!(
            "[TWAP-trait] Executing {}: {} shares @ ${:.2} across {} slices ({}/slice)",
            symbol, quantity, price, self.slices, per_slice
        );
    }

    fn name(&self) -> &str {
        "TWAP"
    }

    fn clone_box(&self) -> Box<dyn ExecutionStrategyTrait> {
        Box::new(self.clone())
    }
}

#[derive(Debug, Clone)]
struct VwapStrategy {
    participation_rate: f64,
}

impl ExecutionStrategyTrait for VwapStrategy {
    fn execute(&self, symbol: &str, quantity: u32, price: f64) {
        println!(
            "[VWAP-trait] Executing {}: {} shares @ ${:.2} with {:.0}% participation",
            symbol, quantity, price, self.participation_rate * 100.0
        );
    }

    fn name(&self) -> &str {
        "VWAP"
    }

    fn clone_box(&self) -> Box<dyn ExecutionStrategyTrait> {
        Box::new(self.clone())
    }
}

#[derive(Debug)]
struct TraitOrder {
    symbol: String,
    quantity: u32,
    price: f64,
    strategy: Box<dyn ExecutionStrategyTrait>,
}

impl Clone for TraitOrder {
    fn clone(&self) -> Self {
        Self {
            symbol: self.symbol.clone(),
            quantity: self.quantity,
            price: self.price,
            strategy: self.strategy.clone_box(),
        }
    }
}

impl TraitOrder {
    fn new(symbol: &str, qty: u32, price: f64, strategy: Box<dyn ExecutionStrategyTrait>) -> Self {
        Self {
            symbol: symbol.to_string(),
            quantity: qty,
            price,
            strategy,
        }
    }

    fn set_strategy(&mut self, strategy: Box<dyn ExecutionStrategyTrait>) {
        self.strategy = strategy;
    }

    fn send(&self) {
        println!(
            "Order: {} {} shares @ ${:.2} using {}",
            self.symbol,
            self.quantity,
            self.price,
            self.strategy.name()
        );
        self.strategy
            .execute(&self.symbol, self.quantity, self.price);
    }
}

// ============================================================
// APPROACH 3: Closures (most idiomatic for simple strategies)
// ============================================================

type StrategyFn = Box<dyn Fn(&str, u32, f64)>;

fn twap_closure(slices: u32) -> StrategyFn {
    Box::new(move |symbol, qty, price| {
        let per_slice = qty / slices;
        println!(
            "[TWAP-closure] Executing {}: {} shares @ ${:.2} across {} slices ({}/slice)",
            symbol, qty, price, slices, per_slice
        );
    })
}

fn vwap_closure(participation_rate: f64) -> StrategyFn {
    Box::new(move |symbol, qty, price| {
        println!(
            "[VWAP-closure] Executing {}: {} shares @ ${:.2} with {:.0}% participation",
            symbol, qty, price, participation_rate * 100.0
        );
    })
}

// ============================================================

fn main() {
    println!("=== Rust Strategy Pattern: Order Execution ===");
    println!("========== Approach 1: Enum Dispatch ==========\n");

    let mut order = Order::new("AAPL", 10000, 185.50, ExecutionStrategy::Twap { slices: 5 });
    order.send();

    println!("\n--- Switching to VWAP ---");
    order.set_strategy(ExecutionStrategy::Vwap {
        participation_rate: 0.15,
    });
    order.send();

    println!("\n--- Switching to Iceberg ---");
    order.set_strategy(ExecutionStrategy::Iceberg { visible_qty: 500 });
    order.send();

    // Clone is trivial — #[derive(Clone)] does everything
    println!("\n--- Cloning order ---");
    let mut order2 = order.clone();
    order2.set_strategy(ExecutionStrategy::Twap { slices: 10 });

    println!("Original:");
    order.send();
    println!("Clone (independent):");
    order2.send();

    println!("\n========== Approach 2: Trait Objects ==========\n");

    let mut trait_order =
        TraitOrder::new("GOOGL", 5000, 140.25, Box::new(TwapStrategy { slices: 8 }));
    trait_order.send();

    println!("\n--- Switching to VWAP ---");
    trait_order.set_strategy(Box::new(VwapStrategy {
        participation_rate: 0.20,
    }));
    trait_order.send();

    // Cloneable via clone_box
    println!("\n--- Cloning trait order ---");
    let trait_order2 = trait_order.clone();
    println!("Original:");
    trait_order.send();
    println!("Clone:");
    trait_order2.send();

    println!("\n========== Approach 3: Closures ==========\n");

    let strategy = twap_closure(6);
    strategy("TSLA", 3000, 175.00);

    let strategy = vwap_closure(0.25);
    strategy("NVDA", 1000, 890.50);
}// ============================================================
// Strategy Pattern in Rust — Order Execution Strategies
//
// Rust gives you three natural approaches:
//
// 1. Enum dispatch (like std::variant / C tagged union)
//    - Closed set, zero overhead, exhaustive match
//    - Clone/Copy for free via derive
//
// 2. Trait objects (like C++ type erasure / C function pointers)
//    - Open set, dynamic dispatch via vtable
//    - Box<dyn Trait> for owned, &dyn Trait for borrowed
//    - Clone requires a workaround (CloneStrategy supertrait)
//
// 3. Closures (Rust's killer feature for Strategy)
//    - Any Fn closure IS a strategy — no boilerplate at all
//    - The most idiomatic Rust approach for simple strategies
//
// All three shown below.
// ============================================================

use std::fmt;

// ============================================================
// APPROACH 1: Enum Dispatch
// ============================================================

#[derive(Debug, Clone)]
enum ExecutionStrategy {
    Twap { slices: u32 },
    Vwap { participation_rate: f64 },
    Iceberg { visible_qty: u32 },
}

impl ExecutionStrategy {
    fn execute(&self, symbol: &str, quantity: u32, price: f64) {
        match self {
            Self::Twap { slices } => {
                let per_slice = quantity / slices;
                println!(
                    "[TWAP] Executing {}: {} shares @ ${:.2} across {} slices ({}/slice)",
                    symbol, quantity, price, slices, per_slice
                );
            }
            Self::Vwap { participation_rate } => {
                println!(
                    "[VWAP] Executing {}: {} shares @ ${:.2} with {:.0}% participation",
                    symbol, quantity, price, participation_rate * 100.0
                );
            }
            Self::Iceberg { visible_qty } => {
                println!(
                    "[Iceberg] Executing {}: {} shares @ ${:.2} showing {} at a time",
                    symbol, quantity, price, visible_qty
                );
            }
        }
    }

    fn name(&self) -> &'static str {
        match self {
            Self::Twap { .. } => "TWAP",
            Self::Vwap { .. } => "VWAP",
            Self::Iceberg { .. } => "Iceberg",
        }
    }
}

#[derive(Debug, Clone)]
struct Order {
    symbol: String,
    quantity: u32,
    price: f64,
    strategy: ExecutionStrategy, // value, not a pointer
}

impl Order {
    fn new(symbol: &str, quantity: u32, price: f64, strategy: ExecutionStrategy) -> Self {
        Self {
            symbol: symbol.to_string(),
            quantity,
            price,
            strategy,
        }
    }

    fn set_strategy(&mut self, strategy: ExecutionStrategy) {
        self.strategy = strategy;
    }

    fn send(&self) {
        println!(
            "Order: {} {} shares @ ${:.2} using {}",
            self.symbol,
            self.quantity,
            self.price,
            self.strategy.name()
        );
        self.strategy.execute(&self.symbol, self.quantity, self.price);
    }
}

// ============================================================
// APPROACH 2: Trait Objects (open extension)
// ============================================================

// The base trait — Rust's equivalent of an abstract interface.
// We add a clone_box method to enable cloning of trait objects.
trait ExecutionStrategyTrait: fmt::Debug {
    fn execute(&self, symbol: &str, quantity: u32, price: f64);
    fn name(&self) -> &str;
    fn clone_box(&self) -> Box<dyn ExecutionStrategyTrait>;
}

impl Clone for Box<dyn ExecutionStrategyTrait> {
    fn clone(&self) -> Self {
        self.clone_box()
    }
}

// Concrete strategies — plain structs, opt into the trait explicitly
#[derive(Debug, Clone)]
struct TwapStrategy {
    slices: u32,
}

impl ExecutionStrategyTrait for TwapStrategy {
    fn execute(&self, symbol: &str, quantity: u32, price: f64) {
        let per_slice = quantity / self.slices;
        println!(
            "[TWAP-trait] Executing {}: {} shares @ ${:.2} across {} slices ({}/slice)",
            symbol, quantity, price, self.slices, per_slice
        );
    }

    fn name(&self) -> &str {
        "TWAP"
    }

    fn clone_box(&self) -> Box<dyn ExecutionStrategyTrait> {
        Box::new(self.clone())
    }
}

#[derive(Debug, Clone)]
struct VwapStrategy {
    participation_rate: f64,
}

impl ExecutionStrategyTrait for VwapStrategy {
    fn execute(&self, symbol: &str, quantity: u32, price: f64) {
        println!(
            "[VWAP-trait] Executing {}: {} shares @ ${:.2} with {:.0}% participation",
            symbol, quantity, price, self.participation_rate * 100.0
        );
    }

    fn name(&self) -> &str {
        "VWAP"
    }

    fn clone_box(&self) -> Box<dyn ExecutionStrategyTrait> {
        Box::new(self.clone())
    }
}

#[derive(Debug)]
struct TraitOrder {
    symbol: String,
    quantity: u32,
    price: f64,
    strategy: Box<dyn ExecutionStrategyTrait>,
}

impl Clone for TraitOrder {
    fn clone(&self) -> Self {
        Self {
            symbol: self.symbol.clone(),
            quantity: self.quantity,
            price: self.price,
            strategy: self.strategy.clone_box(),
        }
    }
}

impl TraitOrder {
    fn new(symbol: &str, qty: u32, price: f64, strategy: Box<dyn ExecutionStrategyTrait>) -> Self {
        Self {
            symbol: symbol.to_string(),
            quantity: qty,
            price,
            strategy,
        }
    }

    fn set_strategy(&mut self, strategy: Box<dyn ExecutionStrategyTrait>) {
        self.strategy = strategy;
    }

    fn send(&self) {
        println!(
            "Order: {} {} shares @ ${:.2} using {}",
            self.symbol,
            self.quantity,
            self.price,
            self.strategy.name()
        );
        self.strategy
            .execute(&self.symbol, self.quantity, self.price);
    }
}

// ============================================================
// APPROACH 3: Closures (most idiomatic for simple strategies)
// ============================================================

type StrategyFn = Box<dyn Fn(&str, u32, f64)>;

fn twap_closure(slices: u32) -> StrategyFn {
    Box::new(move |symbol, qty, price| {
        let per_slice = qty / slices;
        println!(
            "[TWAP-closure] Executing {}: {} shares @ ${:.2} across {} slices ({}/slice)",
            symbol, qty, price, slices, per_slice
        );
    })
}

fn vwap_closure(participation_rate: f64) -> StrategyFn {
    Box::new(move |symbol, qty, price| {
        println!(
            "[VWAP-closure] Executing {}: {} shares @ ${:.2} with {:.0}% participation",
            symbol, qty, price, participation_rate * 100.0
        );
    })
}

// ============================================================

fn main() {
    println!("=== Rust Strategy Pattern: Order Execution ===");
    println!("========== Approach 1: Enum Dispatch ==========\n");

    let mut order = Order::new("AAPL", 10000, 185.50, ExecutionStrategy::Twap { slices: 5 });
    order.send();

    println!("\n--- Switching to VWAP ---");
    order.set_strategy(ExecutionStrategy::Vwap {
        participation_rate: 0.15,
    });
    order.send();

    println!("\n--- Switching to Iceberg ---");
    order.set_strategy(ExecutionStrategy::Iceberg { visible_qty: 500 });
    order.send();

    // Clone is trivial — #[derive(Clone)] does everything
    println!("\n--- Cloning order ---");
    let mut order2 = order.clone();
    order2.set_strategy(ExecutionStrategy::Twap { slices: 10 });

    println!("Original:");
    order.send();
    println!("Clone (independent):");
    order2.send();

    println!("\n========== Approach 2: Trait Objects ==========\n");

    let mut trait_order =
        TraitOrder::new("GOOGL", 5000, 140.25, Box::new(TwapStrategy { slices: 8 }));
    trait_order.send();

    println!("\n--- Switching to VWAP ---");
    trait_order.set_strategy(Box::new(VwapStrategy {
        participation_rate: 0.20,
    }));
    trait_order.send();

    // Cloneable via clone_box
    println!("\n--- Cloning trait order ---");
    let trait_order2 = trait_order.clone();
    println!("Original:");
    trait_order.send();
    println!("Clone:");
    trait_order2.send();

    println!("\n========== Approach 3: Closures ==========\n");

    let strategy = twap_closure(6);
    strategy("TSLA", 3000, 175.00);

    let strategy = vwap_closure(0.25);
    strategy("NVDA", 1000, 890.50);
}
