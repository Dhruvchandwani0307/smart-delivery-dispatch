#pragma once
#include <string>
#include "Location.h"
#include "../third_party/json.hpp"

using json = nlohmann::json;

// ============================================================================
// Restaurant.h
// ============================================================================
class Restaurant {
private:
    std::string id;
    std::string name;
    std::string cuisine;
    Location location;
    double avgPrepTimeMinutes;
    double rating;

public:
    Restaurant(std::string id_, std::string name_, std::string cuisine_, Location loc,
               double avgPrepTimeMinutes_ = 15.0, double rating_ = 4.3)
        : id(std::move(id_)), name(std::move(name_)), cuisine(std::move(cuisine_)),
          location(std::move(loc)), avgPrepTimeMinutes(avgPrepTimeMinutes_), rating(rating_) {}

    const std::string& getId() const { return id; }
    const std::string& getName() const { return name; }
    const std::string& getCuisine() const { return cuisine; }
    const Location& getLocation() const { return location; }
    double getAvgPrepTimeMinutes() const { return avgPrepTimeMinutes; }
    double getRating() const { return rating; }

    json toJson() const {
        return json{
            {"id", id},
            {"name", name},
            {"cuisine", cuisine},
            {"location", location.toJson()},
            {"avgPrepTimeMinutes", avgPrepTimeMinutes},
            {"rating", rating}
        };
    }
};
