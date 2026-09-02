#pragma once
#include <unordered_map>
#include <vector>
#include <queue>
#include <set>
#include <string>
#include <chrono>
#include "../models/Order.h"
#include "../utils/Config.h"

// ============================================================================
// OrderManager.h
//
// Data structures used (per project spec):
//   - unordered_map<string, Order>  : O(1) average order lookup by ID
//   - queue<string>                 : FIFO of freshly PLACED orders, drained
//                                      by the simulated kitchen pipeline
//                                      (PLACED -> CONFIRMED -> PREPARING)
//   - priority_queue<QueueEntry>    : orders that are READY and waiting to
//                                      be handed to the DispatchEngine,
//                                      ordered by effective priority
//                                      (base priority + anti-starvation aging)
//   - set<string>                   : unique delivery zones seen so far
// ============================================================================

struct DispatchQueueEntry {
    std::string orderId;
    double effectivePriorityValue;
    std::chrono::system_clock::time_point enqueuedAt;

    // Smaller "less than" result = lower priority to pop first in a max-heap
    // std::priority_queue pops the *largest* element by default, which is
    // exactly what we want: highest effective priority first.
    bool operator<(const DispatchQueueEntry& other) const {
        return effectivePriorityValue < other.effectivePriorityValue;
    }
};

class OrderManager {
private:
    std::unordered_map<std::string, Order> orders;
    std::vector<std::string> insertionOrder; // stable listing / iteration order

    std::queue<std::string> incomingOrderQueue;             // PLACED orders pipeline
    std::priority_queue<DispatchQueueEntry> dispatchQueue;  // READY orders waiting for an agent
    std::set<std::string> deliveryZones;                    // unique zones

    static double basePriorityValue(OrderPriority p) {
        switch (p) {
            case OrderPriority::NORMAL: return 0.0;
            case OrderPriority::HIGH: return config::PRIORITY_BONUS_HIGH;
            case OrderPriority::URGENT: return config::PRIORITY_BONUS_URGENT;
            case OrderPriority::PERISHABLE: return config::PRIORITY_BONUS_PERISHABLE;
        }
        return 0.0;
    }

public:
    Order* createOrder(Order order) {
        const std::string id = order.getId();
        deliveryZones.insert(order.getDeliveryLocation().zone);
        orders.emplace(id, std::move(order));
        insertionOrder.push_back(id);
        incomingOrderQueue.push(id);
        return &orders.at(id);
    }

    Order* getOrder(const std::string& id) {
        auto it = orders.find(id);
        if (it == orders.end()) return nullptr;
        return &it->second;
    }

    bool exists(const std::string& id) const { return orders.find(id) != orders.end(); }

    std::vector<Order*> getAllOrders() {
        std::vector<Order*> result;
        result.reserve(insertionOrder.size());
        for (const auto& id : insertionOrder) result.push_back(&orders.at(id));
        return result;
    }

    std::vector<Order*> getOrdersByCustomer(const std::string& customerId) {
        std::vector<Order*> result;
        for (const auto& id : insertionOrder) {
            Order& o = orders.at(id);
            if (o.getCustomerId() == customerId) result.push_back(&o);
        }
        return result;
    }

    std::vector<Order*> getOrdersByStatus(OrderStatus status) {
        std::vector<Order*> result;
        for (const auto& id : insertionOrder) {
            Order& o = orders.at(id);
            if (o.getStatus() == status) result.push_back(&o);
        }
        return result;
    }

    // Pops the next PLACED order from the FIFO pipeline (used by the
    // simulated kitchen workflow). Returns nullptr if empty.
    Order* popNextIncoming() {
        while (!incomingOrderQueue.empty()) {
            const std::string id = incomingOrderQueue.front();
            incomingOrderQueue.pop();
            auto it = orders.find(id);
            if (it != orders.end() && it->second.getStatus() == OrderStatus::PLACED) {
                return &it->second;
            }
        }
        return nullptr;
    }

    bool hasIncoming() const { return !incomingOrderQueue.empty(); }

    // Called once an order transitions into READY - enqueues it for the
    // DispatchEngine's attention.
    void enqueueForDispatch(const std::string& orderId) {
        Order* o = getOrder(orderId);
        if (!o) return;
        double eff = basePriorityValue(o->getPriority());
        dispatchQueue.push(DispatchQueueEntry{orderId, eff, std::chrono::system_clock::now()});
    }

    // Recomputes effective priority for every order still sitting in READY
    // status based on how long it has waited (anti-starvation aging), then
    // rebuilds the heap. Cheap enough for a demo-scale system.
    void applyAgingAndRebuild() {
        std::priority_queue<DispatchQueueEntry> rebuilt;
        std::priority_queue<DispatchQueueEntry> temp = dispatchQueue;
        std::set<std::string> seen;

        while (!temp.empty()) {
            DispatchQueueEntry entry = temp.top();
            temp.pop();
            if (seen.count(entry.orderId)) continue; // dedupe stale duplicates
            seen.insert(entry.orderId);

            Order* o = getOrder(entry.orderId);
            if (!o || o->getStatus() != OrderStatus::READY) continue; // stale, drop

            long waitSeconds = o->secondsSinceReady();
            double agingBonus = std::min(
                config::AGING_MAX_BONUS,
                (double)(waitSeconds / config::AGING_INTERVAL_SECONDS) * config::AGING_BONUS_STEP);

            double eff = basePriorityValue(o->getPriority()) + agingBonus;
            rebuilt.push(DispatchQueueEntry{entry.orderId, eff, entry.enqueuedAt});
        }
        dispatchQueue = std::move(rebuilt);
    }

    bool hasReadyForDispatch() {
        applyAgingAndRebuild();
        return !dispatchQueue.empty();
    }

    // Pops the single best (highest effective priority, oldest wait as
    // tiebreak via std::priority_queue's stable-ish heap) order ready for
    // dispatch. Returns nullptr if none.
    Order* popNextForDispatch() {
        applyAgingAndRebuild();
        while (!dispatchQueue.empty()) {
            DispatchQueueEntry entry = dispatchQueue.top();
            dispatchQueue.pop();
            Order* o = getOrder(entry.orderId);
            if (o && o->getStatus() == OrderStatus::READY) return o;
        }
        return nullptr;
    }

    // Peek all orders currently sitting in the dispatch queue without
    // removing them - used by the Dispatch Center UI.
    std::vector<Order*> peekDispatchQueue() {
        applyAgingAndRebuild();
        std::vector<Order*> result;
        std::priority_queue<DispatchQueueEntry> temp = dispatchQueue;
        std::set<std::string> seen;
        while (!temp.empty()) {
            DispatchQueueEntry entry = temp.top();
            temp.pop();
            if (seen.count(entry.orderId)) continue;
            seen.insert(entry.orderId);
            Order* o = getOrder(entry.orderId);
            if (o && o->getStatus() == OrderStatus::READY) result.push_back(o);
        }
        return result;
    }

    const std::set<std::string>& getDeliveryZones() const { return deliveryZones; }

    size_t size() const { return orders.size(); }
};
