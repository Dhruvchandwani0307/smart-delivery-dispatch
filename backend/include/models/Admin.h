#pragma once
#include "User.h"

// ============================================================================
// Admin.h
// ============================================================================
class Admin : public User {
private:
    std::string accessLevel; // e.g. "SUPER", "OPS"

public:
    Admin(std::string id_, std::string name_, std::string email_, std::string phone_, std::string accessLevel_ = "SUPER")
        : User(std::move(id_), std::move(name_), std::move(email_), std::move(phone_)),
          accessLevel(std::move(accessLevel_)) {}

    UserRole role() const override { return UserRole::ADMIN; }
    const std::string& getAccessLevel() const { return accessLevel; }

    json toJson() const override {
        return json{
            {"id", id},
            {"name", name},
            {"email", email},
            {"phone", phone},
            {"role", userRoleToString(role())},
            {"accessLevel", accessLevel}
        };
    }
};
