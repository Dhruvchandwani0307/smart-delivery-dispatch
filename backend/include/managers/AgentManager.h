#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>
#include "../models/DeliveryAgent.h"

// ============================================================================
// AgentManager.h
// Owns all DeliveryAgent instances. Agents are stored as unique_ptr (each
// DeliveryAgent already owns a unique_ptr<Vehicle>, so DeliveryAgent itself
// is non-copyable) inside an unordered_map keyed by agent ID for O(1)
// average lookup, matching the "unordered_map for agent lookup by ID"
// requirement.
// ============================================================================
class AgentManager {
private:
    std::unordered_map<std::string, std::unique_ptr<DeliveryAgent>> agents;
    std::vector<std::string> insertionOrder; // preserves a stable listing order

public:
    DeliveryAgent* registerAgent(std::unique_ptr<DeliveryAgent> agent) {
        const std::string id = agent->getId();
        DeliveryAgent* raw = agent.get();
        agents[id] = std::move(agent);
        insertionOrder.push_back(id);
        return raw;
    }

    DeliveryAgent* getAgent(const std::string& id) {
        auto it = agents.find(id);
        if (it == agents.end()) return nullptr;
        return it->second.get();
    }

    const DeliveryAgent* getAgent(const std::string& id) const {
        auto it = agents.find(id);
        if (it == agents.end()) return nullptr;
        return it->second.get();
    }

    bool exists(const std::string& id) const { return agents.find(id) != agents.end(); }

    std::vector<DeliveryAgent*> getAllAgents() {
        std::vector<DeliveryAgent*> result;
        result.reserve(insertionOrder.size());
        for (const auto& id : insertionOrder) result.push_back(agents.at(id).get());
        return result;
    }

    std::vector<DeliveryAgent*> getAvailableAgents() {
        std::vector<DeliveryAgent*> result;
        for (const auto& id : insertionOrder) {
            DeliveryAgent* a = agents.at(id).get();
            if (a->isAvailableForAssignment()) result.push_back(a);
        }
        return result;
    }

    size_t countByStatus(AgentStatus status) const {
        size_t count = 0;
        for (const auto& [id, agent] : agents) {
            if (agent->getStatus() == status) count++;
        }
        return count;
    }

    size_t size() const { return agents.size(); }
};
