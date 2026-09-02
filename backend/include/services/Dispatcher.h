#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>
#include "DispatchEngine.h"
#include "NotificationService.h"
#include "../models/Delivery.h"

// ============================================================================
// Dispatcher.h
// Thin orchestration layer sitting between the REST routes (main.cpp) and
// the DispatchEngine algorithm. Responsible for:
//   - invoking DispatchEngine
//   - persisting a Delivery record (transparent score breakdown) per assignment
//   - raising notifications
// This keeps DispatchEngine a pure, easily-unit-testable algorithm class,
// while Dispatcher handles the "side effects" of a successful dispatch.
// ============================================================================
class Dispatcher {
private:
    DispatchEngine engine;
    NotificationService& notifications;
    std::unordered_map<std::string, Delivery> deliveries; // history, keyed by Delivery ID
    std::vector<std::string> deliveryInsertionOrder;

    void recordDelivery(const DispatchResult& result, bool manual) {
        if (!result.success || result.candidates.empty()) return;
        const CandidateScore& chosen = manual ? result.candidates.front()
            : *std::find_if(result.candidates.begin(), result.candidates.end(),
                             [&](const CandidateScore& c) { return c.agentId == result.agentId; });

        std::string deliveryId = "DEL-" + std::to_string(deliveries.size() + 5000);
        Delivery d(deliveryId, result.orderId, result.agentId,
                   chosen.distanceScore, chosen.workloadScore, chosen.ratingScore,
                   chosen.priorityAdjustment, chosen.zoneBonus, chosen.finalScore, manual);
        deliveries.emplace(deliveryId, d);
        deliveryInsertionOrder.push_back(deliveryId);
    }

public:
    Dispatcher(AgentManager& agentMgr, OrderManager& orderMgr, RestaurantManager& restaurantMgr,
               NotificationService& notificationService)
        : engine(agentMgr, orderMgr, restaurantMgr), notifications(notificationService) {}

    DispatchEngine& getEngine() { return engine; }

    DispatchResult dispatchOne(const std::string& orderId) {
        DispatchResult result = engine.dispatchOrder(orderId);
        if (result.success) {
            recordDelivery(result, false);
            notifications.push(result.message, NotificationType::SUCCESS, "ADMIN");
            notifications.push("Your order " + orderId + " has been assigned to a delivery agent!",
                                NotificationType::SUCCESS, "CUSTOMER");
        } else {
            notifications.push(result.message, NotificationType::WARNING, "ADMIN");
        }
        return result;
    }

    std::vector<DispatchResult> autoDispatchAll() {
        std::vector<DispatchResult> results = engine.autoDispatchAll();
        for (const auto& r : results) {
            if (r.success) {
                recordDelivery(r, false);
                notifications.push(r.message, NotificationType::SUCCESS, "ADMIN");
            }
        }
        return results;
    }

    DispatchResult manualAssign(const std::string& orderId, const std::string& agentId) {
        DispatchResult result = engine.manualAssign(orderId, agentId);
        if (result.success) {
            recordDelivery(result, true);
            notifications.push(result.message, NotificationType::INFO, "ADMIN");
        }
        return result;
    }

    std::vector<Delivery*> getDeliveryHistory() {
        std::vector<Delivery*> result;
        for (const auto& id : deliveryInsertionOrder) result.push_back(&deliveries.at(id));
        return result;
    }
};
