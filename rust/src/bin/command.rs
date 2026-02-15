// ============================================================
// Command Pattern in Rust — Trade Management with Undo/Redo
//
// Two approaches:
//
// 1. Enum commands (closed set)
//    - Commands are pure data, #[derive(Clone)]
//    - Execute/undo are match arms in free functions
//    - History is Vec<TradeAction> — trivially cloneable
//    - Perfect for event sourcing and audit trails
//
// 2. Trait objects (open extension)
//    - Box<dyn Command> with clone_box for value semantics
//    - New command types don't require modifying existing code
//    - Slightly more ceremony than enum approach
//
// Rust's ownership system gives a unique advantage here:
// the borrow checker ensures commands can't hold dangling
// references to the portfolio. Commands store data, not
// references — the portfolio is passed explicitly to execute/undo.
// ============================================================

use std::collections::HashMap;
use std::fmt;

// --- Receiver: Portfolio ---

#[derive(Debug, Clone)]
struct Portfolio {
    positions: HashMap<String, i32>,
    cash: f64,
}

impl Portfolio {
    fn new(cash: f64) -> Self {
        Self {
            positions: HashMap::new(),
            cash,
        }
    }

    fn buy(&mut self, symbol: &str, qty: i32, price: f64) {
        *self.positions.entry(symbol.to_string()).or_insert(0) += qty;
        self.cash -= qty as f64 * price;
        println!(
            "  [EXEC] BUY  {} {} @ ${:.2}  (cash: ${:.2})",
            qty, symbol, price, self.cash
        );
    }

    fn sell(&mut self, symbol: &str, qty: i32, price: f64) {
        *self.positions.entry(symbol.to_string()).or_insert(0) -= qty;
        self.cash += qty as f64 * price;
        println!(
            "  [EXEC] SELL {} {} @ ${:.2}  (cash: ${:.2})",
            qty, symbol, price, self.cash
        );
    }

    fn reverse_buy(&mut self, symbol: &str, qty: i32, price: f64) {
        *self.positions.entry(symbol.to_string()).or_insert(0) -= qty;
        self.cash += qty as f64 * price;
        println!(
            "  [UNDO] BUY  {} {} @ ${:.2} reversed  (cash: ${:.2})",
            qty, symbol, price, self.cash
        );
    }

    fn reverse_sell(&mut self, symbol: &str, qty: i32, price: f64) {
        *self.positions.entry(symbol.to_string()).or_insert(0) += qty;
        self.cash -= qty as f64 * price;
        println!(
            "  [UNDO] SELL {} {} @ ${:.2} reversed  (cash: ${:.2})",
            qty, symbol, price, self.cash
        );
    }

    fn print_positions(&self) {
        println!("  Portfolio:");
        println!("    Cash: ${:.2}", self.cash);
        for (sym, qty) in &self.positions {
            if *qty != 0 {
                println!("    {}: {} shares", sym, qty);
            }
        }
    }
}

// ============================================================
// APPROACH 1: Enum Commands (closed set)
// ============================================================

#[derive(Debug, Clone)]
enum TradeAction {
    Buy {
        symbol: String,
        quantity: i32,
        price: f64,
    },
    Sell {
        symbol: String,
        quantity: i32,
        price: f64,
    },
}

impl fmt::Display for TradeAction {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Buy {
                symbol,
                quantity,
                price,
            } => write!(f, "BUY {} {} @ ${:.2}", quantity, symbol, price),
            Self::Sell {
                symbol,
                quantity,
                price,
            } => write!(f, "SELL {} {} @ ${:.2}", quantity, symbol, price),
        }
    }
}

impl TradeAction {
    fn execute(&self, portfolio: &mut Portfolio) {
        match self {
            Self::Buy {
                symbol,
                quantity,
                price,
            } => portfolio.buy(symbol, *quantity, *price),
            Self::Sell {
                symbol,
                quantity,
                price,
            } => portfolio.sell(symbol, *quantity, *price),
        }
    }

    fn undo(&self, portfolio: &mut Portfolio) {
        match self {
            Self::Buy {
                symbol,
                quantity,
                price,
            } => portfolio.reverse_buy(symbol, *quantity, *price),
            Self::Sell {
                symbol,
                quantity,
                price,
            } => portfolio.reverse_sell(symbol, *quantity, *price),
        }
    }
}

// --- Command History: Vec of Clone-able values ---

#[derive(Debug, Clone)]
struct TradeHistory {
    executed: Vec<TradeAction>,
    undone: Vec<TradeAction>,
}

impl TradeHistory {
    fn new() -> Self {
        Self {
            executed: Vec::new(),
            undone: Vec::new(),
        }
    }

    fn execute(&mut self, action: TradeAction, portfolio: &mut Portfolio) {
        action.execute(portfolio);
        self.executed.push(action);
        self.undone.clear();
    }

    fn undo(&mut self, portfolio: &mut Portfolio) -> bool {
        if let Some(action) = self.executed.pop() {
            action.undo(portfolio);
            self.undone.push(action);
            true
        } else {
            false
        }
    }

    fn redo(&mut self, portfolio: &mut Portfolio) -> bool {
        if let Some(action) = self.undone.pop() {
            action.execute(portfolio);
            self.executed.push(action);
            true
        } else {
            false
        }
    }

    fn print_history(&self) {
        println!("  Trade History:");
        if self.executed.is_empty() {
            println!("    (empty)");
        } else {
            for (i, action) in self.executed.iter().enumerate() {
                println!("    {}. {}", i + 1, action);
            }
        }
    }
}

// ============================================================
// APPROACH 2: Trait Objects (open extension)
// ============================================================

trait Command: fmt::Debug {
    fn execute(&self, portfolio: &mut Portfolio);
    fn undo(&self, portfolio: &mut Portfolio);
    fn description(&self) -> String;
    fn clone_box(&self) -> Box<dyn Command>;
}

impl Clone for Box<dyn Command> {
    fn clone(&self) -> Self {
        self.clone_box()
    }
}

#[derive(Debug, Clone)]
struct MarketBuy {
    symbol: String,
    quantity: i32,
    price: f64,
}

impl Command for MarketBuy {
    fn execute(&self, portfolio: &mut Portfolio) {
        portfolio.buy(&self.symbol, self.quantity, self.price);
    }

    fn undo(&self, portfolio: &mut Portfolio) {
        portfolio.reverse_buy(&self.symbol, self.quantity, self.price);
    }

    fn description(&self) -> String {
        format!(
            "MARKET BUY {} {} @ ${:.2}",
            self.quantity, self.symbol, self.price
        )
    }

    fn clone_box(&self) -> Box<dyn Command> {
        Box::new(self.clone())
    }
}

#[derive(Debug, Clone)]
struct LimitSell {
    symbol: String,
    quantity: i32,
    limit_price: f64,
}

impl Command for LimitSell {
    fn execute(&self, portfolio: &mut Portfolio) {
        portfolio.sell(&self.symbol, self.quantity, self.limit_price);
    }

    fn undo(&self, portfolio: &mut Portfolio) {
        portfolio.reverse_sell(&self.symbol, self.quantity, self.limit_price);
    }

    fn description(&self) -> String {
        format!(
            "LIMIT SELL {} {} @ ${:.2}",
            self.quantity, self.symbol, self.limit_price
        )
    }

    fn clone_box(&self) -> Box<dyn Command> {
        Box::new(self.clone())
    }
}

// ============================================================

fn main() {
    println!("=== Rust Command Pattern: Trade Management ===");
    println!("========== Approach 1: Enum Commands ==========\n");

    let mut portfolio = Portfolio::new(1_000_000.0);
    let mut history = TradeHistory::new();

    println!("--- Executing trades ---");
    history.execute(
        TradeAction::Buy {
            symbol: "AAPL".into(),
            quantity: 100,
            price: 185.50,
        },
        &mut portfolio,
    );
    history.execute(
        TradeAction::Buy {
            symbol: "GOOGL".into(),
            quantity: 50,
            price: 140.25,
        },
        &mut portfolio,
    );
    history.execute(
        TradeAction::Sell {
            symbol: "MSFT".into(),
            quantity: 75,
            price: 420.00,
        },
        &mut portfolio,
    );

    println!();
    portfolio.print_positions();
    println!();
    history.print_history();

    println!("\n--- Undo last trade ---");
    history.undo(&mut portfolio);
    portfolio.print_positions();

    println!("\n--- Undo another ---");
    history.undo(&mut portfolio);
    portfolio.print_positions();

    println!("\n--- Redo ---");
    history.redo(&mut portfolio);
    portfolio.print_positions();

    // Snapshot: just clone
    println!("\n--- Snapshot history (clone!) ---");
    let snapshot = history.clone();
    println!("  Snapshot has {} trades", snapshot.executed.len());

    // Continue on original
    history.execute(
        TradeAction::Sell {
            symbol: "AAPL".into(),
            quantity: 50,
            price: 190.00,
        },
        &mut portfolio,
    );

    println!("\n--- Original history ---");
    history.print_history();

    println!("\n--- Snapshot unchanged ---");
    snapshot.print_history();

    // ============================================================
    println!("\n========== Approach 2: Trait Objects ==========\n");

    let mut portfolio2 = Portfolio::new(500_000.0);

    let commands: Vec<Box<dyn Command>> = vec![
        Box::new(MarketBuy {
            symbol: "TSLA".into(),
            quantity: 200,
            price: 175.00,
        }),
        Box::new(LimitSell {
            symbol: "NVDA".into(),
            quantity: 30,
            limit_price: 890.50,
        }),
    ];

    println!("--- Executing trait commands ---");
    for cmd in &commands {
        cmd.execute(&mut portfolio2);
    }

    println!("\n--- Undoing all ---");
    for cmd in commands.iter().rev() {
        cmd.undo(&mut portfolio2);
    }

    portfolio2.print_positions();

    // Commands are cloneable
    println!("\n--- Commands are cloneable ---");
    let commands_copy = commands.clone();
    println!("  Original: {} commands", commands.len());
    println!("  Copy:     {} commands", commands_copy.len());
    for cmd in &commands_copy {
        println!("    {}", cmd.description());
    }

    // ============================================================
    // Rust's ownership advantage:
    //
    // Notice that commands DON'T hold references to the portfolio.
    // The portfolio is passed as &mut to execute/undo. The borrow
    // checker enforces this at compile time — you literally cannot
    // create a command that holds a dangling reference to a portfolio
    // that might be moved or dropped. This eliminates an entire
    // class of bugs that C++ and C must manage manually.
    // ============================================================
}
