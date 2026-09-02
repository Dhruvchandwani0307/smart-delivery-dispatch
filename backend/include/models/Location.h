#pragma once
#include <cmath>
#include <string>
#include "../third_party/json.hpp"
#include "../utils/Config.h"

using json = nlohmann::json;

// ============================================================================
// Location.h
// Simple value type representing a lat/lng point plus a human readable zone
// label (A, B, C...). Provides haversine distance so the DispatchEngine can
// compute real "distanceScore" instead of a placeholder.
// ============================================================================
class Location {
public:
    double latitude;
    double longitude;
    std::string zone;   // e.g. "Zone-A", "Zone-B"
    std::string label;  // human readable address / area name

    Location() : latitude(0.0), longitude(0.0), zone("Zone-A"), label("Unknown") {}

    Location(double lat, double lng, std::string zoneLabel, std::string addressLabel = "")
        : latitude(lat), longitude(lng), zone(std::move(zoneLabel)), label(std::move(addressLabel)) {}

    // Haversine formula - returns distance in kilometers.
    double distanceTo(const Location& other) const {
        double lat1 = latitude * M_PI / 180.0;
        double lat2 = other.latitude * M_PI / 180.0;
        double dLat = (other.latitude - latitude) * M_PI / 180.0;
        double dLon = (other.longitude - longitude) * M_PI / 180.0;

        double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
                   std::cos(lat1) * std::cos(lat2) *
                   std::sin(dLon / 2) * std::sin(dLon / 2);
        double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
        return config::EARTH_RADIUS_KM * c;
    }

    json toJson() const {
        return json{
            {"latitude", latitude},
            {"longitude", longitude},
            {"zone", zone},
            {"label", label}
        };
    }

    static Location fromJson(const json& j) {
        Location loc;
        loc.latitude = j.value("latitude", 0.0);
        loc.longitude = j.value("longitude", 0.0);
        loc.zone = j.value("zone", "Zone-A");
        loc.label = j.value("label", "Unknown");
        return loc;
    }
};
