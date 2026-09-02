// ============================================================================
// main.cpp
// Smart Delivery Dispatch System - Backend Entry Point
//
// This file wires together the C++ business-logic layer (models, managers,
// services) with a REST API exposed via cpp-httplib. The API layer here is
// intentionally "thin" - it parses requests, calls into the managers /
// Dispatcher, and serialises responses to JSON. All real business logic
// (state machine rules, scoring, capacity checks) lives in the model and
// service classes under include/.
// ============================================================================

#include "../include/third_party/httplib.h"
#include "../include/third_party/json.hpp"

#include "../include/managers/OrderManager.h"
#include "../include/managers/AgentManager.h"
#include "../include/managers/CustomerManager.h"
#include "../include/managers/RestaurantManager.h"
#include "../include/managers/AnalyticsManager.h"

#include "../include/services/Dispatcher.h"
#include "../include/services/NotificationService.h"
#include "../include/services/PersistenceService.h"

#include "../include/utils/Exceptions.h"
#include "../include/utils/IdGenerator.h"
#include "../include/utils/Config.h"

#include "../../backend/database/SeedData.h"

#include <iostream>
#include <mutex>

using json = nlohmann::json;

// A single global mutex guards all mutable state. This is a demo/portfolio
// project (not a high-throughput production service), so a coarse lock is
// the simplest correct option and keeps the STL containers above safe
// under httplib's threaded request handling.
static std::mutex g_stateMutex;

// ---------------------------------------------------------------------------
// Small helpers for consistent JSON responses
// ---------------------------------------------------------------------------
static void sendJson(httplib::Response& res, int status, const json& body) {
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

static void sendSuccess(httplib::Response& res, const json& data, int status = 200) {
    json body = {{"success", true}};
    if (data.is_object()) {
        for (auto it = data.begin(); it != data.end(); ++it) body[it.key()] = it.value();
    } else {
        body["data"] = data;
    }
    sendJson(res, status, body);
}

static void sendError(httplib::Response& res, int status, const std::string& message) {
    sendJson(res, status, json{{"success", false}, {"message", message}});
}

int main() {
    // ---- Wire up the business logic layer ----
    OrderManager orderManager;
    AgentManager agentManager;
    CustomerManager customerManager;
    RestaurantManager restaurantManager;
    AnalyticsManager analyticsManager(orderManager, agentManager);
    NotificationService notificationService;
    Dispatcher dispatcher(agentManager, orderManager, restaurantManager, notificationService);
    PersistenceService persistenceService("../../data");

    std::cout << "Seeding demo data...\n";
    SeedData::populate(orderManager, agentManager, customerManager, restaurantManager);
    persistenceService.saveSnapshot(orderManager, agentManager, customerManager, restaurantManager);
    std::cout << "Seed complete: "
              << orderManager.size() << " orders, "
              << agentManager.size() << " agents, "
              << customerManager.size() << " customers, "
              << restaurantManager.size() << " restaurants.\n";

    httplib::Server server;

    // ---- CORS (frontend may be opened as a static file / different port) ----
    server.set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return httplib::Server::HandlerResponse::Unhandled;
    });
    server.Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    // Serve the frontend as static files so the whole app is one process.
    server.set_mount_point("/", "../../frontend");

    // =========================================================================
    // DASHBOARD / ANALYTICS
    // =========================================================================
    server.Get("/api/dashboard", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        sendSuccess(res, json{{"dashboard", analyticsManager.getDashboardSummary()}});
    });

    server.Get("/api/analytics", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        sendSuccess(res, json{{"analytics", analyticsManager.getFullAnalytics()}});
    });

    server.Get("/api/notifications", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        size_t limit = 20;
        if (req.has_param("limit")) limit = std::stoul(req.get_param_value("limit"));
        json arr = json::array();
        for (const auto& n : notificationService.recent(limit)) arr.push_back(n.toJson());
        sendSuccess(res, json{{"notifications", arr}});
    });

    // =========================================================================
    // ORDERS
    // =========================================================================
    server.Get("/api/orders", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        std::vector<Order*> orders;

        if (req.has_param("status")) {
            orders = orderManager.getOrdersByStatus(orderStatusFromString(req.get_param_value("status")));
        } else if (req.has_param("customerId")) {
            orders = orderManager.getOrdersByCustomer(req.get_param_value("customerId"));
        } else {
            orders = orderManager.getAllOrders();
        }

        if (req.has_param("agentId")) {
            std::string agentId = req.get_param_value("agentId");
            std::vector<Order*> filtered;
            for (auto* o : orders) if (o->getAssignedAgentId() == agentId) filtered.push_back(o);
            orders = filtered;
        }

        json arr = json::array();
        for (auto* o : orders) {
            json oj = o->toJson();
            Restaurant* r = restaurantManager.getRestaurant(o->getRestaurantId());
            Customer* c = customerManager.getCustomer(o->getCustomerId());
            if (r) oj["restaurantName"] = r->getName();
            if (c) oj["customerName"] = c->getName();
            if (!o->getAssignedAgentId().empty()) {
                DeliveryAgent* a = agentManager.getAgent(o->getAssignedAgentId());
                if (a) oj["agentName"] = a->getName();
            }
            arr.push_back(oj);
        }
        sendSuccess(res, json{{"orders", arr}, {"count", arr.size()}});
    });

    server.Get(R"(/api/orders/([\w-]+))", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        std::string id = req.matches[1];
        Order* o = orderManager.getOrder(id);
        if (!o) { sendError(res, 404, "Order not found: " + id); return; }
        sendSuccess(res, json{{"order", o->toJson()}});
    });

    server.Post("/api/orders", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        try {
            json body = json::parse(req.body);
            std::string customerId = body.at("customerId").get<std::string>();
            std::string restaurantId = body.at("restaurantId").get<std::string>();

            Customer* customer = customerManager.getCustomer(customerId);
            if (!customer) { sendError(res, 404, "Customer not found: " + customerId); return; }
            Restaurant* restaurant = restaurantManager.getRestaurant(restaurantId);
            if (!restaurant) { sendError(res, 404, "Restaurant not found: " + restaurantId); return; }

            std::vector<OrderItem> items;
            for (const auto& itemJson : body.at("items")) items.push_back(OrderItem::fromJson(itemJson));
            if (items.empty()) { sendError(res, 400, "Order must contain at least one item"); return; }

            OrderPriority priority = orderPriorityFromString(body.value("priority", "NORMAL"));
            Payment payment(0.0, paymentMethodFromString(body.value("paymentMethod", "CASH_ON_DELIVERY")));

            Location deliveryLoc = body.contains("deliveryLocation")
                ? Location::fromJson(body["deliveryLocation"]) : customer->getAddress();

            std::string orderId = IdGenerator::nextFor("ORD");
            Order order(orderId, customerId, restaurantId, items, deliveryLoc, priority, payment);

            Order* stored = orderManager.createOrder(order);
            customer->addOrder(orderId);
            notificationService.push("New order " + orderId + " placed at " + restaurant->getName(),
                                      NotificationType::INFO, "ADMIN");

            sendSuccess(res, json{{"order", stored->toJson()}}, 201);
        } catch (const json::exception& e) {
            sendError(res, 400, std::string("Invalid request body: ") + e.what());
        } catch (const std::exception& e) {
            sendError(res, 400, e.what());
        }
    });

    server.Put(R"(/api/orders/([\w-]+)/status)", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        std::string id = req.matches[1];
        Order* o = orderManager.getOrder(id);
        if (!o) { sendError(res, 404, "Order not found: " + id); return; }
        try {
            json body = json::parse(req.body);
            OrderStatus newStatus = orderStatusFromString(body.at("status").get<std::string>());

            o->transitionTo(newStatus);

            if (newStatus == OrderStatus::READY) {
                orderManager.enqueueForDispatch(id);
                notificationService.push("Order " + id + " is READY and waiting for dispatch",
                                          NotificationType::INFO, "ADMIN");
            }
            if (newStatus == OrderStatus::DELIVERED && !o->getAssignedAgentId().empty()) {
                DeliveryAgent* agent = agentManager.getAgent(o->getAssignedAgentId());
                if (agent) {
                    agent->decrementActiveOrders();
                    agent->recordCompletedDelivery(true, o->getTotalAmount() * 0.12);
                }
                notificationService.push("Order " + id + " delivered successfully!",
                                          NotificationType::SUCCESS, "CUSTOMER");
            }
            sendSuccess(res, json{{"order", o->toJson()}});
        } catch (const InvalidOrderTransition& e) {
            sendError(res, 409, e.what());
        } catch (const json::exception& e) {
            sendError(res, 400, std::string("Invalid request body: ") + e.what());
        }
    });

    server.Post(R"(/api/orders/([\w-]+)/cancel)", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        std::string id = req.matches[1];
        Order* o = orderManager.getOrder(id);
        if (!o) { sendError(res, 404, "Order not found: " + id); return; }
        if (!o->isCancellable()) {
            sendError(res, 409, "Order " + id + " can no longer be cancelled (status: " +
                      orderStatusToString(o->getStatus()) + ")");
            return;
        }
        if (!o->getAssignedAgentId().empty()) {
            DeliveryAgent* agent = agentManager.getAgent(o->getAssignedAgentId());
            if (agent) agent->decrementActiveOrders();
        }
        o->transitionTo(OrderStatus::CANCELLED);
        notificationService.push("Order " + id + " was cancelled", NotificationType::WARNING, "ADMIN");
        sendSuccess(res, json{{"order", o->toJson()}});
    });

    server.Post(R"(/api/orders/([\w-]+)/dispatch)", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        std::string id = req.matches[1];
        try {
            DispatchResult result = dispatcher.dispatchOne(id);
            json candidatesJson = json::array();
            for (const auto& c : result.candidates) {
                candidatesJson.push_back({
                    {"agentId", c.agentId}, {"agentName", c.agentName},
                    {"distanceKm", c.distanceKm}, {"distanceScore", c.distanceScore},
                    {"workloadScore", c.workloadScore}, {"ratingScore", c.ratingScore},
                    {"priorityAdjustment", c.priorityAdjustment}, {"zoneBonus", c.zoneBonus},
                    {"finalScore", c.finalScore}
                });
            }
            json resultJson = {
                {"success", result.success}, {"orderId", result.orderId},
                {"agentId", result.agentId}, {"message", result.message},
                {"candidates", candidatesJson}
            };
            if (!result.success) { sendJson(res, 409, json{{"success", false}, {"message", result.message}}); return; }
            sendSuccess(res, json{{"dispatch", resultJson}});
        } catch (const OrderNotFoundException& e) {
            sendError(res, 404, e.what());
        } catch (const InvalidRequestException& e) {
            sendError(res, 409, e.what());
        } catch (const std::exception& e) {
            sendError(res, 500, e.what());
        }
    });

    server.Post(R"(/api/orders/([\w-]+)/reassign)", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        std::string id = req.matches[1];
        try {
            json body = json::parse(req.body);
            std::string agentId = body.at("agentId").get<std::string>();
            DispatchResult result = dispatcher.manualAssign(id, agentId);
            sendSuccess(res, json{{"order", orderManager.getOrder(id)->toJson()}, {"message", result.message}});
        } catch (const OrderNotFoundException& e) {
            sendError(res, 404, e.what());
        } catch (const AgentNotFoundException& e) {
            sendError(res, 404, e.what());
        } catch (const AgentCapacityExceededException& e) {
            sendError(res, 409, e.what());
        } catch (const InvalidRequestException& e) {
            sendError(res, 409, e.what());
        } catch (const json::exception& e) {
            sendError(res, 400, std::string("Invalid request body: ") + e.what());
        }
    });

    // =========================================================================
    // DISPATCH CENTER
    // =========================================================================
    server.Get(R"(/api/dispatch/([\w-]+))", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        std::string id = req.matches[1];
        Order* o = orderManager.getOrder(id);
        if (!o) { sendError(res, 404, "Order not found: " + id); return; }

        auto candidates = dispatcher.getEngine().evaluateCandidates(*o);
        json arr = json::array();
        for (size_t i = 0; i < candidates.size(); ++i) {
            const auto& c = candidates[i];
            arr.push_back({
                {"agentId", c.agentId}, {"agentName", c.agentName},
                {"distanceKm", c.distanceKm}, {"distanceScore", c.distanceScore},
                {"workloadScore", c.workloadScore}, {"ratingScore", c.ratingScore},
                {"priorityAdjustment", c.priorityAdjustment}, {"zoneBonus", c.zoneBonus},
                {"finalScore", c.finalScore}, {"recommended", i == 0}
            });
        }
        sendSuccess(res, json{{"orderId", id}, {"candidates", arr}});
    });

    server.Get("/api/dispatch-queue", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        json arr = json::array();
        for (auto* o : orderManager.peekDispatchQueue()) {
            json oj = o->toJson();
            Restaurant* r = restaurantManager.getRestaurant(o->getRestaurantId());
            if (r) oj["restaurantName"] = r->getName();
            arr.push_back(oj);
        }
        sendSuccess(res, json{{"queue", arr}});
    });

    server.Post("/api/dispatch/auto", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        auto results = dispatcher.autoDispatchAll();
        json arr = json::array();
        int assigned = 0;
        for (const auto& r : results) {
            if (r.success) assigned++;
            arr.push_back({{"orderId", r.orderId}, {"success", r.success},
                            {"agentId", r.agentId}, {"message", r.message}});
        }
        sendSuccess(res, json{{"results", arr}, {"assignedCount", assigned}});
    });

    // =========================================================================
    // AGENTS
    // =========================================================================
    server.Get("/api/agents", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        json arr = json::array();
        for (auto* a : agentManager.getAllAgents()) {
            if (req.has_param("status") && agentStatusToString(a->getStatus()) != req.get_param_value("status")) continue;
            arr.push_back(a->toJson());
        }
        sendSuccess(res, json{{"agents", arr}, {"count", arr.size()}});
    });

    server.Get(R"(/api/agents/([\w-]+))", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        std::string id = req.matches[1];
        DeliveryAgent* a = agentManager.getAgent(id);
        if (!a) { sendError(res, 404, "Agent not found: " + id); return; }
        sendSuccess(res, json{{"agent", a->toJson()}});
    });

    server.Post("/api/agents", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        try {
            json body = json::parse(req.body);
            std::string name = body.at("name").get<std::string>();
            std::string phone = body.value("phone", "");
            std::string vehicleType = body.value("vehicleType", "Bike");
            std::string regNo = body.value("registrationNumber", "NEW-0000");
            double lat = body.value("latitude", 25.45);
            double lng = body.value("longitude", 78.57);
            std::string zone = body.value("zone", "Zone-A");
            double rating = body.value("rating", 4.5);
            int capacity = body.value("maxCapacity", config::DEFAULT_AGENT_MAX_CAPACITY);

            std::string id = IdGenerator::nextFor("AGT");
            auto vehicle = makeVehicle(vehicleType, regNo);
            Location loc(lat, lng, zone, "Zone " + zone);
            auto agent = std::make_unique<DeliveryAgent>(id, name, "", phone, std::move(vehicle), loc, rating, capacity);
            DeliveryAgent* raw = agentManager.registerAgent(std::move(agent));

            notificationService.push("New delivery agent " + name + " onboarded", NotificationType::INFO, "ADMIN");
            sendSuccess(res, json{{"agent", raw->toJson()}}, 201);
        } catch (const json::exception& e) {
            sendError(res, 400, std::string("Invalid request body: ") + e.what());
        }
    });

    server.Put(R"(/api/agents/([\w-]+)/availability)", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        std::string id = req.matches[1];
        DeliveryAgent* a = agentManager.getAgent(id);
        if (!a) { sendError(res, 404, "Agent not found: " + id); return; }
        try {
            json body = json::parse(req.body);
            AgentStatus newStatus = agentStatusFromString(body.at("status").get<std::string>());
            if (newStatus == AgentStatus::AVAILABLE && a->getActiveOrders() >= a->getMaxCapacity()) {
                sendError(res, 409, "Agent is at maximum capacity and cannot be marked AVAILABLE");
                return;
            }
            a->setStatus(newStatus);
            sendSuccess(res, json{{"agent", a->toJson()}});
        } catch (const json::exception& e) {
            sendError(res, 400, std::string("Invalid request body: ") + e.what());
        }
    });

    // =========================================================================
    // RESTAURANTS
    // =========================================================================
    server.Get("/api/restaurants", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        json arr = json::array();
        for (auto* r : restaurantManager.getAllRestaurants()) arr.push_back(r->toJson());
        sendSuccess(res, json{{"restaurants", arr}});
    });

    server.Post("/api/restaurants", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        try {
            json body = json::parse(req.body);
            std::string id = IdGenerator::nextFor("RST");
            Location loc = body.contains("location") ? Location::fromJson(body["location"]) : Location();
            Restaurant r(id, body.at("name").get<std::string>(), body.value("cuisine", "General"),
                         loc, body.value("avgPrepTimeMinutes", 15.0), body.value("rating", 4.0));
            Restaurant* stored = restaurantManager.addRestaurant(r);
            sendSuccess(res, json{{"restaurant", stored->toJson()}}, 201);
        } catch (const json::exception& e) {
            sendError(res, 400, std::string("Invalid request body: ") + e.what());
        }
    });

    // =========================================================================
    // CUSTOMERS
    // =========================================================================
    server.Get("/api/customers", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        json arr = json::array();
        for (auto* c : customerManager.getAllCustomers()) arr.push_back(c->toJson());
        sendSuccess(res, json{{"customers", arr}});
    });

    server.Get(R"(/api/customers/([\w-]+))", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        std::string id = req.matches[1];
        Customer* c = customerManager.getCustomer(id);
        if (!c) { sendError(res, 404, "Customer not found: " + id); return; }
        sendSuccess(res, json{{"customer", c->toJson()}});
    });

    std::cout << "\nSmart Delivery Dispatch System backend running at http://localhost:"
              << config::SERVER_PORT << "\n";
    std::cout << "Frontend served from the same origin at http://localhost:"
              << config::SERVER_PORT << "/index.html\n\n";

    // Most PaaS hosts (Render, Railway, Fly.io) inject a PORT env var and
    // expect the app to bind to it; fall back to the configured default for
    // local runs where no such env var is set.
    int port = config::SERVER_PORT;
    if (const char* envPort = std::getenv("PORT")) {
        try { port = std::stoi(envPort); } catch (...) { /* keep default */ }
    }

    server.listen("0.0.0.0", port);
    return 0;
}
