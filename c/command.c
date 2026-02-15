#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================
 * Command Pattern in Pure C — Trade Management with Undo/Redo
 *
 * Two approaches shown:
 *
 * 1. Tagged Union (closed set of commands)
 *    - Commands are pure data — no behavior embedded
 *    - Execute/undo are free functions that switch on the tag
 *    - History is a plain array — trivially copyable/snapshottable
 *
 * 2. Function Pointer Table (open extension)
 *    - Each command carries its own execute/undo function pointers
 *    - New command types can be added without modifying existing code
 *    - Still value-semantic with inline storage
 *
 * In both cases, the command history is fully copyable.
 * ============================================================ */

/* --- Receiver: Portfolio --- */

#define MAX_POSITIONS 32

typedef struct {
    char symbols[MAX_POSITIONS][16];
    int quantities[MAX_POSITIONS];
    int count;
    double cash;
} Portfolio;

static void portfolio_init(Portfolio *p, double cash) {
    memset(p, 0, sizeof(*p));
    p->cash = cash;
}

static int portfolio_find(Portfolio *p, const char *symbol) {
    for (int i = 0; i < p->count; ++i) {
        if (strcmp(p->symbols[i], symbol) == 0) return i;
    }
    return -1;
}

static void portfolio_adjust(Portfolio *p, const char *symbol, int qty_delta,
                              double cash_delta) {
    int idx = portfolio_find(p, symbol);
    if (idx < 0) {
        idx = p->count++;
        strncpy(p->symbols[idx], symbol, 15);
        p->symbols[idx][15] = '\0';
        p->quantities[idx] = 0;
    }
    p->quantities[idx] += qty_delta;
    p->cash += cash_delta;
}

static void portfolio_buy(Portfolio *p, const char *symbol, int qty, double price) {
    portfolio_adjust(p, symbol, qty, -(qty * price));
    printf("  [EXEC] BUY  %d %s @ $%.2f  (cash: $%.2f)\n",
           qty, symbol, price, p->cash);
}

static void portfolio_sell(Portfolio *p, const char *symbol, int qty, double price) {
    portfolio_adjust(p, symbol, -qty, qty * price);
    printf("  [EXEC] SELL %d %s @ $%.2f  (cash: $%.2f)\n",
           qty, symbol, price, p->cash);
}

static void portfolio_reverse_buy(Portfolio *p, const char *symbol,
                                    int qty, double price) {
    portfolio_adjust(p, symbol, -qty, qty * price);
    printf("  [UNDO] BUY  %d %s @ $%.2f reversed  (cash: $%.2f)\n",
           qty, symbol, price, p->cash);
}

static void portfolio_reverse_sell(Portfolio *p, const char *symbol,
                                     int qty, double price) {
    portfolio_adjust(p, symbol, qty, -(qty * price));
    printf("  [UNDO] SELL %d %s @ $%.2f reversed  (cash: $%.2f)\n",
           qty, symbol, price, p->cash);
}

static void portfolio_print(const Portfolio *p) {
    puts("  Portfolio:");
    printf("    Cash: $%.2f\n", p->cash);
    for (int i = 0; i < p->count; ++i) {
        if (p->quantities[i] != 0)
            printf("    %s: %d shares\n", p->symbols[i], p->quantities[i]);
    }
}

/* ============================================================
 * APPROACH 1: Tagged Union Commands
 * ============================================================ */

typedef enum {
    CMD_BUY,
    CMD_SELL
} TradeCommandType;

typedef struct {
    TradeCommandType type;
    char symbol[16];
    int quantity;
    double price;
} TradeCommand;

static TradeCommand cmd_buy(const char *symbol, int qty, double price) {
    TradeCommand cmd;
    cmd.type = CMD_BUY;
    strncpy(cmd.symbol, symbol, 15);
    cmd.symbol[15] = '\0';
    cmd.quantity = qty;
    cmd.price = price;
    return cmd;
}

static TradeCommand cmd_sell(const char *symbol, int qty, double price) {
    TradeCommand cmd;
    cmd.type = CMD_SELL;
    strncpy(cmd.symbol, symbol, 15);
    cmd.symbol[15] = '\0';
    cmd.quantity = qty;
    cmd.price = price;
    return cmd;
}

static void cmd_execute(const TradeCommand *cmd, Portfolio *p) {
    switch (cmd->type) {
    case CMD_BUY:  portfolio_buy(p, cmd->symbol, cmd->quantity, cmd->price);  break;
    case CMD_SELL: portfolio_sell(p, cmd->symbol, cmd->quantity, cmd->price); break;
    }
}

static void cmd_undo(const TradeCommand *cmd, Portfolio *p) {
    switch (cmd->type) {
    case CMD_BUY:  portfolio_reverse_buy(p, cmd->symbol, cmd->quantity, cmd->price);  break;
    case CMD_SELL: portfolio_reverse_sell(p, cmd->symbol, cmd->quantity, cmd->price); break;
    }
}

static void cmd_describe(const TradeCommand *cmd, char *buf, size_t len) {
    snprintf(buf, len, "%s %d %s @ $%.2f",
             cmd->type == CMD_BUY ? "BUY" : "SELL",
             cmd->quantity, cmd->symbol, cmd->price);
}

/* --- Command History: plain array of values --- */

#define MAX_HISTORY 64

typedef struct {
    TradeCommand executed[MAX_HISTORY];
    int exec_count;
    TradeCommand undone[MAX_HISTORY];
    int undo_count;
} TradeHistory;

static void history_init(TradeHistory *h) {
    h->exec_count = 0;
    h->undo_count = 0;
}

static void history_execute(TradeHistory *h, TradeCommand cmd, Portfolio *p) {
    cmd_execute(&cmd, p);
    h->executed[h->exec_count++] = cmd;  /* value copy */
    h->undo_count = 0;  /* clear redo stack */
}

static int history_undo(TradeHistory *h, Portfolio *p) {
    if (h->exec_count == 0) return 0;
    TradeCommand cmd = h->executed[--h->exec_count];
    cmd_undo(&cmd, p);
    h->undone[h->undo_count++] = cmd;
    return 1;
}

static int history_redo(TradeHistory *h, Portfolio *p) {
    if (h->undo_count == 0) return 0;
    TradeCommand cmd = h->undone[--h->undo_count];
    cmd_execute(&cmd, p);
    h->executed[h->exec_count++] = cmd;
    return 1;
}

static void history_print(const TradeHistory *h) {
    char buf[128];
    puts("  Trade History:");
    for (int i = 0; i < h->exec_count; ++i) {
        cmd_describe(&h->executed[i], buf, sizeof(buf));
        printf("    %d. %s\n", i + 1, buf);
    }
    if (h->exec_count == 0) puts("    (empty)");
}

/* ============================================================
 * APPROACH 2: Function Pointer Table (Type Erasure)
 * ============================================================ */

#define ERASED_CMD_STORAGE 48

typedef struct {
    void (*execute)(const void *self, Portfolio *p);
    void (*undo)(const void *self, Portfolio *p);
    void (*describe)(const void *self, char *buf, size_t len);
    char storage[ERASED_CMD_STORAGE];
} ErasedCommand;

/* --- Market Buy command --- */

typedef struct {
    char symbol[16];
    int quantity;
    double price;
} MarketBuyData;

static void market_buy_exec(const void *self, Portfolio *p) {
    const MarketBuyData *d = (const MarketBuyData *)self;
    portfolio_buy(p, d->symbol, d->quantity, d->price);
}

static void market_buy_undo(const void *self, Portfolio *p) {
    const MarketBuyData *d = (const MarketBuyData *)self;
    portfolio_reverse_buy(p, d->symbol, d->quantity, d->price);
}

static void market_buy_describe(const void *self, char *buf, size_t len) {
    const MarketBuyData *d = (const MarketBuyData *)self;
    snprintf(buf, len, "MARKET BUY %d %s @ $%.2f", d->quantity, d->symbol, d->price);
}

static ErasedCommand erased_market_buy(const char *symbol, int qty, double price) {
    ErasedCommand cmd;
    cmd.execute = market_buy_exec;
    cmd.undo = market_buy_undo;
    cmd.describe = market_buy_describe;
    MarketBuyData *d = (MarketBuyData *)cmd.storage;
    strncpy(d->symbol, symbol, 15);
    d->symbol[15] = '\0';
    d->quantity = qty;
    d->price = price;
    return cmd;
}

/* --- Limit Sell command --- */

typedef struct {
    char symbol[16];
    int quantity;
    double limit_price;
} LimitSellData;

static void limit_sell_exec(const void *self, Portfolio *p) {
    const LimitSellData *d = (const LimitSellData *)self;
    portfolio_sell(p, d->symbol, d->quantity, d->limit_price);
}

static void limit_sell_undo(const void *self, Portfolio *p) {
    const LimitSellData *d = (const LimitSellData *)self;
    portfolio_reverse_sell(p, d->symbol, d->quantity, d->limit_price);
}

static void limit_sell_describe(const void *self, char *buf, size_t len) {
    const LimitSellData *d = (const LimitSellData *)self;
    snprintf(buf, len, "LIMIT SELL %d %s @ $%.2f",
             d->quantity, d->symbol, d->limit_price);
}

static ErasedCommand erased_limit_sell(const char *symbol, int qty, double price) {
    ErasedCommand cmd;
    cmd.execute = limit_sell_exec;
    cmd.undo = limit_sell_undo;
    cmd.describe = limit_sell_describe;
    LimitSellData *d = (LimitSellData *)cmd.storage;
    strncpy(d->symbol, symbol, 15);
    d->symbol[15] = '\0';
    d->quantity = qty;
    d->limit_price = price;
    return cmd;
}

/* ============================================================ */

int main(void) {
    puts("=== C Command Pattern: Trade Management ===");
    puts("========== Approach 1: Tagged Union ==========\n");

    Portfolio portfolio;
    portfolio_init(&portfolio, 1000000.0);

    TradeHistory history;
    history_init(&history);

    puts("--- Executing trades ---");
    history_execute(&history, cmd_buy("AAPL", 100, 185.50), &portfolio);
    history_execute(&history, cmd_buy("GOOGL", 50, 140.25), &portfolio);
    history_execute(&history, cmd_sell("MSFT", 75, 420.00), &portfolio);

    puts("");
    portfolio_print(&portfolio);
    puts("");
    history_print(&history);

    puts("\n--- Undo last trade ---");
    history_undo(&history, &portfolio);
    portfolio_print(&portfolio);

    puts("\n--- Undo another ---");
    history_undo(&history, &portfolio);
    portfolio_print(&portfolio);

    puts("\n--- Redo ---");
    history_redo(&history, &portfolio);
    portfolio_print(&portfolio);

    /* Snapshot: just copy the structs */
    puts("\n--- Snapshot history (plain struct copy!) ---");
    TradeHistory snapshot = history;  /* entire history copied by value */
    printf("  Snapshot has %d trades\n", snapshot.exec_count);

    /* Continue on original */
    history_execute(&history, cmd_sell("AAPL", 50, 190.00), &portfolio);

    puts("\n--- Original history ---");
    history_print(&history);

    puts("\n--- Snapshot unchanged ---");
    history_print(&snapshot);

    printf("\n  sizeof(TradeCommand) = %zu bytes\n", sizeof(TradeCommand));
    printf("  sizeof(TradeHistory) = %zu bytes (all on stack)\n", sizeof(TradeHistory));

    /* ============================================================ */
    puts("\n========== Approach 2: Function Pointer Table ==========\n");

    Portfolio portfolio2;
    portfolio_init(&portfolio2, 500000.0);

    ErasedCommand commands[2];
    commands[0] = erased_market_buy("TSLA", 200, 175.00);
    commands[1] = erased_limit_sell("NVDA", 30, 890.50);

    puts("--- Executing type-erased commands ---");
    for (int i = 0; i < 2; ++i) {
        commands[i].execute(commands[i].storage, &portfolio2);
    }

    puts("\n--- Undoing all ---");
    for (int i = 1; i >= 0; --i) {
        commands[i].undo(commands[i].storage, &portfolio2);
    }

    portfolio_print(&portfolio2);

    /* Copy erased commands */
    puts("\n--- Erased commands are copyable ---");
    ErasedCommand cmd_copy = commands[0];
    char buf[128];
    cmd_copy.describe(cmd_copy.storage, buf, sizeof(buf));
    printf("  Copied command: %s\n", buf);

    printf("\n  sizeof(ErasedCommand) = %zu bytes\n", sizeof(ErasedCommand));

    return 0;
}
