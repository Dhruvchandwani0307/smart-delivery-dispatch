#pragma once
#include <vector>
#include <string>
#include <deque>
#include "../third_party/json.hpp"

using json = nlohmann::json;

enum class NotificationType { INFO, SUCCESS, WARNING, ERROR };

inline std::string notificationTypeToString(NotificationType t) {
    switch (t) {
        case NotificationType::INFO: return "INFO";
        case NotificationType::SUCCESS: return "SUCCESS";
        case NotificationType::WARNING: return "WARNING";
        case NotificationType::ERROR: return "ERROR";
    }
    return "INFO";
}

struct NotificationEntry {
    std::string message;
    NotificationType type;
    std::string targetRole; // "ADMIN", "CUSTOMER", "DELIVERY_AGENT", or "ALL"
    std::string targetId;   // specific user id, or empty for broadcast

    json toJson() const {
        return json{
            {"message", message},
            {"type", notificationTypeToString(type)},
            {"targetRole", targetRole},
            {"targetId", targetId}
        };
    }
};

// ============================================================================
// NotificationService.h
// Keeps a rolling log of the most recent system notifications (toast-style
// events) so the frontend can poll /api/notifications and surface real
// backend events instead of faking them client-side.
// ============================================================================
class NotificationService {
private:
    std::deque<NotificationEntry> log;
    static constexpr size_t MAX_ENTRIES = 200;

public:
    void push(const std::string& message, NotificationType type,
              const std::string& targetRole = "ALL", const std::string& targetId = "") {
        log.push_front(NotificationEntry{message, type, targetRole, targetId});
        while (log.size() > MAX_ENTRIES) log.pop_back();
    }

    std::vector<NotificationEntry> recent(size_t limit = 20) const {
        std::vector<NotificationEntry> result;
        for (const auto& entry : log) {
            if (result.size() >= limit) break;
            result.push_back(entry);
        }
        return result;
    }
};
