#pragma once
#include <vector>
#include <memory>
#include <random>
#include <algorithm>
#include "../include/managers/OrderManager.h"
#include "../include/managers/AgentManager.h"
#include "../include/managers/CustomerManager.h"
#include "../include/managers/RestaurantManager.h"
#include "../include/models/Customer.h"
#include "../include/models/DeliveryAgent.h"
#include "../include/models/Restaurant.h"
#include "../include/models/Order.h"
#include "../include/utils/IdGenerator.h"

// ============================================================================
// SeedData.h
// Populates the system with realistic demo data so the dashboard looks
// populated immediately after running the project:
//   10 customers, 10 delivery agents, 5 restaurants, 30 orders spanning
//   multiple statuses, priorities and zones.
// ============================================================================
class SeedData {
public:
    static void populate(OrderManager& orderMgr, AgentManager& agentMgr,
                          CustomerManager& customerMgr, RestaurantManager& restaurantMgr) {
        std::mt19937 rng(42); // fixed seed -> reproducible demo data

        // ---- Restaurants (5) ----
        struct RSeed { std::string name, cuisine, zone; double lat, lng; };
        std::vector<RSeed> restaurantSeeds = {
            {"Burger Point", "Fast Food", "Zone-A", 25.4520, 78.5680},
            {"Spice Villa", "North Indian", "Zone-B", 25.4480, 78.5720},
            {"Curry Junction", "South Indian", "Zone-A", 25.4550, 78.5650},
            {"Pizza Hub", "Italian", "Zone-C", 25.4610, 78.5590},
            {"Sweet Treats Bakery", "Bakery & Desserts", "Zone-B", 25.4495, 78.5705}
        };
        std::vector<std::string> restaurantIds;
        for (const auto& r : restaurantSeeds) {
            std::string id = IdGenerator::nextFor("RST");
            Location loc(r.lat, r.lng, r.zone, r.name + " Outlet");
            restaurantMgr.addRestaurant(Restaurant(id, r.name, r.cuisine, loc, 12 + (rng() % 15), 4.0 + (rng() % 10) / 10.0));
            restaurantIds.push_back(id);
        }

        // ---- Customers (10) ----
        std::vector<std::string> customerNames = {
            "Aarav Sharma", "Priya Patel", "Rohan Gupta", "Ananya Singh", "Vikram Mehta",
            "Sneha Reddy", "Arjun Nair", "Kavya Iyer", "Rahul Verma", "Ishita Joshi"
        };
        std::vector<std::string> customerIds;
        for (size_t i = 0; i < customerNames.size(); ++i) {
            std::string id = IdGenerator::nextFor("CUS");
            std::string email = customerNames[i].substr(0, customerNames[i].find(' '));
            std::transform(email.begin(), email.end(), email.begin(), ::tolower);
            std::string zone = (i % 3 == 0) ? "Zone-A" : (i % 3 == 1) ? "Zone-B" : "Zone-C";
            Location addr(25.45 + (i * 0.002), 78.57 - (i * 0.0015), zone, "House " + std::to_string(100 + i) + ", " + zone);
            customerMgr.addCustomer(Customer(id, customerNames[i], email + "@example.com",
                                              "+91-9" + std::to_string(700000000 + i * 111), addr));
            customerIds.push_back(id);
        }

        // ---- Delivery Agents (10) ----
        struct ASeed { std::string name, vehicleType, zone; double rating; };
        std::vector<ASeed> agentSeeds = {
            {"Rahul Yadav", "Bike", "Zone-A", 4.7},
            {"Aman Kumar", "Bike", "Zone-B", 4.9},
            {"Karan Malhotra", "Car", "Zone-A", 4.5},
            {"Deepak Chauhan", "Bike", "Zone-C", 4.2},
            {"Sanjay Tiwari", "Van", "Zone-B", 4.6},
            {"Vivek Pandey", "Bike", "Zone-A", 4.8},
            {"Manoj Kushwaha", "Bike", "Zone-C", 4.1},
            {"Suresh Rathore", "Car", "Zone-B", 4.4},
            {"Naveen Dubey", "Bike", "Zone-A", 4.3},
            {"Ajay Bisht", "Bike", "Zone-C", 4.9}
        };
        std::vector<std::string> agentIds;
        int regCounter = 1000;
        for (const auto& a : agentSeeds) {
            std::string id = IdGenerator::nextFor("AGT");
            std::string regNo = "UP-53-" + std::to_string(regCounter++);
            auto vehicle = makeVehicle(a.vehicleType, regNo);
            double lat = 25.448 + ((rng() % 100) / 5000.0);
            double lng = 78.565 + ((rng() % 100) / 5000.0);
            Location loc(lat, lng, a.zone, "Currently near " + a.zone);
            auto agent = std::make_unique<DeliveryAgent>(
                id, a.name, "", "+91-9" + std::to_string(800000000 + regCounter),
                std::move(vehicle), loc, a.rating, 3);
            agentMgr.registerAgent(std::move(agent));
            agentIds.push_back(id);
        }
        // Take a couple of agents offline / busy for a realistic mixed demo state
        agentMgr.getAgent(agentIds[3])->setStatus(AgentStatus::OFFLINE);
        agentMgr.getAgent(agentIds[7])->setStatus(AgentStatus::OFFLINE);

        // ---- Orders (30) across multiple statuses/priorities/zones ----
        std::vector<std::vector<OrderItem>> menuOptions = {
            {{"Cheeseburger", 2, 149.0}, {"Fries", 1, 89.0}},
            {{"Paneer Butter Masala", 1, 220.0}, {"Butter Naan", 3, 40.0}},
            {{"Masala Dosa", 2, 110.0}, {"Filter Coffee", 2, 45.0}},
            {{"Margherita Pizza", 1, 299.0}, {"Garlic Bread", 1, 99.0}},
            {{"Chocolate Pastry", 2, 79.0}, {"Cupcake", 4, 45.0}}
        };
        std::vector<OrderStatus> statusPool = {
            OrderStatus::PLACED, OrderStatus::CONFIRMED, OrderStatus::PREPARING,
            OrderStatus::READY, OrderStatus::ASSIGNED, OrderStatus::PICKED_UP,
            OrderStatus::OUT_FOR_DELIVERY, OrderStatus::DELIVERED, OrderStatus::DELIVERED,
            OrderStatus::CANCELLED
        };
        std::vector<OrderPriority> priorityPool = {
            OrderPriority::NORMAL, OrderPriority::NORMAL, OrderPriority::HIGH,
            OrderPriority::URGENT, OrderPriority::PERISHABLE
        };

        for (int i = 0; i < 30; ++i) {
            std::string orderId = IdGenerator::nextFor("ORD");
            const std::string& customerId = customerIds[i % customerIds.size()];
            const std::string& restaurantId = restaurantIds[i % restaurantIds.size()];
            Customer* customer = customerMgr.getCustomer(customerId);

            OrderPriority priority = priorityPool[i % priorityPool.size()];
            Payment payment(0.0, (i % 2 == 0) ? PaymentMethod::UPI : PaymentMethod::CASH_ON_DELIVERY);

            Order order(orderId, customerId, restaurantId, menuOptions[i % menuOptions.size()],
                        customer->getAddress(), priority, payment);

            OrderStatus targetStatus = statusPool[i % statusPool.size()];
            // Walk the order through valid transitions up to targetStatus so
            // the state machine invariants are respected even for seed data.
            static const std::vector<OrderStatus> path = {
                OrderStatus::CONFIRMED, OrderStatus::PREPARING, OrderStatus::READY,
                OrderStatus::ASSIGNED, OrderStatus::PICKED_UP, OrderStatus::OUT_FOR_DELIVERY,
                OrderStatus::DELIVERED
            };

            std::string assignedAgent;
            // PLACED is the order's constructed initial state and is not
            // part of `path`, so it needs no transitions at all. Walking
            // `path` unconditionally for a PLACED target would otherwise
            // run every step (since PLACED never matches) and leave every
            // "PLACED" seed order actually DELIVERED - guard against that.
            for (OrderStatus step : path) {
                if (targetStatus == OrderStatus::CANCELLED || targetStatus == OrderStatus::PLACED) break;
                if (step == OrderStatus::ASSIGNED) {
                    // Picked with the RNG rather than a formula of `i`: both
                    // the target status and the restaurant/customer index
                    // already cycle with period 10, so any fixed-stride
                    // formula here would stay permanently correlated with
                    // status across all three 10-order cycles (every agent
                    // would always land on the same single status). A
                    // random pick spreads deliveries realistically across
                    // agents and statuses.
                    assignedAgent = agentIds[rng() % agentIds.size()];
                    order.assignAgent(assignedAgent);
                    DeliveryAgent* ag = agentMgr.getAgent(assignedAgent);
                    if (ag && ag->getStatus() != AgentStatus::OFFLINE) ag->incrementActiveOrders();
                    order.setEstimatedEtaMinutes(20 + (i % 20));
                } else {
                    order.transitionTo(step);
                }
                if (step == targetStatus) break;
            }
            if (targetStatus == OrderStatus::CANCELLED) {
                order.transitionTo(OrderStatus::CANCELLED);
            }
            if (targetStatus == OrderStatus::DELIVERED && !assignedAgent.empty()) {
                DeliveryAgent* ag = agentMgr.getAgent(assignedAgent);
                if (ag) {
                    ag->decrementActiveOrders();
                    ag->recordCompletedDelivery(true, order.getTotalAmount() * 0.12);
                }
            }

            Order* stored = orderMgr.createOrder(order);
            customer->addOrder(orderId);
            if (stored->getStatus() == OrderStatus::READY) {
                orderMgr.enqueueForDispatch(orderId);
            }
        }
    }
};
