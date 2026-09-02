#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <chrono>
#include <ctime>
#include "OrderManager.h"
#include "AgentManager.h"
#include "../third_party/json.hpp"

using json = nlohmann::json;

// ============================================================================
// AnalyticsManager.h
// Derives all dashboard / analytics numbers live from OrderManager and
// AgentManager state - nothing here is faked or randomly generated.
// ============================================================================
class AnalyticsManager {
private:
    OrderManager& orderManager;
    AgentManager& agentManager;

public:
    AnalyticsManager(OrderManager& orderMgr, AgentManager& agentMgr)
        : orderManager(orderMgr), agentManager(agentMgr) {}

    json getDashboardSummary() {
        auto orders = orderManager.getAllOrders();
        int total = (int)orders.size();
        int active = 0, deliveredToday = 0, cancelled = 0;
        double totalDeliveryMinutes = 0.0;
        int deliveredCount = 0;

        for (Order* o : orders) {
            if (o->isActive()) active++;
            if (o->getStatus() == OrderStatus::DELIVERED) {
                deliveredToday++;
                deliveredCount++;
                totalDeliveryMinutes += o->getEstimatedEtaMinutes();
            }
            if (o->getStatus() == OrderStatus::CANCELLED) cancelled++;
        }

        double avgDeliveryTime = deliveredCount > 0 ? (totalDeliveryMinutes / deliveredCount) : 0.0;
        double cancellationRate = total > 0 ? (100.0 * cancelled / total) : 0.0;

        int availableAgents = (int)agentManager.countByStatus(AgentStatus::AVAILABLE);

        return json{
            {"totalOrders", total},
            {"activeDeliveries", active},
            {"availableAgents", availableAgents},
            {"deliveredToday", deliveredToday},
            {"averageDeliveryTimeMinutes", avgDeliveryTime},
            {"cancellationRate", cancellationRate},
            {"totalAgents", (int)agentManager.size()}
        };
    }

    json getStatusDistribution() {
        std::unordered_map<std::string, int> counts;
        for (Order* o : orderManager.getAllOrders()) {
            counts[orderStatusToString(o->getStatus())]++;
        }
        json result = json::array();
        for (const auto& [status, count] : counts) {
            result.push_back({{"status", status}, {"count", count}});
        }
        return result;
    }

    json getPriorityDistribution() {
        std::unordered_map<std::string, int> counts;
        for (Order* o : orderManager.getAllOrders()) {
            counts[orderPriorityToString(o->getPriority())]++;
        }
        json result = json::array();
        for (const auto& [priority, count] : counts) {
            result.push_back({{"priority", priority}, {"count", count}});
        }
        return result;
    }

    json getOrdersPerHour() {
        // Buckets orders by the hour (0-23) they were placed.
        std::unordered_map<int, int> buckets;
        for (Order* o : orderManager.getAllOrders()) {
            std::time_t t = std::chrono::system_clock::to_time_t(o->getPlacedAt());
            std::tm* localTime = std::localtime(&t);
            buckets[localTime->tm_hour]++;
        }
        json result = json::array();
        for (int h = 0; h < 24; ++h) {
            if (buckets.count(h)) result.push_back({{"hour", h}, {"count", buckets[h]}});
        }
        return result;
    }

    json getAgentUtilization() {
        json result = json::array();
        for (DeliveryAgent* a : agentManager.getAllAgents()) {
            double utilization = a->getMaxCapacity() > 0
                ? (100.0 * a->getActiveOrders() / a->getMaxCapacity()) : 0.0;
            result.push_back({
                {"agentId", a->getId()},
                {"agentName", a->getName()},
                {"activeOrders", a->getActiveOrders()},
                {"maxCapacity", a->getMaxCapacity()},
                {"utilizationPercent", utilization},
                {"status", agentStatusToString(a->getStatus())}
            });
        }
        return result;
    }

    json getFullAnalytics() {
        return json{
            {"summary", getDashboardSummary()},
            {"statusDistribution", getStatusDistribution()},
            {"priorityDistribution", getPriorityDistribution()},
            {"ordersPerHour", getOrdersPerHour()},
            {"agentUtilization", getAgentUtilization()}
        };
    }
};
