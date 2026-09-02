// ============================================================================
// test_dispatch.cpp
// Covers the minimum test matrix requested in the project brief:
//   1. Agent selection
//   2. Priority handling
//   3. Order state transitions
//   4. Agent capacity
//   5. Dispatch scoring
//   6. Cancellation
//   7. No available agent case
// ============================================================================
#include "test_framework.h"

#include "../backend/include/managers/OrderManager.h"
#include "../backend/include/managers/AgentManager.h"
#include "../backend/include/managers/RestaurantManager.h"
#include "../backend/include/services/DispatchEngine.h"
#include "../backend/include/utils/Exceptions.h"

namespace {

// ---- Shared fixtures -------------------------------------------------------

Restaurant makeTestRestaurant() {
    Location loc(25.4520, 78.5680, "Zone-A", "Burger Point Outlet");
    return Restaurant("RST-0001", "Burger Point", "Fast Food", loc, 15.0, 4.5);
}

std::unique_ptr<DeliveryAgent> makeTestAgent(const std::string& id, const std::string& name,
                                              double lat, double lng, const std::string& zone,
                                              double rating, int activeOrders = 0, int maxCapacity = 3,
                                              AgentStatus status = AgentStatus::AVAILABLE) {
    Location loc(lat, lng, zone, "near " + zone);
    auto agent = std::make_unique<DeliveryAgent>(id, name, "", "9999999999",
                                                  makeVehicle("Bike", "UP-01-0001"), loc, rating, maxCapacity);
    for (int i = 0; i < activeOrders; ++i) agent->incrementActiveOrders();
    // Only force-override status when the caller explicitly asked for
    // something other than the default AVAILABLE, so that
    // incrementActiveOrders()'s own BUSY transition (once at capacity)
    // is not silently clobbered back to AVAILABLE.
    if (status != AgentStatus::AVAILABLE) agent->setStatus(status);
    return agent;
}

Order makeReadyOrder(const std::string& id, OrderPriority priority = OrderPriority::NORMAL) {
    Location deliveryLoc(25.46, 78.56, "Zone-A", "Test address");
    std::vector<OrderItem> items = {{"Test Item", 1, 100.0}};
    Payment payment(100.0, PaymentMethod::UPI);
    Order order(id, "CUS-0001", "RST-0001", items, deliveryLoc, priority, payment);
    order.transitionTo(OrderStatus::CONFIRMED);
    order.transitionTo(OrderStatus::PREPARING);
    order.transitionTo(OrderStatus::READY);
    return order;
}

} // namespace

int main() {
    // =========================================================================
    // 1. AGENT SELECTION - closer + less busy + higher rated agent should win
    // =========================================================================
    testfw::runSuite("Agent Selection", [] {
        AgentManager agentMgr;
        OrderManager orderMgr;
        RestaurantManager restaurantMgr;
        restaurantMgr.addRestaurant(makeTestRestaurant());

        // Rahul: close, idle, good rating
        agentMgr.registerAgent(makeTestAgent("AGT-0001", "Rahul", 25.4530, 78.5690, "Zone-A", 4.7, 0));
        // Aman: closer but already has 2 active orders (busier)
        agentMgr.registerAgent(makeTestAgent("AGT-0002", "Aman", 25.4525, 78.5685, "Zone-A", 4.9, 2));
        // Karan: farther away, idle, slightly lower rating
        agentMgr.registerAgent(makeTestAgent("AGT-0003", "Karan", 25.47, 78.60, "Zone-B", 4.5, 0));

        DispatchEngine engine(agentMgr, orderMgr, restaurantMgr);
        Order order = makeReadyOrder("ORD-TEST-1");

        auto candidates = engine.evaluateCandidates(order);
        CHECK(candidates.size() == 3, "All 3 agents evaluated as candidates");
        CHECK(candidates.front().agentId == "AGT-0001",
              "Rahul (close + idle + high rating) scores highest, not just the nearest agent");
    });

    // =========================================================================
    // 2. PRIORITY HANDLING - URGENT should score higher than NORMAL, all else equal
    // =========================================================================
    testfw::runSuite("Priority Handling", [] {
        AgentManager agentMgr;
        OrderManager orderMgr;
        RestaurantManager restaurantMgr;
        restaurantMgr.addRestaurant(makeTestRestaurant());
        agentMgr.registerAgent(makeTestAgent("AGT-0001", "Rahul", 25.4530, 78.5690, "Zone-A", 4.7, 0));

        DispatchEngine engine(agentMgr, orderMgr, restaurantMgr);

        Order normalOrder = makeReadyOrder("ORD-N", OrderPriority::NORMAL);
        Order urgentOrder = makeReadyOrder("ORD-U", OrderPriority::URGENT);

        DeliveryAgent* agent = agentMgr.getAgent("AGT-0001");
        CandidateScore normalScore = engine.score(normalOrder, *agent);
        CandidateScore urgentScore = engine.score(urgentOrder, *agent);

        CHECK(urgentScore.finalScore > normalScore.finalScore,
              "URGENT order scores higher than NORMAL for the same agent");
        CHECK(APPROX_EQ(urgentScore.priorityAdjustment - normalScore.priorityAdjustment, 20.0, 0.01),
              "Priority adjustment gap between URGENT and NORMAL matches configured bonus (20.0)");
    });

    // =========================================================================
    // 3. ORDER STATE TRANSITIONS - valid transitions succeed, invalid ones throw
    // =========================================================================
    testfw::runSuite("Order State Transitions", [] {
        Order order = makeReadyOrder("ORD-STATE-1"); // already walked to READY
        CHECK(order.getStatus() == OrderStatus::READY, "Order reaches READY via valid transition chain");

        bool threw = false;
        try {
            order.transitionTo(OrderStatus::DELIVERED); // READY -> DELIVERED is invalid (must go through ASSIGNED etc.)
        } catch (const InvalidOrderTransition&) {
            threw = true;
        }
        CHECK(threw, "Skipping intermediate states (READY -> DELIVERED) throws InvalidOrderTransition");

        order.assignAgent("AGT-0001");
        CHECK(order.getStatus() == OrderStatus::ASSIGNED, "assignAgent() transitions order to ASSIGNED");
        order.transitionTo(OrderStatus::PICKED_UP);
        order.transitionTo(OrderStatus::OUT_FOR_DELIVERY);
        order.transitionTo(OrderStatus::DELIVERED);
        CHECK(order.getStatus() == OrderStatus::DELIVERED, "Full valid transition chain reaches DELIVERED");

        bool threwAfterDelivered = false;
        try {
            order.transitionTo(OrderStatus::CANCELLED); // no transitions allowed out of DELIVERED
        } catch (const InvalidOrderTransition&) {
            threwAfterDelivered = true;
        }
        CHECK(threwAfterDelivered, "DELIVERED is a terminal state - no further transitions allowed");
    });

    // =========================================================================
    // 4. AGENT CAPACITY - cannot exceed maxCapacity, manual assign rejects overflow
    // =========================================================================
    testfw::runSuite("Agent Capacity", [] {
        AgentManager agentMgr;
        OrderManager orderMgr;
        RestaurantManager restaurantMgr;
        restaurantMgr.addRestaurant(makeTestRestaurant());

        agentMgr.registerAgent(makeTestAgent("AGT-FULL", "Deepak", 25.4530, 78.5690, "Zone-A", 4.7, /*activeOrders=*/2, /*maxCapacity=*/2));
        DeliveryAgent* fullAgent = agentMgr.getAgent("AGT-FULL");
        CHECK(!fullAgent->isAvailableForAssignment(), "Agent at max capacity is not available for assignment");
        CHECK(fullAgent->getStatus() == AgentStatus::BUSY, "incrementActiveOrders() flips status to BUSY once full");

        DispatchEngine engine(agentMgr, orderMgr, restaurantMgr);
        orderMgr.createOrder(makeReadyOrder("ORD-CAP-1"));

        bool threw = false;
        try {
            engine.manualAssign("ORD-CAP-1", "AGT-FULL");
        } catch (const AgentCapacityExceededException&) {
            threw = true;
        }
        CHECK(threw, "manualAssign() rejects assignment to an agent already at max capacity");
    });

    // =========================================================================
    // 5. DISPATCH SCORING - formula components combine as documented
    // =========================================================================
    testfw::runSuite("Dispatch Scoring", [] {
        AgentManager agentMgr;
        OrderManager orderMgr;
        RestaurantManager restaurantMgr;
        restaurantMgr.addRestaurant(makeTestRestaurant());

        // Agent at the exact same location as the restaurant -> distanceScore should be ~100
        agentMgr.registerAgent(makeTestAgent("AGT-SAME", "SameSpot", 25.4520, 78.5680, "Zone-A", 5.0, 0));
        DispatchEngine engine(agentMgr, orderMgr, restaurantMgr);
        Order order = makeReadyOrder("ORD-SCORE-1");

        CandidateScore c = engine.score(order, *agentMgr.getAgent("AGT-SAME"));
        CHECK(c.distanceScore > 99.0, "Agent co-located with restaurant gets near-maximum distanceScore");
        CHECK(APPROX_EQ(c.ratingScore, 100.0, 0.01), "5.0 rating normalises to ratingScore of 100");
        CHECK(c.workloadScore > 99.0, "Idle agent (0 active orders) gets near-maximum workloadScore");
        CHECK(c.finalScore > 90.0, "Ideal candidate (close, idle, top-rated) produces a high final score");
    });

    // =========================================================================
    // 6. CANCELLATION - eligible orders can cancel, delivered/cancelled cannot
    // =========================================================================
    testfw::runSuite("Cancellation", [] {
        Order placedOrder("ORD-CANCEL-1", "CUS-0001", "RST-0001",
                           {{"Item", 1, 50.0}}, Location(), OrderPriority::NORMAL, Payment(50.0));
        CHECK(placedOrder.isCancellable(), "A freshly PLACED order is cancellable");
        placedOrder.transitionTo(OrderStatus::CANCELLED);
        CHECK(placedOrder.getStatus() == OrderStatus::CANCELLED, "Order transitions to CANCELLED successfully");

        Order deliveredOrder = makeReadyOrder("ORD-CANCEL-2");
        deliveredOrder.assignAgent("AGT-0001");
        deliveredOrder.transitionTo(OrderStatus::PICKED_UP);
        deliveredOrder.transitionTo(OrderStatus::OUT_FOR_DELIVERY);
        deliveredOrder.transitionTo(OrderStatus::DELIVERED);
        CHECK(!deliveredOrder.isCancellable(), "A DELIVERED order is no longer cancellable");
    });

    // =========================================================================
    // 7. NO AVAILABLE AGENT CASE
    // =========================================================================
    testfw::runSuite("No Available Agent", [] {
        AgentManager agentMgr; // zero agents registered
        OrderManager orderMgr;
        RestaurantManager restaurantMgr;
        restaurantMgr.addRestaurant(makeTestRestaurant());

        DispatchEngine engine(agentMgr, orderMgr, restaurantMgr);
        Order order = makeReadyOrder("ORD-NOAGENT-1");

        auto candidates = engine.evaluateCandidates(order);
        CHECK(candidates.empty(), "No agents registered -> zero candidates evaluated");

        orderMgr.createOrder(order);
        DispatchResult result = engine.dispatchOrder("ORD-NOAGENT-1");
        CHECK(!result.success, "dispatchOrder() reports failure when no agent is available");
        CHECK(result.message == "No suitable delivery agent available",
              "Failure message matches the documented API error message");
    });

    return testfw::summary();
}
