#pragma once
#include <string>
#include "../third_party/json.hpp"

using json = nlohmann::json;

enum class PaymentMethod { CASH_ON_DELIVERY, UPI, CARD, WALLET };
enum class PaymentStatus { PENDING, PAID, FAILED, REFUNDED };

inline std::string paymentMethodToString(PaymentMethod m) {
    switch (m) {
        case PaymentMethod::CASH_ON_DELIVERY: return "CASH_ON_DELIVERY";
        case PaymentMethod::UPI: return "UPI";
        case PaymentMethod::CARD: return "CARD";
        case PaymentMethod::WALLET: return "WALLET";
    }
    return "UNKNOWN";
}

inline PaymentMethod paymentMethodFromString(const std::string& s) {
    if (s == "UPI") return PaymentMethod::UPI;
    if (s == "CARD") return PaymentMethod::CARD;
    if (s == "WALLET") return PaymentMethod::WALLET;
    return PaymentMethod::CASH_ON_DELIVERY;
}

inline std::string paymentStatusToString(PaymentStatus s) {
    switch (s) {
        case PaymentStatus::PENDING: return "PENDING";
        case PaymentStatus::PAID: return "PAID";
        case PaymentStatus::FAILED: return "FAILED";
        case PaymentStatus::REFUNDED: return "REFUNDED";
    }
    return "UNKNOWN";
}

// ============================================================================
// Payment.h
// ============================================================================
class Payment {
private:
    double amount;
    PaymentMethod method;
    PaymentStatus status;

public:
    Payment(double amount_ = 0.0, PaymentMethod method_ = PaymentMethod::CASH_ON_DELIVERY)
        : amount(amount_), method(method_), status(PaymentStatus::PENDING) {}

    double getAmount() const { return amount; }
    PaymentMethod getMethod() const { return method; }
    PaymentStatus getStatus() const { return status; }

    void markPaid() { status = PaymentStatus::PAID; }
    void markFailed() { status = PaymentStatus::FAILED; }
    void markRefunded() { status = PaymentStatus::REFUNDED; }

    json toJson() const {
        return json{
            {"amount", amount},
            {"method", paymentMethodToString(method)},
            {"status", paymentStatusToString(status)}
        };
    }

    static Payment fromJson(const json& j) {
        Payment p(j.value("amount", 0.0), paymentMethodFromString(j.value("method", "CASH_ON_DELIVERY")));
        return p;
    }
};
