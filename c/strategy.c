#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================
 * Strategy Pattern in Pure C — Order Execution Strategies
 *
 * Two approaches shown:
 *
 * 1. Tagged Union (like std::variant)
 *    - Closed set of strategies known at compile time
 *    - Stack allocated, no heap, cache-friendly
 *    - Switch dispatch instead of virtual dispatch
 *
 * 2. Function Pointer Table (like type erasure)
 *    - Open set — anyone can create a new strategy
 *    - Strategy carries its own behavior via function pointers
 *    - Still value-semantic: copy the struct, get independent copy
 *
 * Both give you value semantics for free — C structs copy by value.
 * ============================================================ */

/* ============================================================
 * APPROACH 1: Tagged Union
 * ============================================================ */

typedef enum {
    STRATEGY_TWAP,
    STRATEGY_VWAP,
    STRATEGY_ICEBERG
} StrategyType;

typedef struct {
    int slices;
} TWAPParams;

typedef struct {
    double participation_rate;
} VWAPParams;

typedef struct {
    int visible_qty;
} IcebergParams;

/* The tagged union — C's variant */
typedef struct {
    StrategyType type;
    union {
        TWAPParams   twap;
        VWAPParams   vwap;
        IcebergParams iceberg;
    } params;
} ExecutionStrategy;

/* "Constructors" — return by value */
static ExecutionStrategy strategy_twap(int slices) {
    ExecutionStrategy s;
    s.type = STRATEGY_TWAP;
    s.params.twap.slices = slices;
    return s;
}

static ExecutionStrategy strategy_vwap(double rate) {
    ExecutionStrategy s;
    s.type = STRATEGY_VWAP;
    s.params.vwap.participation_rate = rate;
    return s;
}

static ExecutionStrategy strategy_iceberg(int visible) {
    ExecutionStrategy s;
    s.type = STRATEGY_ICEBERG;
    s.params.iceberg.visible_qty = visible;
    return s;
}

/* Dispatch via switch — the C equivalent of std::visit */
static void strategy_execute(const ExecutionStrategy *s,
                              const char *symbol, int qty, double price) {
    switch (s->type) {
    case STRATEGY_TWAP: {
        int per_slice = qty / s->params.twap.slices;
        printf("[TWAP] Executing %s: %d shares @ $%.2f across %d slices (%d/slice)\n",
               symbol, qty, price, s->params.twap.slices, per_slice);
        break;
    }
    case STRATEGY_VWAP:
        printf("[VWAP] Executing %s: %d shares @ $%.2f with %.0f%% participation\n",
               symbol, qty, price, s->params.vwap.participation_rate * 100);
        break;
    case STRATEGY_ICEBERG:
        printf("[Iceberg] Executing %s: %d shares @ $%.2f showing %d at a time\n",
               symbol, qty, price, s->params.iceberg.visible_qty);
        break;
    }
}

static const char* strategy_name(const ExecutionStrategy *s) {
    switch (s->type) {
    case STRATEGY_TWAP:    return "TWAP";
    case STRATEGY_VWAP:    return "VWAP";
    case STRATEGY_ICEBERG: return "Iceberg";
    }
    return "Unknown";
}

/* Order — value type, fully copyable */
typedef struct {
    char symbol[16];
    int quantity;
    double price;
    ExecutionStrategy strategy;  /* embedded value, not a pointer */
} Order;

static Order order_create(const char *symbol, int qty, double price,
                           ExecutionStrategy strategy) {
    Order o;
    strncpy(o.symbol, symbol, sizeof(o.symbol) - 1);
    o.symbol[sizeof(o.symbol) - 1] = '\0';
    o.quantity = qty;
    o.price = price;
    o.strategy = strategy;  /* value copy */
    return o;
}

static void order_send(const Order *o) {
    printf("Order: %s %d shares @ $%.2f using %s\n",
           o->symbol, o->quantity, o->price, strategy_name(&o->strategy));
    strategy_execute(&o->strategy, o->symbol, o->quantity, o->price);
}

/* ============================================================
 * APPROACH 2: Function Pointer Table (Type Erasure)
 * ============================================================ */

/* The "vtable" — but carried by each instance, not by a class */
typedef struct {
    void (*execute)(const void *self, const char *symbol, int qty, double price);
    const char* (*name)(const void *self);
} StrategyOps;

/*
 * Type-erased strategy: ops table + inline storage.
 * No heap allocation — the strategy data lives inside the struct.
 * 64 bytes is enough for any of our strategy params.
 */
#define STRATEGY_STORAGE_SIZE 64

typedef struct {
    StrategyOps ops;
    char storage[STRATEGY_STORAGE_SIZE];
} ErasedStrategy;

/* --- TWAP via function pointers --- */

typedef struct {
    int slices;
} TWAPData;

static void twap_execute(const void *self, const char *symbol, int qty, double price) {
    const TWAPData *d = (const TWAPData *)self;
    int per_slice = qty / d->slices;
    printf("[TWAP-erased] Executing %s: %d shares @ $%.2f across %d slices (%d/slice)\n",
           symbol, qty, price, d->slices, per_slice);
}

static const char* twap_name(const void *self) {
    (void)self;
    return "TWAP";
}

static ErasedStrategy erased_twap(int slices) {
    ErasedStrategy es;
    es.ops.execute = twap_execute;
    es.ops.name = twap_name;
    TWAPData *d = (TWAPData *)es.storage;
    d->slices = slices;
    return es;
}

/* --- VWAP via function pointers --- */

typedef struct {
    double participation_rate;
} VWAPData;

static void vwap_execute(const void *self, const char *symbol, int qty, double price) {
    const VWAPData *d = (const VWAPData *)self;
    printf("[VWAP-erased] Executing %s: %d shares @ $%.2f with %.0f%% participation\n",
           symbol, qty, price, d->participation_rate * 100);
}

static const char* vwap_name(const void *self) {
    (void)self;
    return "VWAP";
}

static ErasedStrategy erased_vwap(double rate) {
    ErasedStrategy es;
    es.ops.execute = vwap_execute;
    es.ops.name = vwap_name;
    VWAPData *d = (VWAPData *)es.storage;
    d->participation_rate = rate;
    return es;
}

/* --- Iceberg via function pointers --- */

typedef struct {
    int visible_qty;
} IcebergData;

static void iceberg_execute(const void *self, const char *symbol, int qty, double price) {
    const IcebergData *d = (const IcebergData *)self;
    printf("[Iceberg-erased] Executing %s: %d shares @ $%.2f showing %d at a time\n",
           symbol, qty, price, d->visible_qty);
}

static const char* iceberg_name(const void *self) {
    (void)self;
    return "Iceberg";
}

static ErasedStrategy erased_iceberg(int visible) {
    ErasedStrategy es;
    es.ops.execute = iceberg_execute;
    es.ops.name = iceberg_name;
    IcebergData *d = (IcebergData *)es.storage;
    d->visible_qty = visible;
    return es;
}

/* Dispatch through the function pointer table */
static void erased_execute(const ErasedStrategy *s,
                            const char *symbol, int qty, double price) {
    s->ops.execute(s->storage, symbol, qty, price);
}

static const char* erased_name(const ErasedStrategy *s) {
    return s->ops.name(s->storage);
}

/* Order using erased strategy */
typedef struct {
    char symbol[16];
    int quantity;
    double price;
    ErasedStrategy strategy;
} ErasedOrder;

static ErasedOrder erased_order_create(const char *symbol, int qty, double price,
                                        ErasedStrategy strategy) {
    ErasedOrder o;
    strncpy(o.symbol, symbol, sizeof(o.symbol) - 1);
    o.symbol[sizeof(o.symbol) - 1] = '\0';
    o.quantity = qty;
    o.price = price;
    o.strategy = strategy;  /* value copy — memcpy under the hood */
    return o;
}

static void erased_order_send(const ErasedOrder *o) {
    printf("Order: %s %d shares @ $%.2f using %s\n",
           o->symbol, o->quantity, o->price, erased_name(&o->strategy));
    erased_execute(&o->strategy, o->symbol, o->quantity, o->price);
}

/* ============================================================ */

int main(void) {
    puts("=== C Strategy Pattern: Order Execution ===");
    puts("========== Approach 1: Tagged Union ==========\n");

    Order order = order_create("AAPL", 10000, 185.50, strategy_twap(5));
    order_send(&order);

    puts("\n--- Switching to VWAP ---");
    order.strategy = strategy_vwap(0.15);  /* just assign a new value */
    order_send(&order);

    puts("\n--- Switching to Iceberg ---");
    order.strategy = strategy_iceberg(500);
    order_send(&order);

    /* Value semantics: copy the order, modify independently */
    puts("\n--- Copying order (value semantics for free!) ---");
    Order order2 = order;  /* plain struct copy */
    order2.strategy = strategy_twap(10);

    puts("Original:");
    order_send(&order);
    puts("Copy (independent):");
    order_send(&order2);

    puts("\n========== Approach 2: Function Pointer Table ==========\n");

    ErasedOrder eorder = erased_order_create("GOOGL", 5000, 140.25, erased_twap(8));
    erased_order_send(&eorder);

    puts("\n--- Switching to VWAP ---");
    eorder.strategy = erased_vwap(0.20);
    erased_order_send(&eorder);

    puts("\n--- Switching to Iceberg ---");
    eorder.strategy = erased_iceberg(200);
    erased_order_send(&eorder);

    /* Also copyable — the function pointers and storage copy together */
    puts("\n--- Copying erased order ---");
    ErasedOrder eorder2 = eorder;
    eorder2.strategy = erased_twap(3);

    puts("Original:");
    erased_order_send(&eorder);
    puts("Copy (independent):");
    erased_order_send(&eorder2);

    return 0;
}
