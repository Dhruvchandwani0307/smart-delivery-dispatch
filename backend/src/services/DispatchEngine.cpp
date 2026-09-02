#include "../../include/services/DispatchEngine.h"
#include "../../include/utils/Exceptions.h"
#include "../../include/utils/Config.h"
#include <algorithm>

double DispatchEngine::effectivePriorityBonus(const Order& order) const {
    double base = 0.0;
    switch (order.getPriority()) {
        case OrderPriority::NORMAL: base = config::PRIORITY_BONUS_NORMAL; break;
        case OrderPriority::HIGH: base = config::PRIORITY_BONUS_HIGH; break;
        case OrderPriority::URGENT: base = config::PRIORITY_BONUS_URGENT; break;
        case OrderPriority::PERISHABLE: base = config::PRIORITY_BONUS_PERISHABLE; break;
    }
    long waitSeconds = order.secondsSinceReady();
    double agingBonus = std::min(
        config::AGING_MAX_BONUS,
        (double)(waitSeconds / config::AGING_INTERVAL_SECONDS) * config::AGING_BONUS_STEP);
    return base + agingBonus;
}

CandidateScore DispatchEngine::score(const Order& order, const DeliveryAgent& agent) const {
    Restaurant* restaurant = restaurantManager.getRestaurant(order.getRestaurantId());

    double distanceKm = 0.0;
    if (restaurant) {
        distanceKm = agent.getLocation().distanceTo(restaurant->getLocation());
    }

    double cappedDistance = std::min(distanceKm, config::MAX_RELEVANT_DISTANCE_KM);
    double distanceScore = 100.0 * (1.0 - (cappedDistance / config::MAX_RELEVANT_DISTANCE_KM));

    double workloadScore = 100.0 * (1.0 - ((double)agent.getActiveOrders() / (double)agent.getMaxCapacity()));
    if (workloadScore < 0.0) workloadScore = 0.0;

    double ratingScore = 100.0 * (agent.getRating() / config::MAX_RATING);

    double priorityAdjustment = effectivePriorityBonus(order);

    double zoneBonus = 0.0;
    if (restaurant && agent.getLocation().zone == restaurant->getLocation().zone) {
        zoneBonus = config::ZONE_MATCH_BONUS;
    }

    double weighted =
        (config::DispatchWeights::DISTANCE_WEIGHT * distanceScore) +
        (config::DispatchWeights::WORKLOAD_WEIGHT * workloadScore) +
        (config::DispatchWeights::RATING_WEIGHT * ratingScore);

    double finalScore = weighted + priorityAdjustment + zoneBonus;

    return CandidateScore{
        agent.getId(),
        agent.getName(),
        distanceKm,
        distanceScore,
        workloadScore,
        ratingScore,
        priorityAdjustment,
        zoneBonus,
        finalScore
    };
}

std::vector<CandidateScore> DispatchEngine::evaluateCandidates(const Order& order) const {
    std::vector<CandidateScore> results;
    // agentManager is non-const in this class, but evaluateCandidates() is
    // const - use a const-friendly path via getAllAgents() through a
    // const_cast-free approach: AgentManager exposes only non-const
    // iteration, so we loop here using getAllAgents() on the mutable ref
    // stored in the engine. This method does not mutate any agent.
    AgentManager& mutableAgentMgr = const_cast<AgentManager&>(agentManager);
    for (DeliveryAgent* agent : mutableAgentMgr.getAllAgents()) {
        if (!agent->isAvailableForAssignment()) continue;
        results.push_back(score(order, *agent));
    }
    std::sort(results.begin(), results.end(),
              [](const CandidateScore& a, const CandidateScore& b) {
                  return a.finalScore > b.finalScore;
              });
    return results;
}

DispatchResult DispatchEngine::dispatchOrder(const std::string& orderId) {
    Order* order = orderManager.getOrder(orderId);
    if (!order) throw OrderNotFoundException("Order not found: " + orderId);
    if (order->getStatus() != OrderStatus::READY) {
        throw InvalidRequestException("Order " + orderId + " is not READY for dispatch (current status: " +
                                       orderStatusToString(order->getStatus()) + ")");
    }

    std::vector<CandidateScore> candidates = evaluateCandidates(*order);

    if (candidates.empty()) {
        DispatchResult result;
        result.success = false;
        result.orderId = orderId;
        result.message = "No suitable delivery agent available";
        result.candidates = {};
        return result;
    }

    const CandidateScore& best = candidates.front();
    DeliveryAgent* agent = agentManager.getAgent(best.agentId);
    if (!agent) throw AgentNotFoundException("Agent not found: " + best.agentId);

    agent->incrementActiveOrders();
    order->assignAgent(agent->getId());

    // Simple ETA estimate: distance / vehicle speed + restaurant prep buffer
    double speed = agent->getVehicle().averageSpeedKmph();
    int etaMinutes = static_cast<int>((best.distanceKm / speed) * 60.0) + 10; // +10 min buffer
    order->setEstimatedEtaMinutes(std::max(5, etaMinutes));

    DispatchResult result;
    result.success = true;
    result.orderId = orderId;
    result.agentId = agent->getId();
    result.message = "Order " + orderId + " assigned to " + agent->getName();
    result.candidates = candidates;
    return result;
}

std::vector<DispatchResult> DispatchEngine::autoDispatchAll() {
    std::vector<DispatchResult> results;
    while (orderManager.hasReadyForDispatch()) {
        Order* order = orderManager.popNextForDispatch();
        if (!order) break;
        try {
            results.push_back(dispatchOrder(order->getId()));
        } catch (const std::exception& e) {
            DispatchResult failed;
            failed.success = false;
            failed.orderId = order->getId();
            failed.message = e.what();
            results.push_back(failed);
        }
        if (!results.back().success) {
            // Could not assign (no agent) - stop trying further orders this
            // pass since agent availability won't spontaneously improve.
            break;
        }
    }
    return results;
}

DispatchResult DispatchEngine::manualAssign(const std::string& orderId, const std::string& agentId) {
    Order* order = orderManager.getOrder(orderId);
    if (!order) throw OrderNotFoundException("Order not found: " + orderId);

    DeliveryAgent* agent = agentManager.getAgent(agentId);
    if (!agent) throw AgentNotFoundException("Agent not found: " + agentId);

    if (order->getStatus() != OrderStatus::READY && order->getStatus() != OrderStatus::ASSIGNED) {
        throw InvalidRequestException("Order " + orderId + " cannot be (re)assigned from status " +
                                       orderStatusToString(order->getStatus()));
    }

    if (agent->getActiveOrders() >= agent->getMaxCapacity()) {
        throw AgentCapacityExceededException("Agent " + agentId + " is already at maximum capacity");
    }

    // If this is a reassignment away from a previous agent, free that agent up.
    if (order->getStatus() == OrderStatus::ASSIGNED && !order->getAssignedAgentId().empty() &&
        order->getAssignedAgentId() != agentId) {
        DeliveryAgent* previous = agentManager.getAgent(order->getAssignedAgentId());
        if (previous) previous->decrementActiveOrders();
        order->unassignForReassignment();
    }

    CandidateScore breakdown = score(*order, *agent);

    agent->incrementActiveOrders();
    order->assignAgent(agent->getId());

    double speed = agent->getVehicle().averageSpeedKmph();
    int etaMinutes = static_cast<int>((breakdown.distanceKm / speed) * 60.0) + 10;
    order->setEstimatedEtaMinutes(std::max(5, etaMinutes));

    DispatchResult result;
    result.success = true;
    result.orderId = orderId;
    result.agentId = agentId;
    result.message = "Order " + orderId + " manually assigned to " + agent->getName();
    result.candidates = {breakdown};
    return result;
}
