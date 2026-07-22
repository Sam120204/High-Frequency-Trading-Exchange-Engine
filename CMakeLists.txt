cmake_minimum_required(VERSION 3.16)

project(NanosecondHFT LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

set(COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Exchange Matching Engine/Common Files")
set(EXCHANGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Exchange Matching Engine/EXCHANGE")

set(COMMON_SOURCES
    "${COMMON_DIR}/latency_tracker.cpp"
    "${COMMON_DIR}/performance_dashboard.cpp"
    "${COMMON_DIR}/tcp_socket.cpp"
    "${COMMON_DIR}/tcp_server.cpp"
    "${COMMON_DIR}/mcast_socket.cpp"
)

function(configure_hft_target target)
  target_compile_features(${target} PRIVATE cxx_std_20)
  target_link_libraries(${target} PRIVATE Threads::Threads)
  target_include_directories(${target} PRIVATE
      "${COMMON_DIR}"
      "${EXCHANGE_DIR}"
      "${EXCHANGE_DIR}/matcher"
      "${EXCHANGE_DIR}/order_server"
      "${EXCHANGE_DIR}/market_data"
      "${CMAKE_CURRENT_SOURCE_DIR}/trading"
      "${CMAKE_CURRENT_SOURCE_DIR}/trading/strategy"
      "${CMAKE_CURRENT_SOURCE_DIR}/trading/market_data"
      "${CMAKE_CURRENT_SOURCE_DIR}/trading/order_gw"
  )

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
    target_compile_options(${target} PRIVATE
        $<$<CONFIG:Release>:-O3;-march=native;-mtune=native;-flto;-funroll-loops;-fomit-frame-pointer;-ffast-math;-falign-functions=64;-falign-loops=64>
        $<$<CONFIG:Debug>:-O0;-g3;-fsanitize=address,undefined;-fno-omit-frame-pointer>
    )
    target_link_options(${target} PRIVATE
        $<$<CONFIG:Release>:-flto>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
  endif()
endfunction()

add_executable(exchange_main
    "${EXCHANGE_DIR}/exchange_main.cpp"
    "${EXCHANGE_DIR}/matcher/matching_engine.cpp"
    "${EXCHANGE_DIR}/matcher/me_order_book.cpp"
    "${EXCHANGE_DIR}/matcher/me_order.cpp"
    "${EXCHANGE_DIR}/market_data/market_data_publisher.cpp"
    "${EXCHANGE_DIR}/market_data/snapshot_synthesizer.cpp"
    "${EXCHANGE_DIR}/order_server/order_server.cpp"
    ${COMMON_SOURCES}
)
configure_hft_target(exchange_main)

add_executable(trading_main
    "${CMAKE_CURRENT_SOURCE_DIR}/trading/trading_main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/trading/strategy/trade_engine.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/trading/strategy/market_order_book.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/trading/strategy/market_order.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/trading/strategy/market_maker.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/trading/strategy/liquidity_taker.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/trading/strategy/order_manager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/trading/strategy/risk_manager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/trading/market_data/market_data_consumer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/trading/order_gw/order_gateway.cpp"
    ${COMMON_SOURCES}
)
configure_hft_target(trading_main)

if(BUILD_TESTING)
  add_executable(matching_engine_tests
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/matching_engine_tests.cpp"
      "${EXCHANGE_DIR}/matcher/matching_engine.cpp"
      "${EXCHANGE_DIR}/matcher/me_order_book.cpp"
      "${EXCHANGE_DIR}/matcher/me_order.cpp"
      "${COMMON_DIR}/latency_tracker.cpp"
  )
  configure_hft_target(matching_engine_tests)
  add_test(NAME matching_engine_tests COMMAND matching_engine_tests)
endif()

add_executable(matching_engine_benchmark
    "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/matching_engine_benchmark.cpp"
    "${EXCHANGE_DIR}/matcher/matching_engine.cpp"
    "${EXCHANGE_DIR}/matcher/me_order_book.cpp"
    "${EXCHANGE_DIR}/matcher/me_order.cpp"
    "${COMMON_DIR}/latency_tracker.cpp"
)
configure_hft_target(matching_engine_benchmark)
