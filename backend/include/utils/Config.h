#pragma once

// ============================================================================
// Config.h
// Centralised, tunable configuration for the Smart Dispatch Engine.
// Keeping these as named constants (instead of magic numbers scattered across
// DispatchEngine.cpp) makes the scoring formula easy to explain, tune and
// unit-test.
// ============================================================================

namespace config {

// ---- Dispatch scoring weights -------------------------------------------
// finalScore = (distanceWeight   * distanceScore)
//            + (workloadWeight   * workloadScore)
//            + (ratingWeight     * ratingScore)
//            + priorityAdjustment
//
// distanceScore, workloadScore and ratingScore are all normalised to a
// 0-100 scale before the weights are applied. See DispatchEngine::score().
struct DispatchWeights {
    static constexpr double DISTANCE_WEIGHT = 0.40;   // closeness matters most
    static constexpr double WORKLOAD_WEIGHT = 0.30;   // avoid overloading agents
    static constexpr double RATING_WEIGHT   = 0.30;   // reward good agents
};

// ---- Distance scoring ------------------------------------------------------
// distanceScore = 100 * (1 - min(distanceKm, MAX_RELEVANT_DISTANCE_KM) / MAX_RELEVANT_DISTANCE_KM)
constexpr double MAX_RELEVANT_DISTANCE_KM = 8.0;

// ---- Workload scoring -------------------------------------------------------
// workloadScore = 100 * (1 - activeOrders / maxCapacity)
constexpr int DEFAULT_AGENT_MAX_CAPACITY = 3;

// ---- Rating scoring ----------------------------------------------------------
// rating is stored 0.0-5.0 -> normalised to 0-100
constexpr double MAX_RATING = 5.0;

// ---- Priority adjustment (flat bonus added after weighting) -----------------
constexpr double PRIORITY_BONUS_NORMAL     = 0.0;
constexpr double PRIORITY_BONUS_HIGH       = 10.0;
constexpr double PRIORITY_BONUS_URGENT     = 20.0;
constexpr double PRIORITY_BONUS_PERISHABLE = 25.0;

// ---- Anti-starvation aging -----------------------------------------------
// Every AGING_INTERVAL_SECONDS an order waits in queue without being
// dispatched, its effective priority bonus increases by AGING_BONUS_STEP,
// up to AGING_MAX_BONUS, so normal orders are never starved forever by a
// constant stream of high priority orders.
constexpr long   AGING_INTERVAL_SECONDS = 45;   // demo-friendly (real world: minutes)
constexpr double AGING_BONUS_STEP       = 5.0;
constexpr double AGING_MAX_BONUS        = 30.0;

// ---- Zone compatibility bonus -----------------------------------------------
// Small bonus if the agent's current zone matches the restaurant's zone,
// since same-zone agents typically know local routes better.
constexpr double ZONE_MATCH_BONUS = 5.0;

// ---- Misc --------------------------------------------------------------------
constexpr int    SERVER_PORT = 8080;
constexpr double EARTH_RADIUS_KM = 6371.0;

} // namespace config
