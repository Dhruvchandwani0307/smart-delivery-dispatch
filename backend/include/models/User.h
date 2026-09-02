#pragma once
#include <string>
#include "../third_party/json.hpp"

using json = nlohmann::json;

enum class UserRole { ADMIN, CUSTOMER, DELIVERY_AGENT };

inline std::string userRoleToString(UserRole role) {
    switch (role) {
        case UserRole::ADMIN: return "ADMIN";
        case UserRole::CUSTOMER: return "CUSTOMER";
        case UserRole::DELIVERY_AGENT: return "DELIVERY_AGENT";
    }
    return "UNKNOWN";
}

// ============================================================================
// User.h
// Abstract base class for all system users. Demonstrates encapsulation
// (private data + public accessors) and abstraction (pure virtual role()).
//
//        User (abstract)
//        ├── Customer
//        ├── Admin
//        └── DeliveryAgent
// ============================================================================
class User {
protected:
    std::string id;
    std::string name;
    std::string email;
    std::string phone;

public:
    User(std::string id_, std::string name_, std::string email_, std::string phone_)
        : id(std::move(id_)), name(std::move(name_)), email(std::move(email_)), phone(std::move(phone_)) {}

    virtual ~User() = default;

    virtual UserRole role() const = 0;
    virtual json toJson() const = 0;

    const std::string& getId() const { return id; }
    const std::string& getName() const { return name; }
    const std::string& getEmail() const { return email; }
    const std::string& getPhone() const { return phone; }

    void setPhone(const std::string& p) { phone = p; }
};
