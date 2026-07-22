# High-Frequency Trading Exchange Engine

A C++20 exchange simulator built around a price-time-priority matching engine, single-producer/single-consumer queues, preallocated memory pools, a TCP order gateway, and UDP multicast market data. The repository includes both the exchange and sample trading clients.

> This is an educational systems project, not production trading software. The latency figures in `METRICS.json` are historical results from one run; they are not independently reproducible guarantees. Results depend heavily on the compiler, CPU, operating system, load, and measurement method.

## Architecture

```text
Trading client(s)
  Trade engine <-> Order gateway -- TCP :12345 --> Order server
       ^                                              |
       |                                              v
  Market-data consumer <-- UDP multicast -- Publisher <-- Matching engine
       |                  snapshot :20000                |
       +---------------- incremental :20001 <------------+
```

Orders enter through the TCP order server, are sequenced, and reach the matching engine through an SPSC queue. The matcher applies price-time priority and publishes client responses and market updates through separate queues. A snapshot synthesizer complements the incremental multicast stream.

## Features

- Limit-order matching with price-time priority
- New and cancel order requests
- Non-blocking TCP order transport
- Incremental and snapshot UDP multicast market data
- Lock-free SPSC queues and preallocated object pools
- Market-maker, liquidity-taker, and random-order client modes
- Per-ticker position, order-size, and loss controls
- Latency tracking and a periodic performance dashboard

## Requirements

The networking and thread-affinity code currently targets Linux/POSIX. A native Windows build is not supported; on Windows, use WSL2 with a Linux distribution.

- Linux (or WSL2)
- CMake 3.16 or newer
- GCC or Clang with C++20 support
- POSIX threads and multicast-capable loopback networking

## Build

Release build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Debug builds enable AddressSanitizer and UndefinedBehaviorSanitizer with GCC or Clang:

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --parallel
```

The executables are written to the selected build directory as `exchange_main` and `trading_main`.

## Run

Start the exchange first:

```bash
./build/exchange_main
```

In another terminal, start a client:

```bash
./build/trading_main 1 MAKER 100 0.5 1000 5000 100
```

Client syntax:

```text
trading_main CLIENT_ID ALGO_TYPE [CLIP THRESHOLD MAX_ORDER_SIZE MAX_POSITION MAX_LOSS]...
```

- `CLIENT_ID`: integer from `0` through `255`; each connected client must use a unique ID.
- `ALGO_TYPE`: `RANDOM`, `MAKER`, or `TAKER`.
- Each five-value group configures the next ticker, beginning with ticker `0`.
- At most eight ticker configuration groups may be supplied.
- `RANDOM` is a load-generating mode and currently submits 100 million new/cancel pairs before exiting.
- Stop the exchange with `Ctrl+C`.

Runtime logs are created in the current working directory and ignored by Git.

## Default Network Configuration

| Channel | Address | Port | Transport |
| --- | --- | ---: | --- |
| Order entry | `127.0.0.1` / loopback | 12345 | TCP |
| Market-data snapshots | `233.252.14.1` | 20000 | UDP multicast |
| Incremental market data | `233.252.14.3` | 20001 | UDP multicast |

These values are currently compile-time constants in the two program entry points.

## Project Layout

```text
.
|-- CMakeLists.txt
|-- Exchange Matching Engine/
|   |-- Common Files/       shared queues, pools, sockets, logging, and timing
|   `-- EXCHANGE/
|       |-- exchange_main.cpp
|       |-- matcher/        matching engine and order book
|       |-- order_server/   TCP gateway and FIFO sequencer
|       `-- market_data/    incremental publisher and snapshot synthesizer
|-- trading/
|   |-- trading_main.cpp
|   |-- strategy/           algorithms, risk, positions, and client order book
|   |-- order_gw/           TCP order client
|   `-- market_data/        multicast consumer
`-- METRICS.json            historical run metadata
```

## Performance Notes

Release builds enable aggressive native-CPU optimizations, including LTO, `-march=native`, and `-ffast-math`. Consequently, a release binary may not run on a different CPU and floating-point behavior may differ from strict IEEE semantics.

For meaningful measurements, build and run on the same isolated Linux host, pin work to suitable cores, use a fixed CPU frequency, record the exact compiler and CPU model, warm up the process, and report the latency boundary being measured. The current in-process timing should not be interpreted as end-to-end network latency.

## Current Limitations

- Linux-specific socket APIs and optional CPU affinity
- Fixed network addresses, ports, and instrument count
- No persistence, authentication, encrypted transport, or recovery journal
- No automated test suite yet
- Historical benchmark data has no checked-in benchmark harness or environment manifest

## License

No license file is currently included. Unless a license is added, copyright law reserves reuse and redistribution rights to the repository owner.
