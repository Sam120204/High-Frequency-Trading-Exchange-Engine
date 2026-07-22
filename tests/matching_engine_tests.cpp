#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "matching_engine.h"

namespace {
using namespace Common;
using namespace Exchange;

auto require(bool condition, const std::string &message) -> void {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

template<typename T>
auto drain(Common::LFQueue<T> &queue) -> std::vector<T> {
  std::vector<T> values;
  while (const auto *next = queue.getNextToRead()) {
    values.push_back(*next);
    queue.updateReadIndex();
  }
  return values;
}

auto request(ClientRequestType type, ClientId client, TickerId ticker, OrderId order,
             Side side, Price price, Qty qty) -> MEClientRequest {
  return {type, client, ticker, order, side, price, qty};
}
}

int main() {
  Common::LFQueue<int> wraparound_queue(8);
  for (int value = 0; value < 100; ++value) {
    auto *slot = wraparound_queue.getNextToWriteTo();
    require(slot != nullptr, "queue unexpectedly reported full during wraparound test");
    *slot = value;
    wraparound_queue.updateWriteIndex();
    require(*wraparound_queue.getNextToRead() == value, "queue corrupted data after index wraparound");
    wraparound_queue.updateReadIndex();
  }

  ClientRequestLFQueue requests(64);
  ClientResponseLFQueue responses(256);
  MEMarketUpdateLFQueue updates(256);
  MatchingEngine engine(&requests, &responses, &updates, 64, 65536);

  auto submit = [&](const MEClientRequest &value) {
    engine.processClientRequest(&value);
    return std::pair{drain(responses), drain(updates)};
  };

  // Full fill at the resting order's price.
  submit(request(ClientRequestType::NEW, 1, 0, 1, Side::SELL, 100, 10));
  auto [full_responses, full_updates] = submit(request(ClientRequestType::NEW, 2, 0, 2, Side::BUY, 100, 10));
  require(full_responses.size() == 3, "full fill must emit accepted and two fill responses");
  require(full_responses[1].client_id_ == 2 && full_responses[1].exec_qty_ == 10 && full_responses[1].leaves_qty_ == 0,
          "aggressive full-fill response is incorrect");
  require(full_responses[2].client_id_ == 1 && full_responses[2].leaves_qty_ == 0,
          "passive full-fill response is incorrect");
  require(full_updates.size() == 2 && full_updates[0].type_ == MarketUpdateType::TRADE &&
          full_updates[1].type_ == MarketUpdateType::CANCEL, "full fill market-data sequence is incorrect");

  // Partial fill leaves the passive order on the book.
  submit(request(ClientRequestType::NEW, 3, 1, 10, Side::SELL, 105, 10));
  auto [partial_responses, partial_updates] = submit(request(ClientRequestType::NEW, 4, 1, 11, Side::BUY, 105, 6));
  require(partial_responses.size() == 3 && partial_responses[2].leaves_qty_ == 4,
          "partial fill must leave four units resting");
  require(partial_updates.size() == 2 && partial_updates[1].type_ == MarketUpdateType::MODIFY &&
          partial_updates[1].qty_ == 4, "partial fill must publish a modify update");

  // FIFO: two orders at one price must execute in arrival order.
  submit(request(ClientRequestType::NEW, 5, 2, 20, Side::SELL, 110, 5));
  submit(request(ClientRequestType::NEW, 6, 2, 21, Side::SELL, 110, 5));
  auto [fifo_responses, fifo_updates] = submit(request(ClientRequestType::NEW, 7, 2, 22, Side::BUY, 110, 6));
  require(fifo_responses.size() == 5, "sweeping two FIFO orders must emit four fills plus acceptance");
  require(fifo_responses[2].client_id_ == 5 && fifo_responses[4].client_id_ == 6,
          "same-price orders did not execute FIFO");
  require(fifo_responses[4].leaves_qty_ == 4, "second FIFO order should have four units remaining");

  // Price priority: the best ask executes before a worse ask regardless of arrival order.
  submit(request(ClientRequestType::NEW, 8, 3, 30, Side::SELL, 121, 1));
  submit(request(ClientRequestType::NEW, 9, 3, 31, Side::SELL, 120, 1));
  auto [price_responses, price_updates] = submit(request(ClientRequestType::NEW, 10, 3, 32, Side::BUY, 121, 2));
  require(price_responses.size() == 5, "cross-level sweep response count is incorrect");
  require(price_responses[1].price_ == 120 && price_responses[3].price_ == 121,
          "best ask was not matched first");

  // Cancellation and rejection for a no-longer-resting order.
  submit(request(ClientRequestType::NEW, 11, 4, 40, Side::BUY, 90, 3));
  auto [cancel_responses, cancel_updates] = submit(request(ClientRequestType::CANCEL, 11, 4, 40, Side::BUY, 90, 3));
  require(cancel_responses.size() == 1 && cancel_responses[0].type_ == ClientResponseType::CANCELED,
          "valid cancellation was not accepted");
  require(cancel_updates.size() == 1 && cancel_updates[0].type_ == MarketUpdateType::CANCEL,
          "valid cancellation did not publish market data");
  auto [reject_responses, reject_updates] = submit(request(ClientRequestType::CANCEL, 11, 4, 40, Side::BUY, 90, 3));
  require(reject_responses.size() == 1 && reject_responses[0].type_ == ClientResponseType::CANCEL_REJECTED,
          "unknown cancellation was not rejected");
  require(reject_updates.empty(), "rejected cancellation must not publish market data");

  std::cout << "PASS: matching engine scenarios\n";
  return EXIT_SUCCESS;
}
