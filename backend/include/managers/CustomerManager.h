#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include "../models/Customer.h"

// ============================================================================
// CustomerManager.h
// ============================================================================
class CustomerManager {
private:
    std::unordered_map<std::string, Customer> customers;
    std::vector<std::string> insertionOrder;

public:
    Customer* addCustomer(const Customer& c) {
        customers.emplace(c.getId(), c);
        insertionOrder.push_back(c.getId());
        return &customers.at(c.getId());
    }

    Customer* getCustomer(const std::string& id) {
        auto it = customers.find(id);
        if (it == customers.end()) return nullptr;
        return &it->second;
    }

    bool exists(const std::string& id) const { return customers.find(id) != customers.end(); }

    std::vector<Customer*> getAllCustomers() {
        std::vector<Customer*> result;
        for (const auto& id : insertionOrder) result.push_back(&customers.at(id));
        return result;
    }

    size_t size() const { return customers.size(); }
};
