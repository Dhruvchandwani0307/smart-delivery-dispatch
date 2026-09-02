#pragma once
#include <vector>
#include <string>
#include "User.h"
#include "Location.h"

// ============================================================================
// Customer.h
// ============================================================================
class Customer : public User {
private:
    Location defaultAddress;
    std::vector<std::string> orderIds; // history of order IDs placed by this customer

public:
    Customer(std::string id_, std::string name_, std::string email_, std::string phone_, Location address)
        : User(std::move(id_), std::move(name_), std::move(email_), std::move(phone_)),
          defaultAddress(std::move(address)) {}

    UserRole role() const override { return UserRole::CUSTOMER; }

    const Location& getAddress() const { return defaultAddress; }
    void setAddress(const Location& loc) { defaultAddress = loc; }

    void addOrder(const std::string& orderId) { orderIds.push_back(orderId); }
    const std::vector<std::string>& getOrderIds() const { return orderIds; }

    json toJson() const override {
        return json{
            {"id", id},
            {"name", name},
            {"email", email},
            {"phone", phone},
            {"role", userRoleToString(role())},
            {"address", defaultAddress.toJson()},
            {"totalOrders", orderIds.size()}
        };
    }
};
