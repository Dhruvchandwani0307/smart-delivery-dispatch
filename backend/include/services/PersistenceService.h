#pragma once
#include <fstream>
#include <string>
#include "../managers/OrderManager.h"
#include "../managers/AgentManager.h"
#include "../managers/CustomerManager.h"
#include "../managers/RestaurantManager.h"

// ============================================================================
// PersistenceService.h
// File-based persistence (structured JSON) as specified by the project brief
// as the initial persistence layer, with the code organised so a SQLite
// backend could be dropped in later behind the same interface (see the
// "Why JSON, not SQLite (yet)" note in README.md).
//
// Writes a consistent snapshot of the in-memory state to data/*.json so the
// system's current state can be inspected, backed up, or (in a future
// iteration) reloaded on startup.
// ============================================================================
class PersistenceService {
private:
    std::string dataDir;

    void writeJsonFile(const std::string& filename, const json& data) const {
        std::ofstream file(dataDir + "/" + filename);
        if (file.is_open()) {
            file << data.dump(2);
        }
    }

public:
    explicit PersistenceService(std::string dataDirectory) : dataDir(std::move(dataDirectory)) {}

    void saveSnapshot(OrderManager& orderMgr, AgentManager& agentMgr,
                       CustomerManager& customerMgr, RestaurantManager& restaurantMgr) const {
        json orders = json::array();
        for (auto* o : orderMgr.getAllOrders()) orders.push_back(o->toJson());
        writeJsonFile("orders.json", orders);

        json agents = json::array();
        for (auto* a : agentMgr.getAllAgents()) agents.push_back(a->toJson());
        writeJsonFile("agents.json", agents);

        json customers = json::array();
        for (auto* c : customerMgr.getAllCustomers()) customers.push_back(c->toJson());
        writeJsonFile("customers.json", customers);

        json restaurants = json::array();
        for (auto* r : restaurantMgr.getAllRestaurants()) restaurants.push_back(r->toJson());
        writeJsonFile("restaurants.json", restaurants);
    }
};
