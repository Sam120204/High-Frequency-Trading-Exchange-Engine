# Architecture

## Processing Model

The system separates network I/O, matching, market-data publication, and client strategy logic into dedicated threads. Components exchange fixed message structures through bounded single-producer/single-consumer queues.

```text
OrderGateway -> TCP -> OrderServer -> ClientRequest queue -> MatchingEngine
                                                            |          |
                                            ClientResponse queue       MEMarketUpdate queue
                                                            |          |
                                                   OrderServer    MarketDataPublisher
                                                            |          |
                                                         TCP       UDP multicast
```

The matching engine owns one order book per ticker. Only the matching thread mutates these books, so book operations do not require locks. Each price level is a circular doubly linked list of orders in arrival order. Price levels are also maintained as a side-specific linked list from best to worst price.

## Order Lifecycle

1. A client assigns an order ID and sends a `NEW` request through the order gateway.
2. The order server assigns a transport sequence number and places the decoded request on the matching queue.
3. The matching engine emits `ACCEPTED`, then crosses the order against resting liquidity at the best available prices.
4. Every execution emits `FILLED` responses for both participants and a `TRADE` market update.
5. Any remaining quantity becomes a resting order and emits an `ADD`; a partial passive fill emits `MODIFY`.
6. A valid cancellation emits `CANCELED` and a market-data `CANCEL`. An unknown order emits `CANCEL_REJECTED`.

## Ownership And Allocation

- `MatchingEngine` owns all exchange-side order books.
- Each order book owns fixed-capacity pools for orders and price levels.
- Active client/order lookup is sparse, keyed by `(client_id, client_order_id)`, avoiding the previous multi-gigabyte fixed pointer matrix.
- Network servers and publishers own their sockets.
- Queue storage is allocated at construction; queue elements are reused.

The sparse lookup uses `std::unordered_map`, which may allocate when active orders are inserted. The map is reserved to the configured order capacity to reduce rehashing, but the current implementation therefore does not guarantee zero allocation on every hot-path insert.

## Ordering Guarantees

- The order server FIFO sequencer establishes the order delivered to the matching thread.
- One matching thread processes requests serially.
- Better prices execute before worse prices.
- At one price, resting orders execute in arrival order.
- Each outbound transport maintains a monotonically increasing sequence number.

## Backpressure And Failure Modes

Queues are bounded. `LFQueue::getNextToWriteTo()` returns `nullptr` when full, but several producers currently assume space is available. Under sustained overload this can terminate the process rather than reject or shed load cleanly. Production-quality overload handling would require an explicit policy, metrics, and tests.

The market-data consumer detects sequence discontinuities and uses snapshots to rebuild state, but the project does not yet provide redundant incremental feeds, request-based replay, persisted sequence state, or exchange restart recovery. The order path has no durable journal, so acknowledged state cannot be reconstructed after a process failure.

## Operational Boundaries

This simulator runs all services on loopback with fixed addresses and ports. It has no authentication, authorization, encryption, persistence, high-availability failover, or administrative control plane. Those omissions are intentional boundaries of an educational project and are documented rather than hidden behind a production-readiness claim.
