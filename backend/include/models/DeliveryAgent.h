#pragma once
#include <memory>
#include <string>
#include "User.h"
#include "Vehicle.h"
#include "Location.h"
#include "../utils/Config.h"

enum class AgentStatus { AVAILABLE, BUSY, OFFLINE };

inline std::string agentStatusToString(AgentStatus s) {
    switch (s) {
        case AgentStatus::AVAILABLE: return "AVAILABLE";
        case AgentStatus::BUSY: return "BUSY";
        case AgentStatus::OFFLINE: return "OFFLINE";
    }
    return "UNKNOWN";
}

inline AgentStatus agentStatusFromString(const std::string& s) {
    if (s == "AVAILABLE") return AgentStatus::AVAILABLE;
    if (s == "BUSY") return AgentStatus::BUSY;
    return AgentStatus::OFFLINE;
}

// ============================================================================
// DeliveryAgent.h
// Composition: a DeliveryAgent *has a* Vehicle (unique_ptr, exclusive
// ownership -> demonstrates smart pointer usage instead of raw new/delete).
// ============================================================================
class DeliveryAgent : public User {
private:
    std::unique_ptr<Vehicle> vehicle;
    Location currentLocation;
    AgentStatus status;
    int activeOrders;
    int maxCapacity;
    double rating;          // 0.0 - 5.0
    int totalDeliveries;
    int successfulDeliveries;
    double todayEarnings;

public:
    DeliveryAgent(std::string id_, std::string name_, std::string email_, std::string phone_,
                  std::unique_ptr<Vehicle> vehicle_, Location location_,
                  double rating_ = 4.5, int maxCapacity_ = config::DEFAULT_AGENT_MAX_CAPACITY)
        : User(std::move(id_), std::move(name_), std::move(email_), std::move(phone_)),
          vehicle(std::move(vehicle_)),
          currentLocation(std::move(location_)),
          status(AgentStatus::AVAILABLE),
          activeOrders(0),
          maxCapacity(maxCapacity_),
          rating(rating_),
          totalDeliveries(0),
          successfulDeliveries(0),
          todayEarnings(0.0) {}

    UserRole role() const override { return UserRole::DELIVERY_AGENT; }

    // ---- Accessors ----
    const Vehicle& getVehicle() const { return *vehicle; }
    const Location& getLocation() const { return currentLocation; }
    void setLocation(const Location& loc) { currentLocation = loc; }

    AgentStatus getStatus() const { return status; }
    int getActiveOrders() const { return activeOrders; }
    int getMaxCapacity() const { return maxCapacity; }
    double getRating() const { return rating; }
    int getTotalDeliveries() const { return totalDeliveries; }
    int getSuccessfulDeliveries() const { return successfulDeliveries; }
    double getTodayEarnings() const { return todayEarnings; }

    bool isAvailableForAssignment() const {
        return status == AgentStatus::AVAILABLE && activeOrders < maxCapacity;
    }

    void setStatus(AgentStatus s) { status = s; }

    // Called by DispatchEngine/AgentManager when an order is assigned.
    void incrementActiveOrders() {
        activeOrders++;
        if (activeOrders >= maxCapacity) status = AgentStatus::BUSY;
    }

    // Called when a delivery completes or is reassigned away.
    void decrementActiveOrders() {
        if (activeOrders > 0) activeOrders--;
        if (activeOrders < maxCapacity && status == AgentStatus::BUSY) {
            status = AgentStatus::AVAILABLE;
        }
    }

    void recordCompletedDelivery(bool successful, double earnings) {
        totalDeliveries++;
        if (successful) successfulDeliveries++;
        todayEarnings += earnings;
    }

    void updateRating(double newRatingPoint) {
        // simple running average nudge toward the newest rating
        rating = (rating * 0.85) + (newRatingPoint * 0.15);
        if (rating > config::MAX_RATING) rating = config::MAX_RATING;
        if (rating < 0.0) rating = 0.0;
    }

    json toJson() const override {
        return json{
            {"id", id},
            {"name", name},
            {"email", email},
            {"phone", phone},
            {"role", userRoleToString(role())},
            {"vehicle", vehicle->toJson()},
            {"location", currentLocation.toJson()},
            {"status", agentStatusToString(status)},
            {"activeOrders", activeOrders},
            {"maxCapacity", maxCapacity},
            {"rating", rating},
            {"totalDeliveries", totalDeliveries},
            {"successfulDeliveries", successfulDeliveries},
            {"todayEarnings", todayEarnings}
        };
    }
};
