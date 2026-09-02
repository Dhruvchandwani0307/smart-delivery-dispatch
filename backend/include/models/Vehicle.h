#pragma once
#include <string>
#include <memory>
#include "../third_party/json.hpp"

using json = nlohmann::json;

// ============================================================================
// Vehicle.h
// Demonstrates classic inheritance + polymorphism.
//
//        Vehicle (abstract base)
//        ├── Bike
//        ├── Car
//        └── Van
//
// Each subclass overrides averageSpeedKmph() and capacityFactor(), both of
// which are consumed by DispatchEngine when estimating ETA / suitability.
// ============================================================================
class Vehicle {
protected:
    std::string registrationNumber;

public:
    explicit Vehicle(std::string regNo) : registrationNumber(std::move(regNo)) {}
    virtual ~Vehicle() = default;

    virtual std::string type() const = 0;
    virtual double averageSpeedKmph() const = 0;
    // How many simultaneous orders this vehicle type can reasonably carry.
    virtual int capacityFactor() const = 0;

    const std::string& getRegistrationNumber() const { return registrationNumber; }

    virtual json toJson() const {
        return json{
            {"type", type()},
            {"registrationNumber", registrationNumber},
            {"averageSpeedKmph", averageSpeedKmph()},
            {"capacityFactor", capacityFactor()}
        };
    }
};

class Bike : public Vehicle {
public:
    explicit Bike(std::string regNo) : Vehicle(std::move(regNo)) {}
    std::string type() const override { return "Bike"; }
    double averageSpeedKmph() const override { return 30.0; }
    int capacityFactor() const override { return 2; }
};

class Car : public Vehicle {
public:
    explicit Car(std::string regNo) : Vehicle(std::move(regNo)) {}
    std::string type() const override { return "Car"; }
    double averageSpeedKmph() const override { return 25.0; }
    int capacityFactor() const override { return 4; }
};

class Van : public Vehicle {
public:
    explicit Van(std::string regNo) : Vehicle(std::move(regNo)) {}
    std::string type() const override { return "Van"; }
    double averageSpeedKmph() const override { return 20.0; }
    int capacityFactor() const override { return 6; }
};

// Factory helper - keeps "if type == ..." logic in one place.
inline std::unique_ptr<Vehicle> makeVehicle(const std::string& type, const std::string& regNo) {
    if (type == "Bike") return std::make_unique<Bike>(regNo);
    if (type == "Car") return std::make_unique<Car>(regNo);
    if (type == "Van") return std::make_unique<Van>(regNo);
    return std::make_unique<Bike>(regNo); // sensible default
}
