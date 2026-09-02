#pragma once
#include <string>
#include <chrono>
#include "../third_party/json.hpp"

using json = nlohmann::json;

// ============================================================================
// Delivery.h
// Represents one dispatch "assignment event" - an order handed to an agent,
// including the transparent score breakdown that produced the decision.
// Kept separate from Order so an order's dispatch history (in case of
// reassignment) can be tracked independently.
// ============================================================================
class Delivery {
private:
    std::string id;
    std::string orderId;
    std::string agentId;
    double distanceScore;
    double workloadScore;
    double ratingScore;
    double priorityAdjustment;
    double zoneBonus;
    double finalScore;
    std::chrono::system_clock::time_point assignedAt;
    bool manualOverride;

public:
    Delivery(std::string id_, std::string orderId_, std::string agentId_,
             double distanceScore_, double workloadScore_, double ratingScore_,
             double priorityAdjustment_, double zoneBonus_, double finalScore_,
             bool manualOverride_ = false)
        : id(std::move(id_)), orderId(std::move(orderId_)), agentId(std::move(agentId_)),
          distanceScore(distanceScore_), workloadScore(workloadScore_), ratingScore(ratingScore_),
          priorityAdjustment(priorityAdjustment_), zoneBonus(zoneBonus_), finalScore(finalScore_),
          assignedAt(std::chrono::system_clock::now()), manualOverride(manualOverride_) {}

    const std::string& getId() const { return id; }
    const std::string& getOrderId() const { return orderId; }
    const std::string& getAgentId() const { return agentId; }
    double getFinalScore() const { return finalScore; }

    json toJson() const {
        return json{
            {"id", id},
            {"orderId", orderId},
            {"agentId", agentId},
            {"scoreBreakdown", {
                {"distanceScore", distanceScore},
                {"workloadScore", workloadScore},
                {"ratingScore", ratingScore},
                {"priorityAdjustment", priorityAdjustment},
                {"zoneBonus", zoneBonus},
                {"finalScore", finalScore}
            }},
            {"manualOverride", manualOverride}
        };
    }
};
