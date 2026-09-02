#pragma once
#include <vector>
#include <string>
#include "../managers/AgentManager.h"
#include "../managers/OrderManager.h"
#include "../managers/RestaurantManager.h"

// ============================================================================
// DispatchEngine.h
//
// THE core algorithm of this project. Given a READY order, it scores every
// available, capacity-eligible delivery agent using a weighted, configurable
// formula and picks the best one. The algorithm is intentionally isolated
// from the HTTP/API layer (see routes in main.cpp) so it can be unit tested
// in isolation - see tests/test_dispatch.cpp.
//
// Scoring formula (weights documented in include/utils/Config.h):
//
//   distanceScore  = 100 * (1 - min(distanceKm, MAX_DIST) / MAX_DIST)
//   workloadScore  = 100 * (1 - activeOrders / maxCapacity)
//   ratingScore    = 100 * (rating / 5.0)
//
//   weighted       = DISTANCE_WEIGHT * distanceScore
//                   + WORKLOAD_WEIGHT * workloadScore
//                   + RATING_WEIGHT   * ratingScore
//
//   finalScore     = weighted + priorityAdjustment + zoneBonus
//
// priorityAdjustment comes from the order's (aging-adjusted) priority.
// zoneBonus is a small flat bonus if the agent's current zone matches the
// restaurant's zone.
// ============================================================================

struct CandidateScore {
    std::string agentId;
    std::string agentName;
    double distanceKm;
    double distanceScore;
    double workloadScore;
    double ratingScore;
    double priorityAdjustment;
    double zoneBonus;
    double finalScore;
};

struct DispatchResult {
    bool success;
    std::string orderId;
    std::string agentId;
    std::string message;
    std::vector<CandidateScore> candidates; // full transparent breakdown, sorted best-first
};

class DispatchEngine {
private:
    AgentManager& agentManager;
    OrderManager& orderManager;
    RestaurantManager& restaurantManager;

    double effectivePriorityBonus(const Order& order) const;

public:
    DispatchEngine(AgentManager& agentMgr, OrderManager& orderMgr, RestaurantManager& restaurantMgr)
        : agentManager(agentMgr), orderManager(orderMgr), restaurantManager(restaurantMgr) {}

    // Scores a single agent against a single order. Public so the Dispatch
    // Center UI / tests can request a full breakdown without performing an
    // actual assignment.
    CandidateScore score(const Order& order, const DeliveryAgent& agent) const;

    // Returns all eligible candidates for an order, sorted best score first.
    std::vector<CandidateScore> evaluateCandidates(const Order& order) const;

    // Runs the full algorithm end-to-end for one order: evaluate, pick best,
    // mutate agent workload, transition order to ASSIGNED. Throws
    // NoAgentAvailableException if nobody is eligible.
    DispatchResult dispatchOrder(const std::string& orderId);

    // Iterates the OrderManager's dispatch queue and assigns as many READY
    // orders as currently possible. Returns one DispatchResult per attempt.
    std::vector<DispatchResult> autoDispatchAll();

    // Admin manual override - bypasses scoring, still enforces capacity.
    DispatchResult manualAssign(const std::string& orderId, const std::string& agentId);
};
