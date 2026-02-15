# Design Patterns: Classic C++ vs Modern C++ vs Pure C vs Rust

> **One-liner:** Side-by-side implementations of Strategy, Visitor, and Command in classic OOP C++, modern value-semantic C++, pure C, and Rust — showing the full arc from inheritance-based polymorphism to data-oriented design across four languages.

---

## Table of Contents

1. [Overview](#overview)
2. [Why This Project Exists](#why-this-project-exists)
3. [The Strategy Pattern](#the-strategy-pattern)
   - [What It Is](#what-strategy-is)
   - [Classic C++ Strategy](#classic-c-strategy)
   - [Modern C++ Strategy](#modern-c-strategy)
   - [Pure C Strategy](#pure-c-strategy)
   - [Rust Strategy](#rust-strategy)
   - [Strategy Comparison](#strategy-comparison)
4. [The Visitor Pattern](#the-visitor-pattern)
   - [What It Is](#what-visitor-is)
   - [Classic C++ Visitor](#classic-c-visitor)
   - [Modern C++ Visitor](#modern-c-visitor)
   - [Pure C Visitor](#pure-c-visitor)
   - [Rust Visitor](#rust-visitor)
   - [Visitor Comparison](#visitor-comparison)
5. [The Command Pattern](#the-command-pattern)
   - [What It Is](#what-command-is)
   - [Classic C++ Command](#classic-c-command)
   - [Modern C++ Command](#modern-c-command)
   - [Pure C Command](#pure-c-command)
   - [Rust Command](#rust-command)
   - [Command Comparison](#command-comparison)
6. [Key Concepts Explained](#key-concepts-explained)
   - [Value Semantics vs Reference Semantics](#value-semantics-vs-reference-semantics)
   - [std::variant and Tagged Unions](#stdvariant-and-tagged-unions)
   - [Type Erasure](#type-erasure)
   - [The Overloaded Lambda Pattern](#the-overloaded-lambda-pattern)
   - [Rust Enums and Pattern Matching](#rust-enums-and-pattern-matching)
   - [Rust Trait Objects](#rust-trait-objects)
   - [Rust Ownership and the Borrow Checker](#rust-ownership-and-the-borrow-checker)
7. [When to Use What](#when-to-use-what)
8. [Building the Project](#building-the-project)
   - [Prerequisites](#prerequisites)
   - [Using build.sh (Recommended)](#using-buildsh-recommended)
   - [Manual Build](#manual-build)
9. [Project Structure](#project-structure)
10. [References](#references)

---

## Overview

This project implements three classic Gang of Four design patterns — Strategy, Visitor, and Command — in four different styles:

| Style | Mechanism | Value Semantics? | Heap Required? | Lifetime Safety? |
|-------|-----------|-----------------|----------------|-----------------|
| **Classic C++** | Inheritance, virtual dispatch, `unique_ptr` | No | Yes | Manual |
| **Modern C++** | `std::variant`, type erasure, `std::visit` | Yes | Sometimes | Manual |
| **Pure C** | Tagged unions, function pointer tables | Yes (by default) | No | Manual |
| **Rust** | `enum` + `match`, trait objects, ownership | Yes | Sometimes | Compiler-enforced |

All examples use a **financial trading domain**: order execution strategies (TWAP, VWAP, Iceberg), instrument pricing and risk (bonds, swaps, options), and trade management with undo/redo. This keeps the examples grounded in real-world problems rather than abstract shapes and animals.

---

## Why This Project Exists

The C++ community is undergoing a significant philosophical shift. For decades, the standard approach to runtime polymorphism was the "classic OOP" style inherited from Simula and Smalltalk: define an abstract base class, derive concrete types, and dispatch through virtual function calls via base class pointers.

This approach has real costs that compound in production systems: heap allocation for every polymorphic object, inability to copy object graphs without manual clone methods, cache-hostile pointer chasing, tight coupling between types through inheritance hierarchies, and the "expression problem" where adding new types or new operations always requires modifying existing code.

Modern C++ (C++17 and beyond) offers alternatives that provide runtime polymorphism with value semantics. Objects behave like integers: you can copy them, assign them, put them on the stack, and store them contiguously in vectors without worrying about ownership, lifetimes, or slicing.

The irony — and the reason this project includes pure C implementations — is that C has always worked this way. C structs are value types by default. Tagged unions are the original `std::variant`. Function pointer tables are the original type erasure. Modern C++ spent three decades building an inheritance-based object system on top of C, and is now circling back to the data-oriented patterns C has used since the 1970s.

Rust closes the loop. Its `enum` type is a first-class tagged union with compiler-enforced exhaustive pattern matching — what `std::variant` + `std::visit` achieves through template machinery, Rust provides as a language primitive. Its trait objects provide type erasure with explicit, compiler-checked lifetime guarantees that C++ and C can only enforce through programmer discipline. And its ownership system eliminates the dangling reference and use-after-free bugs that plague all three of the other languages.

---

## The Strategy Pattern

### What Strategy Is

**Intent:** Define a family of algorithms, encapsulate each one, and make them interchangeable at runtime. The strategy lets the algorithm vary independently from the clients that use it.

**Our domain:** An order execution system where trades can be executed using different algorithms — Time-Weighted Average Price (TWAP), Volume-Weighted Average Price (VWAP), or Iceberg — and the algorithm can be swapped at runtime without modifying the order itself.

**Why it matters:** In a real trading system, the choice of execution algorithm depends on market conditions, order size, urgency, and regulatory constraints. The same order might start with TWAP and switch to Iceberg if the market moves. The strategy pattern makes this swap a one-liner rather than a refactor.

### Classic C++ Strategy

**File:** `strategy/classic/main.cpp`

The classic approach uses an abstract base class `ExecutionStrategy` with pure virtual methods `execute()` and `name()`. Concrete strategies (`TWAPStrategy`, `VWAPStrategy`, `IcebergStrategy`) inherit from this base and override the virtual methods. The `Order` class holds a `std::unique_ptr<ExecutionStrategy>` to dispatch at runtime.

```cpp
class ExecutionStrategy {
public:
    virtual ~ExecutionStrategy() = default;
    virtual void execute(const std::string& symbol, int qty, double price) const = 0;
    virtual std::string name() const = 0;
};

class Order {
    std::unique_ptr<ExecutionStrategy> strategy_;  // heap-allocated, move-only
    // ...
};
```

**Pros:** Simple, well-understood, open for extension (new strategies don't touch existing code).

**Cons:** Every strategy is heap-allocated via `make_unique`. The `Order` is non-copyable because `unique_ptr` is move-only. If you need to snapshot an order or pass it by value, you're stuck writing manual `clone()` methods on every concrete strategy. Every concrete class is ~20 lines of boilerplate (class declaration, constructor, overrides). All concrete types are permanently coupled to the `ExecutionStrategy` base class.

### Modern C++ Strategy

**File:** `strategy/modern/main.cpp`

The modern approach uses type erasure. `ExecutionStrategy` is a concrete class that internally holds a `Concept`/`Model` hierarchy (hidden from the user). Any type with `execute()` and `name()` methods can be wrapped — no base class required. The `Order` is a fully copyable value type.

```cpp
struct TWAPStrategy {       // plain struct, no inheritance
    int slices;
    void execute(const std::string& symbol, int qty, double price) const;
    std::string name() const { return "TWAP"; }
};

Order order("AAPL", 10000, 185.50, TWAPStrategy{5});  // just pass a value
Order order2 = order;  // deep copy works!
```

**Pros:** Strategies are plain structs with zero coupling to any base class. The `Order` is copyable — deep copies of the type-erased strategy happen automatically through the internal `clone()` mechanism. No `make_unique` at the call site. Adding a new strategy is just writing a new struct with the right methods.

**Cons:** The type erasure machinery (Concept/Model/pimpl) adds ~30 lines of internal complexity. One heap allocation per strategy (mitigable with SBO). Slightly harder to debug since the concrete type is hidden behind the pimpl.

### Pure C Strategy

**File:** `c/strategy.c`

Two approaches are shown side by side:

**Tagged union** (closed set): An `ExecutionStrategy` struct containing an enum tag and a union of parameter structs. Dispatch is a switch statement. Stack-allocated, zero overhead, trivially copyable.

```c
typedef struct {
    StrategyType type;
    union {
        TWAPParams   twap;
        VWAPParams   vwap;
        IcebergParams iceberg;
    } params;
} ExecutionStrategy;

Order order2 = order;  // struct copy — value semantics for free
```

**Function pointer table** (open set): An `ErasedStrategy` struct carrying function pointers and an inline data buffer. Each "concrete type" provides its own function implementations. No heap allocation — the data lives in a fixed-size `storage[]` array inside the struct.

```c
typedef struct {
    void (*execute)(const void *self, const char *symbol, int qty, double price);
    const char* (*name)(const void *self);
    char storage[64];
} ErasedStrategy;
```

**Pros:** Both approaches give value semantics by default (C structs copy by value). Zero heap allocation. The tagged union is the same concept as `std::variant` without the template machinery. The function pointer table is the same concept as C++ type erasure with built-in SBO.

**Cons:** No compile-time exhaustiveness checking on switch statements (only `-Wswitch` warnings). No type safety on the union (you can read the wrong member). Function pointer approach requires manual `void*` casting. More lines of code per strategy than modern C++.

### Rust Strategy

**File:** `rust/src/bin/strategy.rs`

Three approaches are shown:

**Enum dispatch** (closed set): A Rust `enum` with one variant per strategy. Pattern matching with `match` provides exhaustive dispatch — a hard compiler error if you miss a variant. `#[derive(Clone)]` gives deep value copies for free.

```rust
#[derive(Debug, Clone)]
enum ExecutionStrategy {
    Twap { slices: u32 },
    Vwap { participation_rate: f64 },
    Iceberg { visible_qty: u32 },
}

let order2 = order.clone();  // deep copy, trivial
```

**Trait objects** (open set): A trait `ExecutionStrategyTrait` with `execute()` and `name()`. Concrete types implement the trait. `Box<dyn ExecutionStrategyTrait>` provides dynamic dispatch via a fat pointer (data pointer + vtable pointer). The `clone_box` pattern enables cloning of trait objects.

**Closures** (zero boilerplate): For simple strategies, a function returning a closure is the most idiomatic Rust approach. Zero ceremony — no struct, no trait impl, no enum variant.

```rust
fn twap_closure(slices: u32) -> impl Fn(&str, u32, f64) {
    move |symbol, qty, price| { /* ... */ }
}
```

**Pros:** Enum approach is zero-overhead with compiler-enforced exhaustiveness. `#[derive(Clone)]` eliminates all manual clone boilerplate. Trait objects provide open extension without inheritance (traits are implemented *for* types externally). Closures eliminate all ceremony for simple cases. The ownership system prevents strategies from holding dangling references.

**Cons:** Trait object cloning requires the `clone_box` workaround (or the `dyn-clone` crate). Enum approach requires recompiling when adding new variants. Closures can't be inspected, cloned, or serialized.

### Strategy Comparison

| Aspect | Classic C++ | Modern C++ | Pure C | Rust (enum) | Rust (trait) |
|--------|------------|------------|--------|-------------|--------------|
| Polymorphism | vtable | type erasure | fn pointers / switch | match | trait vtable |
| Storage | Heap | Heap/SBO | Stack | Stack | Heap |
| Copyable? | No | Yes | Yes | Yes (`Clone`) | Yes (`clone_box`) |
| Type coupling | Inheritance | None | None | None | Trait impl |
| Boilerplate/strategy | ~20 lines | ~8 lines | ~15 lines | ~0 (variant) | ~15 lines |
| Lifetime safety | Manual | Manual | Manual | Compiler-enforced | Compiler-enforced |

---

## The Visitor Pattern

### What Visitor Is

**Intent:** Define new operations on a set of types without modifying those types. Separate the algorithm from the object structure it operates on.

**Our domain:** A portfolio of financial instruments (bonds, interest rate swaps, options) where multiple operations — pricing, risk calculation, regulatory capital charges — need to be performed across all instruments. New operations get added frequently; the instrument types are relatively stable.

**Why it matters:** In finance, the same instrument needs to be viewed through many lenses: mark-to-market pricing, sensitivity analysis (DV01, delta), regulatory reporting (Basel III/IV capital charges), accounting treatment, and more. Each of these is an operation that cross-cuts all instrument types. The visitor pattern lets you add new operations without touching the instrument definitions.

**The expression problem:** The Visitor pattern highlights a fundamental tension. If you organize code by type (classic OOP), adding new operations is hard but adding new types is easy. If you organize code by operation (Visitor), adding new types is hard but adding new operations is easy. Every approach in every language makes this tradeoff — they just do it with different syntax and different safety guarantees.

### Classic C++ Visitor

**File:** `visitor/classic/main.cpp`

The classic approach uses double dispatch. Each instrument class (`Bond`, `Swap`, `Option`) inherits from `Instrument` and implements `accept(InstrumentVisitor& v)`, which calls `v.visit(*this)`. The `InstrumentVisitor` base class has a `visit()` overload for each concrete instrument type. Concrete visitors (`PricingVisitor`, `RiskVisitor`, `RegulatoryVisitor`) inherit from `InstrumentVisitor` and implement each overload.

```cpp
class InstrumentVisitor {
public:
    virtual void visit(const Bond& b) = 0;
    virtual void visit(const Swap& s) = 0;
    virtual void visit(const Option& o) = 0;
};

class Instrument {
public:
    virtual void accept(InstrumentVisitor& v) const = 0;
};

// Every instrument must implement accept():
void Bond::accept(InstrumentVisitor& v) const { v.visit(*this); }
```

The portfolio is a `vector<unique_ptr<Instrument>>` — heap-allocated, pointer-chasing, non-copyable.

**Pros:** Adding a new operation (e.g., accounting treatment) just requires a new visitor class. Well-documented pattern with decades of usage.

**Cons:** Double dispatch (`accept` → `visit`) is an indirection that's confusing to read and debug. Adding a new instrument type requires modifying the `InstrumentVisitor` interface and every concrete visitor — shotgun surgery. The portfolio is non-copyable. Every instrument is heap-allocated with scattered memory layout. Getter methods are required to access instrument data from visitors. Forgetting to handle a new instrument type in a visitor may compile silently and crash at runtime.

### Modern C++ Visitor

**File:** `visitor/modern/main.cpp`

The modern approach replaces the entire double-dispatch mechanism with `std::variant` + `std::visit`. Instruments are plain structs. The variant type alias *is* the polymorphic container. Operations are callable structs or overloaded lambda sets.

```cpp
using Instrument = std::variant<Bond, Swap, Option>;

// Callable struct for operations that return values:
struct PriceCalculator {
    double operator()(const Bond& b) const { /* ... */ }
    double operator()(const Swap& s) const { /* ... */ }
    double operator()(const Option& o) const { /* ... */ }
};

double px = std::visit(PriceCalculator{}, instrument);

// Overloaded lambdas for inline visitors:
std::visit(overloaded{
    [](const Bond& b)   { /* risk calc */ },
    [](const Swap& s)   { /* risk calc */ },
    [](const Option& o) { /* risk calc */ },
}, instrument);
```

The portfolio is `vector<Instrument>` — contiguous memory, fully copyable.

**Pros:** No double dispatch indirection. Instruments are plain structs with public members — direct access, no getters. The portfolio stores values contiguously (cache-friendly). Fully copyable with a single `auto snapshot = portfolio;`. The compiler enforces exhaustive handling — if you add a new variant and forget to handle it, the code won't compile. Two visitor styles: callable structs (for return values) and overloaded lambdas (for inline operations).

**Cons:** The type set is closed — adding a new instrument type requires modifying the variant typedef and all visitors. But the compiler enforces this, which is a significant improvement over the classic approach where it's easy to miss one.

### Pure C Visitor

**File:** `c/visitor.c`

Tagged union approach. Instruments are a struct with an enum tag and a union of data structs. "Visitors" are plain functions that switch on the tag. A generic `visit_portfolio()` function takes a function pointer to apply any visitor across the portfolio array.

```c
typedef struct {
    InstrumentType type;
    union {
        Bond bond;
        Swap swap;
        Option option;
    } data;
} Instrument;

void visit_pricing(const Instrument *inst) {
    switch (inst->type) {
        case INST_BOND:   /* price bond */   break;
        case INST_SWAP:   /* price swap */   break;
        case INST_OPTION: /* price option */ break;
    }
}

// Apply any visitor across the portfolio:
typedef void (*VisitorFn)(const Instrument *);
void visit_portfolio(const Instrument *portfolio, size_t count, VisitorFn visitor);
```

The portfolio is a stack-allocated array — zero heap allocation, trivially copyable with `memcpy`.

**Pros:** The simplest implementation of all four languages. No indirection, no templates, no traits. Adding a new operation is just writing a new function. The portfolio lives entirely on the stack. Value semantics by default. The `visit_portfolio` function with a function pointer shows that first-class visitors are natural in C.

**Cons:** No compile-time exhaustiveness guarantee (only `-Wswitch`). No type safety on the union access. Fixed-size portfolio array (or you manage dynamic allocation yourself).

### Rust Visitor

**File:** `rust/src/bin/visitor.rs`

Rust makes the classic Visitor pattern almost entirely unnecessary. The `enum` + `match` combination does everything the pattern was designed to do, but as a language feature rather than a design pattern.

```rust
#[derive(Debug, Clone)]
enum Instrument {
    Bond(Bond),
    Swap(Swap),
    Option(Option),
}

fn price(inst: &Instrument) -> f64 {
    match inst {
        Instrument::Bond(b) => { /* price bond with direct field access */ },
        Instrument::Swap(s) => { /* price swap */ },
        Instrument::Option(o) => { /* price option */ },
    }
}
```

Adding a new instrument variant (e.g., `Instrument::FRA(FRA)`) and forgetting to handle it in *any* `match` produces a hard compiler error — not a warning, not a runtime crash, not a template error novel.

The file also demonstrates a trait-based visitor for cases where you want to pass different operations as first-class values:

```rust
trait InstrumentVisitor {
    fn visit_bond(&self, b: &Bond);
    fn visit_swap(&self, s: &Swap);
    fn visit_option(&self, o: &Option);
}

fn visit(inst: &Instrument, visitor: &dyn InstrumentVisitor) {
    match inst { /* dispatch to visitor methods */ }
}
```

This replaces the entire `accept()`/double-dispatch mechanism with a single `match` in a free function.

**Pros:** Exhaustive matching is a hard compiler error. `#[derive(Clone)]` gives deep copies for free. Pattern matching destructures directly into the inner data — no getters needed. The `Display` trait provides formatted output naturally. The trait-based visitor is cleaner than C++'s double dispatch because there's only one dispatch point (the `match`), not two (`accept` → `visit`).

**Cons:** Same expression problem tradeoff — adding new variants requires updating all match arms. But the compiler enforces it, which is the critical difference.

### Visitor Comparison

| Aspect | Classic C++ | Modern C++ | Pure C | Rust |
|--------|------------|------------|--------|------|
| Mechanism | Double dispatch | `std::visit` | switch | `match` |
| Element storage | Heap (`unique_ptr`) | Stack (variant) | Stack (tagged union) | Stack (enum) |
| Portfolio copyable? | No | Yes | Yes (`memcpy`) | Yes (`Clone`) |
| Adding new operation | New visitor class | New callable/lambdas | New function | New function |
| Adding new type | Modify interface + all visitors | Modify variant + all visitors | Modify enum + all switches | Modify enum + all matches |
| Exhaustiveness check | None (runtime crash) | Compile error | `-Wswitch` warning | Hard compiler error |
| Memory layout | Scattered (pointers) | Contiguous | Contiguous | Contiguous |
| Data access | Through getters | Direct (public members) | Direct (union members) | Direct (destructuring) |

---

## The Command Pattern

### What Command Is

**Intent:** Encapsulate a request as an object, allowing you to parameterize clients with different requests, queue or log requests, and support undoable operations.

**Our domain:** A trade management system where buy and sell actions are recorded as command objects. The system supports undo (cancel a trade), redo (reinstate a cancelled trade), and full audit trail snapshots.

**Why it matters:** Trade systems require complete audit trails for regulatory compliance. Every action must be reversible for error correction. The command pattern gives you this naturally: each trade is an object that knows how to execute itself and how to reverse itself, and the history is a sequence of these objects. The ability to snapshot the history is critical for auditing, replay, and disaster recovery.

### Classic C++ Command

**File:** `command/classic/main.cpp`

The classic approach defines a `TradeCommand` abstract base class with virtual `execute()`, `undo()`, and `description()` methods. Concrete commands (`BuyCommand`, `SellCommand`) inherit from it and hold a reference to the `Portfolio` receiver. The `CommandHistory` manages stacks of `unique_ptr<TradeCommand>` for undo and redo.

```cpp
class BuyCommand : public TradeCommand {
    Portfolio& portfolio_;  // raw reference — lifetime risk!
    std::string symbol_;
    int quantity_;
    double price_;
public:
    void execute() override { portfolio_.buy(symbol_, quantity_, price_); }
    void undo() override { portfolio_.cancel(symbol_, quantity_, price_, true); }
};
```

**Pros:** Clear separation between command creation and execution. Undo/redo is straightforward with two stacks.

**Cons:** Every command is heap-allocated. The history is non-copyable — you cannot snapshot the audit trail without manually cloning every command. Each command holds a raw reference to the portfolio, creating lifetime risk (if the portfolio is moved or destroyed, all commands dangle). Boilerplate-heavy — `BuyCommand` and `SellCommand` are nearly identical classes with the same fields and structure. The reference-based design makes commands non-serializable and non-thread-safe.

### Modern C++ Command

**File:** `command/modern/main.cpp`

Two modern approaches are shown:

**Variant commands** (closed set): `TradeAction` is a `std::variant<BuyTrade, SellTrade>` where each alternative is a plain data struct. Execute and undo are free functions that take the variant and a portfolio reference. The history is `vector<TradeAction>` — fully copyable, snapshottable.

```cpp
struct BuyTrade { std::string symbol; int quantity; double price; };
struct SellTrade { std::string symbol; int quantity; double price; };

using TradeAction = std::variant<BuyTrade, SellTrade>;

void execute_action(const TradeAction& action, Portfolio& p) {
    std::visit(overloaded{
        [&](const BuyTrade& t)  { p.buy(t.symbol, t.quantity, t.price); },
        [&](const SellTrade& t) { p.sell(t.symbol, t.quantity, t.price); },
    }, action);
}

auto snapshot = history;  // deep copy of entire audit trail
```

**Type-erased commands** (open set): A `Command` wrapper using the Concept/Model pattern that accepts any type with `execute(Portfolio&)`, `undo(Portfolio&)`, and `description()`. New command types can be added without touching existing code. Still copyable through clone.

```cpp
struct MarketBuy {
    std::string symbol; int quantity; double price;
    void execute(Portfolio& p) const { p.buy(symbol, quantity, price); }
    void undo(Portfolio& p) const { p.reverse_buy(symbol, quantity, price); }
    std::string description() const;
};

Command cmd = MarketBuy{"TSLA", 200, 175.00};  // type-erased, copyable
```

**Key difference from classic:** Commands are pure data. They don't hold references to the portfolio — the portfolio is passed explicitly to execute/undo. This eliminates the dangling reference problem and makes commands naturally serializable and copyable.

### Pure C Command

**File:** `c/command.c`

Two approaches mirroring the C++ modern style:

**Tagged union commands** (closed set): A `TradeCommand` struct with an enum tag, symbol, quantity, and price. Execute and undo are switch-based free functions. The `TradeHistory` is a fixed-size array of commands — lives entirely on the stack, snapshottable with a single struct assignment.

```c
TradeHistory snapshot = history;  // entire audit trail copied by value
printf("sizeof(TradeHistory) = %zu bytes (all on stack)\n", sizeof(TradeHistory));
```

**Function pointer table** (open set): Each `ErasedCommand` carries function pointers for execute, undo, and describe, plus an inline storage buffer. New command types just provide new function implementations.

**Pros:** The history snapshot is a single assignment — no clone methods, no heap, no ceremony. The tagged union approach is ideal for event sourcing: commands are pure data that can be trivially serialized and replayed.

**Cons:** Fixed-size arrays mean a maximum history length. No type safety on union access.

### Rust Command

**File:** `rust/src/bin/command.rs`

Two approaches:

**Enum commands** (closed set): `TradeAction` is an enum with `Buy` and `Sell` variants. `#[derive(Clone)]` on the history struct gives snapshots for free. The history is `Vec<TradeAction>` — dynamically sized, fully cloneable.

```rust
#[derive(Debug, Clone)]
enum TradeAction {
    Buy { symbol: String, quantity: i32, price: f64 },
    Sell { symbol: String, quantity: i32, price: f64 },
}

let snapshot = history.clone();  // entire audit trail, deep copy
```

**Trait object commands** (open set): A `Command` trait with `execute(&self, &mut Portfolio)`, `undo(&self, &mut Portfolio)`, `description(&self)`, and `clone_box(&self)`. Any struct implementing `Command` can be used with `Box<dyn Command>`.

**Rust's unique advantage — ownership:** This is where Rust's design shines brightest. Commands *cannot* hold references to the portfolio because the borrow checker prevents it. The portfolio is mutated by `execute()`/`undo()`, which requires `&mut Portfolio`. You can't have a stored `&Portfolio` reference while also passing `&mut Portfolio` — the compiler rejects this at compile time.

```rust
// This design is impossible in Rust:
// struct BuyCommand { portfolio: &mut Portfolio }  // borrow checker: NO.

// Instead, the portfolio is passed explicitly:
fn execute(&self, portfolio: &mut Portfolio);
fn undo(&self, portfolio: &mut Portfolio);
```

This forced design has cascading benefits: commands are pure data, trivially `Clone`-able, trivially serializable (with serde), and trivially sendable across threads. The `Vec<TradeAction>` history can be cloned, serialized to JSON, replayed on a different machine, or sent to another thread — all of which are unsafe or impossible with the classic C++ approach.

### Command Comparison

| Aspect | Classic C++ | Modern C++ (variant) | Modern C++ (erased) | Pure C (tagged) | Pure C (erased) | Rust (enum) | Rust (trait) |
|--------|------------|---------------------|---------------------|----------------|----------------|-------------|--------------|
| Storage | Heap | Stack | Heap (pimpl) | Stack | Stack (inline) | Stack | Heap |
| History copyable? | No | Yes | Yes | Yes | Yes | Yes (`Clone`) | Yes (`clone_box`) |
| Snapshot | Impossible | O(n) copy | O(n) clone | Struct assign | Struct assign | `.clone()` | `.clone()` |
| Holds reference? | Yes (danger) | No | No | No | No | No (enforced) | No (enforced) |
| Serializable? | Difficult | Possible | Possible | Easy | Manual | Easy (serde) | Requires work |
| Thread-safe? | No (shared ref) | Yes (value copy) | Yes (value copy) | Yes (value copy) | Yes (value copy) | Yes (enforced) | Yes (enforced) |

---

## Key Concepts Explained

### Value Semantics vs Reference Semantics

**Value semantics** means that when you copy an object, you get a fully independent copy. Modifying one never affects the other — just like assigning `int b = a;` gives you two independent integers.

**Reference semantics** means objects share state through pointers or references. Changes through one alias are visible through another.

Classic C++ OOP uses reference semantics: a `unique_ptr<Strategy>` owns the strategy on the heap, and you can't copy it. Modern C++ and the other approaches aim for value semantics: `Order order2 = order;` gives you an independent copy with its own strategy.

### std::variant and Tagged Unions

`std::variant<T1, T2, T3>` (C++17) is a type-safe tagged union. It holds exactly one value from a closed set of types at any time. You access it through `std::visit`, which dispatches to the correct handler based on which type is currently held.

Key properties: stack-allocated (no heap), contiguous in vectors (cache-friendly), value-semantic (copies are deep), and the compiler enforces exhaustive handling through `std::visit`.

The pure C equivalent is a struct with an enum tag and a union. The Rust equivalent is an `enum` with data-carrying variants.

### Type Erasure

Type erasure hides a concrete type behind a uniform, non-templated interface. The canonical C++ standard library examples are `std::function` and `std::any`. The internal structure uses three components:

**Concept** — a private abstract base class defining the virtual interface (hidden implementation detail). **Model** — a private template class wrapping any concrete type `T` that forwards to its methods. **Pimpl** — a `unique_ptr<Concept>` that stores the erased type.

The C equivalent is a function pointer table with an inline storage buffer. The Rust equivalent is a trait object (`Box<dyn Trait>`) where the vtable is a language-level construct.

### The Overloaded Lambda Pattern

A utility that lets you define inline visitors for `std::variant` using lambdas:

```cpp
template <typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

std::visit(overloaded{
    [](const Bond& b)   { /* handle bond */ },
    [](const Swap& s)   { /* handle swap */ },
    [](const Option& o) { /* handle option */ },
}, instrument);
```

This aggregates multiple lambdas into a single callable with overloaded `operator()`. It's the C++ equivalent of pattern matching.

### Rust Enums and Pattern Matching

Rust's `enum` is not a C-style list of integer constants. It's a first-class algebraic data type — a tagged union where each variant can carry different data. Pattern matching with `match` is exhaustive: if you miss a variant, the compiler emits a hard error (not a warning). This gives you the same dispatch as `std::visit` but as a language primitive with better diagnostics.

### Rust Trait Objects

When you need an open set of types, Rust uses trait objects. A `Box<dyn Trait>` is a fat pointer (data pointer + vtable pointer). Unlike C++ inheritance, traits are implemented *for* types externally — the concrete type doesn't need to know it's being used polymorphically. This is "external polymorphism" as a language feature.

### Rust Ownership and the Borrow Checker

Rust's ownership system enforces at compile time that every value has exactly one owner, references cannot outlive the data they point to, and you cannot have mutable and immutable references simultaneously. This prevents dangling references, use-after-free, and data races. In the Command pattern, this forces commands to be pure data rather than holding references to the receiver — a design that turns out to be better in every measurable way (copyable, serializable, thread-safe).

---

## When to Use What

**Use enums / `std::variant` / tagged unions when** you know all the types at compile time and the set changes rarely. This is the most performant option: no heap allocation, contiguous memory layout, and the compiler enforces exhaustive handling. Ideal for message types in a protocol, order types in a matching engine, or instrument types in a pricing library. This should be your default choice.

**Use type erasure / trait objects / function pointer tables when** the type set is open — users of your library need to add their own types without modifying your code. This is the right choice for plugin systems, callback registries, or any API where you don't control all the implementations.

**Use closures / lambdas when** the strategy is essentially a function with captured parameters and you don't need to inspect, clone, or serialize it.

**Use classic C++ inheritance when** you're working in a codebase that already uses it extensively and consistency matters more than optimality.

**Use C when** you're writing performance-critical systems code, working in embedded or kernel contexts, need ABI stability, or prefer zero hidden costs.

**Use Rust when** you want the performance characteristics of C with compile-time guarantees that eliminate memory safety bugs.

---

## Building the Project

### Prerequisites

| Tool | Required For | Minimum Version |
|------|-------------|-----------------|
| CMake | C and C++ builds | 3.20+ |
| GCC *or* Clang | C and C++ compilation | GCC 10+ / Clang 12+ |
| Rust (cargo) | Rust builds | 1.70+ |

You don't need all of them — if you only want to build the C/C++ examples, you don't need Rust, and vice versa.

### Using build.sh (Recommended)

The project includes a unified build script that handles C, C++, and Rust builds with a consistent interface. Make it executable first:

```bash
chmod +x build.sh
```

#### Commands

| Command | Description |
|---------|-------------|
| `build` | Compile selected targets (default if no command given) |
| `run` | Build and then run selected targets |
| `clean` | Remove build artifacts |
| `help` | Show full usage information |

#### Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `--lang` | `c`, `cpp`, `rust`, `all` | `all` | Which language(s) to build/run |
| `--debug` | — | — | Build with debug symbols, no optimization |
| `--release` | — | — | Build optimized (this is the default) |
| `--compiler` | `gcc`, `clang` | `gcc` | C/C++ compiler toolchain (ignored for Rust) |
| `--target` | binary name | — | Build and run a specific binary only |
| `--verbose` | — | — | Show full compiler commands |

#### Available Targets

| Language | Targets |
|----------|---------|
| C | `c_strategy`, `c_visitor`, `c_command` |
| C++ | `strategy_classic`, `strategy_modern`, `visitor_classic`, `visitor_modern`, `command_classic`, `command_modern` |
| Rust | `strategy`, `visitor`, `command` |

#### Examples

```bash
# Build everything (all languages, gcc, release)
./build.sh
./build.sh build

# Build only C files with GCC in release mode
./build.sh build --lang c

# Build only C++ files with Clang in debug mode
./build.sh build --lang cpp --compiler clang --debug

# Build only Rust in release mode
./build.sh build --lang rust

# Build and run all C++ examples
./build.sh run --lang cpp

# Build and run all C examples in debug with Clang
./build.sh run --lang c --compiler clang --debug

# Build and run a single specific target
./build.sh run --target strategy_modern
./build.sh run --target c_command --debug
./build.sh run --target visitor     # Rust binary

# Clean all build artifacts
./build.sh clean

# Clean only Rust build artifacts
./build.sh clean --lang rust

# Clean only C/C++ build artifacts
./build.sh clean --lang c
./build.sh clean --lang cpp

# See full help
./build.sh help
```

#### Build Directories

The script namespaces build directories by compiler and mode:

```
build/
├── gcc-release/     # ./build.sh build
├── gcc-debug/       # ./build.sh build --debug
├── clang-release/   # ./build.sh build --compiler clang
└── clang-debug/     # ./build.sh build --compiler clang --debug
```

This means you can have multiple builds coexisting — useful for comparing GCC vs Clang output or examining debug vs release behavior side by side.

The script also symlinks `compile_commands.json` to the project root for LSP integration (clangd, ccls).

### Manual Build

If you prefer not to use the build script, here's how to build everything by hand.

#### C and C++ (CMake)

```bash
# Create a build directory and configure
mkdir build && cd build

# Default (GCC, no specific build type):
cmake ..

# Or specify compiler and build type explicitly:
cmake .. -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release
cmake .. -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug

# Build all 9 targets
make -j$(nproc)

# Or build specific targets
make strategy_classic strategy_modern    # just C++ strategy
make c_strategy c_visitor c_command      # just C

# Run from the build directory
./strategy_classic
./strategy_modern
./visitor_classic
./visitor_modern
./command_classic
./command_modern
./c_strategy
./c_visitor
./c_command
```

#### Rust (Cargo)

```bash
cd rust

# Build all 3 binaries in release mode
cargo build --release

# Build in debug mode
cargo build

# Run a specific binary
cargo run --release --bin strategy
cargo run --release --bin visitor
cargo run --release --bin command

# Or run the compiled binaries directly
./target/release/strategy
./target/release/visitor
./target/release/command

# Debug versions
./target/debug/strategy
./target/debug/visitor
./target/debug/command

# Clean Rust build artifacts
cargo clean
```

#### Running Everything Manually

From the project root, after building both C/C++ and Rust:

```bash
# C++ Classic
./build/strategy_classic
./build/visitor_classic
./build/command_classic

# C++ Modern
./build/strategy_modern
./build/visitor_modern
./build/command_modern

# Pure C
./build/c_strategy
./build/c_visitor
./build/c_command

# Rust
./rust/target/release/strategy
./rust/target/release/visitor
./rust/target/release/command
```

Each binary is self-contained and prints formatted output showing the pattern in action with the finance domain examples.

---

## Project Structure

```
design-patterns-modern/
├── build.sh                    # Unified build script (build/run/clean/help)
├── CMakeLists.txt              # CMake config for all 9 C/C++ targets
├── README.md
├── strategy/
│   ├── classic/
│   │   └── main.cpp            # Inheritance + unique_ptr<Strategy>
│   └── modern/
│       └── main.cpp            # Type erasure, copyable Order
├── visitor/
│   ├── classic/
│   │   └── main.cpp            # Double dispatch, accept/visit
│   └── modern/
│       └── main.cpp            # std::variant + std::visit + overloaded lambdas
├── command/
│   ├── classic/
│   │   └── main.cpp            # Virtual execute/undo, non-copyable history
│   └── modern/
│       └── main.cpp            # Variant commands + type-erased commands
├── c/
│   ├── strategy.c              # Tagged union + function pointer table
│   ├── visitor.c               # Tagged union + switch dispatch
│   └── command.c               # Tagged union + function pointer table
└── rust/
    ├── Cargo.toml              # Cargo config for 3 Rust binaries
    └── src/bin/
        ├── strategy.rs         # Enum dispatch + trait objects + closures
        ├── visitor.rs          # Enum + match (visitor pattern unnecessary)
        └── command.rs          # Enum commands + trait object commands
```
