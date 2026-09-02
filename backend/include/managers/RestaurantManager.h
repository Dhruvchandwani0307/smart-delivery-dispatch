#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include "../models/Restaurant.h"

// ============================================================================
// RestaurantManager.h
// ============================================================================
class RestaurantManager {
private:
    std::unordered_map<std::string, Restaurant> restaurants;
    std::vector<std::string> insertionOrder;

public:
    Restaurant* addRestaurant(const Restaurant& r) {
        restaurants.emplace(r.getId(), r);
        insertionOrder.push_back(r.getId());
        return &restaurants.at(r.getId());
    }

    Restaurant* getRestaurant(const std::string& id) {
        auto it = restaurants.find(id);
        if (it == restaurants.end()) return nullptr;
        return &it->second;
    }

    std::vector<Restaurant*> getAllRestaurants() {
        std::vector<Restaurant*> result;
        for (const auto& id : insertionOrder) result.push_back(&restaurants.at(id));
        return result;
    }

    size_t size() const { return restaurants.size(); }
};
