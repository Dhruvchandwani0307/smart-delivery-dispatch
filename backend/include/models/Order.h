#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <set>
#include "Location.h"
#include "Payment.h"
#include "../third_party/json.hpp"

using json = nlohmann::json;

// ============================================================================
// Order status state machine
// ============================================================================
enum class OrderStatus {
    PLACED,
    CONFIRMED,
    PREPARING,
    READY,
    ASSIGNED,
    PICKED_UP,
    OUT_FOR_DELIVERY,
    DELIVERED,
    CANCELLED
};

inline std::string orderStatusToString(OrderStatus s) {
    switch (s) {
        case OrderStatus::PLACED: return "PLACED";
        case OrderStatus::CONFIRMED: return "CONFIRMED";
        case OrderStatus::PREPARING: return "PREPARING";
        case OrderStatus::READY: return "READY";
        case OrderStatus::ASSIGNED: return "ASSIGNED";
        case OrderStatus::PICKED_UP: return "PICKED_UP";
        case OrderStatus::OUT_FOR_DELIVERY: return "OUT_FOR_DELIVERY";
        case OrderStatus::DELIVERED: return "DELIVERED";
        case OrderStatus::CANCELLED: return "CANCELLED";
    }
    return "UNKNOWN";
}

inline OrderStatus orderStatusFromString(const std::string& s) {
    if (s == "PLACED") return OrderStatus::PLACED;
    if (s == "CONFIRMED") return OrderStatus::CONFIRMED;
    if (s == "PREPARING") return OrderStatus::PREPARING;
    if (s == "READY") return OrderStatus::READY;
    if (s == "ASSIGNED") return OrderStatus::ASSIGNED;
    if (s == "PICKED_UP") return OrderStatus::PICKED_UP;
    if (s == "OUT_FOR_DELIVERY") return OrderStatus::OUT_FOR_DELIVERY;
    if (s == "DELIVERED") return OrderStatus::DELIVERED;
    return OrderStatus::CANCELLED;
}

enum class OrderPriority { NORMAL, HIGH, URGENT, PERISHABLE };

inline std::string orderPriorityToString(OrderPriority p) {
    switch (p) {
        case OrderPriority::NORMAL: return "NORMAL";
        case OrderPriority::HIGH: return "HIGH";
        case OrderPriority::URGENT: return "URGENT";
        case OrderPriority::PERISHABLE: return "PERISHABLE";
    }
    return "UNKNOWN";
}

inline OrderPriority orderPriorityFromString(const std::string& s) {
    if (s == "HIGH") return OrderPriority::HIGH;
    if (s == "URGENT") return OrderPriority::URGENT;
    if (s == "PERISHABLE") return OrderPriority::PERISHABLE;
    return OrderPriority::NORMAL;
}

struct OrderItem {
    std::string name;
    int quantity;
    double price;

    json toJson() const {
        return json{{"name", name}, {"quantity", quantity}, {"price", price}};
    }

    static OrderItem fromJson(const json& j) {
        return OrderItem{j.value("name", ""), j.value("quantity", 1), j.value("price", 0.0)};
    }
};

// Thrown when an invalid state transition is attempted.
class InvalidOrderTransition : public std::runtime_error {
public:
    explicit InvalidOrderTransition(const std::string& msg) : std::runtime_error(msg) {}
};

// ============================================================================
// Order.h
// Encapsulates the order state machine. All status changes MUST go through
// transitionTo() so that invalid transitions (e.g. DELIVERED -> PREPARING)
// are rejected uniformly, instead of being set directly all over the code
// base.
// ============================================================================
class Order {
private:
    std::string id;
    std::string customerId;
    std::string restaurantId;
    std::string assignedAgentId; // empty until ASSIGNED
    std::vector<OrderItem> items;
    Location deliveryLocation;
    OrderStatus status;
    OrderPriority priority;
    Payment payment;
    std::chrono::system_clock::time_point placedAt;
    std::chrono::system_clock::time_point readyAt;   // set when it enters READY (used for aging/ETA)
    std::chrono::system_clock::time_point deliveredAt;
    int estimatedEtaMinutes;

    // Valid forward transitions. Declared static so every Order instance
    // shares one definition of "the rules".
    static const std::set<std::pair<OrderStatus, OrderStatus>>& allowedTransitions() {
        static const std::set<std::pair<OrderStatus, OrderStatus>> table = {
            {OrderStatus::PLACED, OrderStatus::CONFIRMED},
            {OrderStatus::CONFIRMED, OrderStatus::PREPARING},
            {OrderStatus::PREPARING, OrderStatus::READY},
            {OrderStatus::READY, OrderStatus::ASSIGNED},
            {OrderStatus::ASSIGNED, OrderStatus::PICKED_UP},
            {OrderStatus::PICKED_UP, OrderStatus::OUT_FOR_DELIVERY},
            {OrderStatus::OUT_FOR_DELIVERY, OrderStatus::DELIVERED},
            // cancellation allowed from any pre-pickup state
            {OrderStatus::PLACED, OrderStatus::CANCELLED},
            {OrderStatus::CONFIRMED, OrderStatus::CANCELLED},
            {OrderStatus::PREPARING, OrderStatus::CANCELLED},
            {OrderStatus::READY, OrderStatus::CANCELLED},
            {OrderStatus::ASSIGNED, OrderStatus::CANCELLED},
            // allow READY -> READY re-dispatch bookkeeping is handled outside the FSM
        };
        return table;
    }

public:
    Order(std::string id_, std::string customerId_, std::string restaurantId_,
          std::vector<OrderItem> items_, Location deliveryLocation_,
          OrderPriority priority_, Payment payment_)
        : id(std::move(id_)), customerId(std::move(customerId_)), restaurantId(std::move(restaurantId_)),
          items(std::move(items_)), deliveryLocation(std::move(deliveryLocation_)),
          status(OrderStatus::PLACED), priority(priority_), payment(std::move(payment_)),
          placedAt(std::chrono::system_clock::now()),
          readyAt(std::chrono::system_clock::time_point{}),
          deliveredAt(std::chrono::system_clock::time_point{}),
          estimatedEtaMinutes(30) {}

    // ---- Accessors ----
    const std::string& getId() const { return id; }
    const std::string& getCustomerId() const { return customerId; }
    const std::string& getRestaurantId() const { return restaurantId; }
    const std::string& getAssignedAgentId() const { return assignedAgentId; }
    const std::vector<OrderItem>& getItems() const { return items; }
    const Location& getDeliveryLocation() const { return deliveryLocation; }
    OrderStatus getStatus() const { return status; }
    OrderPriority getPriority() const { return priority; }
    void setPriority(OrderPriority p) { priority = p; } // used by aging logic
    const Payment& getPayment() const { return payment; }
    Payment& getPaymentMutable() { return payment; }
    int getEstimatedEtaMinutes() const { return estimatedEtaMinutes; }
    void setEstimatedEtaMinutes(int m) { estimatedEtaMinutes = m; }

    double getTotalAmount() const {
        double total = 0.0;
        for (const auto& item : items) total += item.price * item.quantity;
        return total;
    }

    std::chrono::system_clock::time_point getPlacedAt() const { return placedAt; }
    std::chrono::system_clock::time_point getReadyAt() const { return readyAt; }

    long secondsSinceReady() const {
        if (readyAt.time_since_epoch().count() == 0) return 0;
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - readyAt).count();
    }

    bool isActive() const {
        return status != OrderStatus::DELIVERED && status != OrderStatus::CANCELLED;
    }

    bool isCancellable() const {
        return status == OrderStatus::PLACED || status == OrderStatus::CONFIRMED ||
               status == OrderStatus::PREPARING || status == OrderStatus::READY;
    }

    // The one and only way to change order status.
    void transitionTo(OrderStatus newStatus) {
        if (status == newStatus) return; // no-op
        auto key = std::make_pair(status, newStatus);
        if (allowedTransitions().find(key) == allowedTransitions().end()) {
            throw InvalidOrderTransition(
                "Cannot transition order " + id + " from " + orderStatusToString(status) +
                " to " + orderStatusToString(newStatus));
        }
        status = newStatus;
        if (newStatus == OrderStatus::READY) {
            readyAt = std::chrono::system_clock::now();
        }
        if (newStatus == OrderStatus::DELIVERED) {
            deliveredAt = std::chrono::system_clock::now();
        }
        if (newStatus != OrderStatus::ASSIGNED && newStatus != OrderStatus::PICKED_UP &&
            newStatus != OrderStatus::OUT_FOR_DELIVERY && newStatus != OrderStatus::DELIVERED) {
            // clear agent if we ever bounce back out of the assignment pipeline
        }
    }

    void assignAgent(const std::string& agentId) {
        assignedAgentId = agentId;
        transitionTo(OrderStatus::ASSIGNED);
    }

    // Used by admin "reassign" - clears agent without violating the FSM,
    // order stays READY so it can re-enter the dispatch queue.
    void unassignForReassignment() {
        assignedAgentId.clear();
        status = OrderStatus::READY; // direct set: this is an administrative override
    }

    json toJson() const {
        json itemsJson = json::array();
        for (const auto& item : items) itemsJson.push_back(item.toJson());

        return json{
            {"id", id},
            {"customerId", customerId},
            {"restaurantId", restaurantId},
            {"assignedAgentId", assignedAgentId},
            {"items", itemsJson},
            {"deliveryLocation", deliveryLocation.toJson()},
            {"status", orderStatusToString(status)},
            {"priority", orderPriorityToString(priority)},
            {"payment", payment.toJson()},
            {"totalAmount", getTotalAmount()},
            {"estimatedEtaMinutes", estimatedEtaMinutes},
            {"secondsSinceReady", secondsSinceReady()}
        };
    }
};
