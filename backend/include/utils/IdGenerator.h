#pragma once
#include <string>
#include <atomic>
#include <sstream>
#include <iomanip>

// ============================================================================
// IdGenerator.h
// Thread-safe, monotonically increasing ID generator producing human
// readable, prefixed IDs such as ORD-1024, AGT-0007, CUS-0031.
// ============================================================================
class IdGenerator {
public:
    static std::string next(const std::string& prefix, int startAt = 1000) {
        static std::atomic<int> counter{startAt};
        int value = counter.fetch_add(1);
        std::ostringstream oss;
        oss << prefix << "-" << value;
        return oss.str();
    }

    // Separate counters per-prefix so ORD-, AGT-, CUS-, RST- all start fresh.
    static std::string nextFor(const std::string& prefix) {
        static std::atomic<int> orderCounter{1000};
        static std::atomic<int> agentCounter{1};
        static std::atomic<int> customerCounter{1};
        static std::atomic<int> restaurantCounter{1};
        static std::atomic<int> deliveryCounter{5000};

        int value;
        if (prefix == "ORD") value = orderCounter.fetch_add(1);
        else if (prefix == "AGT") value = agentCounter.fetch_add(1);
        else if (prefix == "CUS") value = customerCounter.fetch_add(1);
        else if (prefix == "RST") value = restaurantCounter.fetch_add(1);
        else if (prefix == "DEL") value = deliveryCounter.fetch_add(1);
        else value = 0;

        std::ostringstream oss;
        oss << prefix << "-" << std::setw(4) << std::setfill('0') << value;
        return oss.str();
    }
};
