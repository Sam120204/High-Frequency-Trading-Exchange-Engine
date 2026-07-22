# Binary Protocol

The current order and market-data transports send packed C++ structures directly. This document records the structures used by the code; it is not a stable, language-neutral protocol specification.

## Encoding Caveats

- Integer fields use the host's native byte order.
- `size_t` is platform-dependent and is 64 bits on the supported Linux x86-64 build.
- Enum underlying widths are explicitly declared, but the protocol has no version field.
- Structures are packed to one-byte alignment.
- There is no checksum, authentication tag, schema negotiation, or compatibility handshake.

Consequently, all participants must currently use a compatible build, architecture, compiler ABI, and structure definitions. A production protocol should define fixed-width sequence fields, network byte order, versioning, framing, validation, and backward-compatibility rules.

## Order Request

`OMClientRequest` wraps a transport sequence number and `MEClientRequest`.

| Field | C++ type | Meaning |
| --- | --- | --- |
| `seq_num_` | `size_t` | Per-client order-stream sequence |
| `type_` | `uint8_t` enum | `INVALID=0`, `NEW=1`, `CANCEL=2` |
| `client_id_` | `uint32_t` | Client identity |
| `ticker_id_` | `uint32_t` | Instrument index |
| `order_id_` | `uint64_t` | Client-assigned order identity |
| `side_` | `int8_t` enum | `SELL=-1`, `INVALID=0`, `BUY=1` |
| `price_` | `int64_t` | Integer price units |
| `qty_` | `uint32_t` | Order quantity |

## Order Response

`OMClientResponse` wraps a sequence number and `MEClientResponse`.

| Field | C++ type | Meaning |
| --- | --- | --- |
| `seq_num_` | `size_t` | Per-client response-stream sequence |
| `type_` | `uint8_t` enum | `ACCEPTED=1`, `CANCELED=2`, `FILLED=3`, `CANCEL_REJECTED=4` |
| `client_id_` | `uint32_t` | Destination client |
| `ticker_id_` | `uint32_t` | Instrument index |
| `client_order_id_` | `uint64_t` | Client-assigned order identity |
| `market_order_id_` | `uint64_t` | Exchange-assigned order identity |
| `side_` | `int8_t` enum | Order side |
| `price_` | `int64_t` | Acceptance or execution price |
| `exec_qty_` | `uint32_t` | Quantity executed by this event |
| `leaves_qty_` | `uint32_t` | Quantity remaining |

## Market Data

`MDPMarketUpdate` wraps a publication sequence number and `MEMarketUpdate`.

| Field | C++ type | Meaning |
| --- | --- | --- |
| `seq_num_` | `size_t` | Market-data stream sequence |
| `type_` | `uint8_t` enum | `CLEAR`, `ADD`, `MODIFY`, `CANCEL`, `TRADE`, `SNAPSHOT_START`, or `SNAPSHOT_END` |
| `order_id_` | `uint64_t` | Exchange-assigned order identity |
| `ticker_id_` | `uint32_t` | Instrument index |
| `side_` | `int8_t` enum | Order/aggressor side |
| `price_` | `int64_t` | Integer price units |
| `qty_` | `uint32_t` | Order or execution quantity |
| `priority_` | `uint64_t` | FIFO priority within a price level |

Snapshot messages bracket reconstructed book state with `SNAPSHOT_START` and `SNAPSHOT_END`; incremental messages describe live changes. Consumers use sequence numbers to identify missing or out-of-order data.
