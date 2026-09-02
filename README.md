# Smart Delivery Dispatch System

A placement-portfolio project that pairs a **C++17 backend** — real OOP, STL
data structures, a weighted dispatch algorithm, and a REST API — with a
**vanilla HTML/CSS/JS** operations dashboard styled like a real logistics
product, not a CRUD demo.

The point of this project is the **Smart Dispatch Engine**: when an order is
ready, the C++ backend scores every eligible delivery agent on distance,
current workload, rating, order priority, and zone match, and assigns the
best one — transparently, with the full score breakdown visible in the UI.

---

## Table of Contents

1. [Problem Statement](#problem-statement)
2. [Features](#features)
3. [Architecture](#architecture)
4. [Tech Stack](#tech-stack)
5. [OOP Concepts Used](#oop-concepts-used)
6. [Data Structures Used](#data-structures-used)
7. [The Dispatch Algorithm](#the-dispatch-algorithm)
8. [Order State Machine](#order-state-machine)
9. [API Documentation](#api-documentation)
10. [Database / Persistence Design](#database--persistence-design)
11. [Project Structure](#project-structure)
12. [How to Build](#how-to-build)
13. [How to Run](#how-to-run)
14. [Demo Credentials](#demo-credentials)
15. [Testing](#testing)
16. [Why C++?](#why-c)
17. [Future Improvements](#future-improvements)
18. [Interview Discussion Points](#interview-discussion-points)
19. [Screenshots](#screenshots)

---

## Problem Statement

A food-delivery platform has orders coming in continuously and a pool of
delivery agents with different vehicles, locations, ratings and current
workloads. Naively assigning "whoever is closest" ignores that the closest
agent might already be overloaded, poorly rated, or headed elsewhere — and
naively assigning "whoever is idle" ignores distance and quality. The system
needs a **single, explainable, tunable algorithm** that balances all of
these factors, respects agent capacity limits, prevents order starvation
under load, and can be inspected/demonstrated (not a black box).

## Features

- **Three role-based dashboards** (Admin, Customer, Delivery Agent) served
  from one responsive frontend.
- **Smart Dispatch Engine** with a documented, weighted scoring formula and
  a live "Dispatch Center" that visualises every candidate's score breakdown.
- **Order state machine** enforced server-side — invalid transitions are
  rejected with a clear error, not silently ignored.
- **Anti-starvation priority aging** — orders waiting too long automatically
  gain effective priority so they aren't starved by a constant stream of
  URGENT orders.
- **Manual assign / reassign** for admins, with the same transparent score
  breakdown used for auto-dispatch.
- **Live analytics** computed from real backend state (no fake/random
  numbers): status distribution, priority distribution, orders per hour,
  agent utilization, cancellation rate, average delivery time.
- **Fully responsive** down to 375px mobile, with tables that collapse into
  cards, an off-canvas sidebar, and touch-friendly controls.
- **Realistic seed data**: 10 customers, 10 delivery agents, 5 restaurants,
  30 orders across every status/priority/zone combination.

## Architecture

```
┌─────────────────────────────┐        HTTP / JSON        ┌──────────────────────────────┐
│   Frontend (vanilla JS)     │  ────────────────────────▶ │   REST API (cpp-httplib)     │
│  admin / customer / agent / │  ◀────────────────────────  │        main.cpp              │
│  dispatch / analytics .html │                             └───────────────┬───────────────┘
└─────────────────────────────┘                                             │
                                                                             ▼
                                                            ┌───────────────────────────────┐
                                                            │   Dispatcher (orchestration)   │
                                                            │  - records Delivery history     │
                                                            │  - pushes notifications         │
                                                            └───────────────┬────────────────┘
                                                                             ▼
                                                            ┌───────────────────────────────┐
                                                            │      DispatchEngine            │
                                                            │  (pure scoring algorithm,       │
                                                            │   unit-testable in isolation)   │
                                                            └───────────────┬────────────────┘
                                                                             ▼
                                          ┌───────────────────────────────────────────────────────┐
                                          │   Managers: OrderManager · AgentManager ·               │
                                          │   CustomerManager · RestaurantManager ·                 │
                                          │   AnalyticsManager                                       │
                                          └───────────────────────────────┬───────────────────────┘
                                                                          ▼
                                          ┌───────────────────────────────────────────────────────┐
                                          │   Models: User→{Admin,Customer,DeliveryAgent} ·          │
                                          │   Vehicle→{Bike,Car,Van} · Order · Delivery ·            │
                                          │   Restaurant · Location · Payment                        │
                                          └───────────────────────────────────────────────────────┘
```

The **API layer (`main.cpp`) is intentionally thin**: it parses JSON,
calls into the managers/`Dispatcher`, and serialises the result. All real
business logic — scoring, state transitions, capacity checks — lives in the
model and service classes under `backend/include/`, which is why the
dispatch algorithm can be unit-tested completely independently of HTTP (see
`tests/test_dispatch.cpp`).

## Tech Stack

| Layer         | Choice                                                            |
|---------------|--------------------------------------------------------------------|
| Backend       | C++17, STL, [cpp-httplib](https://github.com/yhirose/cpp-httplib) (header-only HTTP), [nlohmann/json](https://github.com/nlohmann/json) (header-only JSON) |
| Build         | CMake ≥ 3.15                                                       |
| Persistence   | Structured JSON files (`data/*.json`), designed to be swapped for SQLite — see [Database / Persistence Design](#database--persistence-design) |
| Frontend      | Vanilla HTML5 / CSS3 / JavaScript (Fetch API), no framework        |
| Testing       | A small dependency-free test runner (`tests/test_framework.h`)     |

Both third-party C++ headers are vendored under
`backend/include/third_party/` so the project builds with nothing but a
C++17 compiler and CMake — no package manager required.

## OOP Concepts Used

| Concept              | Where |
|----------------------|-------|
| **Encapsulation**    | Every model (`Order`, `DeliveryAgent`, `Customer`, ...) keeps data members `private`/`protected` and exposes intent-revealing methods (`transitionTo()`, `incrementActiveOrders()`) instead of raw setters. |
| **Abstraction**      | `User::role()` and `Vehicle::averageSpeedKmph()` are pure virtual — callers work through the base interface without knowing the concrete subtype. |
| **Inheritance**      | `User → Customer, Admin, DeliveryAgent` and `Vehicle → Bike, Car, Van`. Used only where a genuine is-a relationship + shared interface exists — **not** applied to `Order`, `Restaurant`, or `Payment`, which are composition, not inheritance, by design. |
| **Polymorphism**     | `DispatchEngine` computes `vehicle.averageSpeedKmph()` and `capacityFactor()` through the base `Vehicle&` reference; the actual behaviour dispatches to `Bike`/`Car`/`Van` at runtime. |
| **Composition**      | `DeliveryAgent` *has a* `std::unique_ptr<Vehicle>` (exclusive ownership, not inheritance) — a Bike doesn't "become" a DeliveryAgent. |
| **Constructors/Destructors** | All resource-owning classes rely on RAII; no manual `delete` anywhere. `Vehicle` has a `virtual ~Vehicle()` for safe polymorphic destruction through `unique_ptr<Vehicle>`. |
| **const-correctness**| `DispatchEngine::score()` and `evaluateCandidates()` are `const`; accessors return `const&` where copies aren't needed. |
| **References**       | Managers are passed by reference (`AgentManager&`, not pointers or copies) into `DispatchEngine`/`Dispatcher` to make ownership and non-null-ness explicit. |
| **Smart pointers**   | `std::unique_ptr<Vehicle>` (agent owns its vehicle exclusively), `std::unique_ptr<DeliveryAgent>` (AgentManager owns agents). No raw `new`/`delete` in the codebase. |
| **Exceptions**       | Custom exception hierarchy (`OrderNotFoundException`, `AgentCapacityExceededException`, `InvalidOrderTransition`, ...) mapped to correct HTTP status codes at the API boundary. |
| **Separation of responsibilities** | Models hold state + invariants → Managers hold collections + queries → `DispatchEngine` holds the algorithm → `Dispatcher` holds orchestration/side-effects → `main.cpp` holds only HTTP glue. |

## Data Structures Used

| Structure                     | Where used | Why |
|--------------------------------|------------|-----|
| `std::vector`                  | Lists of restaurants, agents, order items, candidate scores | Ordered, cache-friendly iteration |
| `std::unordered_map`           | Order lookup by ID (`OrderManager`), agent lookup by ID (`AgentManager`), customer/restaurant lookup | O(1) average lookup by ID, which every REST endpoint needs |
| `std::queue`                   | `OrderManager::incomingOrderQueue` — FIFO pipeline of freshly `PLACED` orders | Orders should be confirmed/prepared in the order they arrived |
| `std::priority_queue`          | `OrderManager::dispatchQueue` — orders that are `READY` and waiting for an agent | Higher-priority (and aged) orders should be dispatched first; a heap gives O(log n) push/pop for the highest-priority order |
| `std::set`                     | `OrderManager::deliveryZones` — unique zone labels seen so far | Uniqueness with a defined order, useful for zone-based filtering |

## The Dispatch Algorithm

Implemented in `backend/include/services/DispatchEngine.h` /
`backend/src/services/DispatchEngine.cpp`. All weights and constants live in
`backend/include/utils/Config.h` — nothing is hardcoded inline in the
scoring function, so the formula can be tuned without touching algorithm
code.

```
distanceScore  = 100 × (1 − min(distanceKm, 8.0) / 8.0)     // closer = higher, capped at 8 km
workloadScore  = 100 × (1 − activeOrders / maxCapacity)      // idler = higher
ratingScore    = 100 × (rating / 5.0)                        // normalised 0–5 rating

weighted       = 0.40 × distanceScore
               + 0.30 × workloadScore
               + 0.30 × ratingScore

priorityAdjustment = basePriority(order.priority) + agingBonus(waitTime)
    // NORMAL +0, HIGH +10, URGENT +20, PERISHABLE +25
    // agingBonus: +5 for every 45s an order waits in READY, capped at +30
    //   (anti-starvation: a NORMAL order that waits long enough eventually
    //    outranks a fresh URGENT order)

zoneBonus      = +5 if the agent's current zone == the restaurant's zone, else 0

finalScore     = weighted + priorityAdjustment + zoneBonus
```

The engine only considers agents that are `AVAILABLE` **and** below
`maxCapacity` (`DeliveryAgent::isAvailableForAssignment()`), scores every
eligible candidate, sorts them, and assigns the highest-scoring one. The
**full ranked candidate list with every score component** is returned to the
API — this is what powers the "Candidate Score Breakdown" panel in the
Dispatch Center, so the algorithm's decision is never a black box.

### Worked example

```
Order ORD-1024 · Burger Point · Zone-B · Priority: HIGH

Rahul   — 1.2 km, 0 active orders, ★4.7
Aman    — 0.8 km, 2 active orders, ★4.9
Karan   — 2.0 km, 0 active orders, ★4.5

                distance   workload   rating   priority   zone   final
Rahul             90.0       100.0      94.0      +10      +0    286.0  ← selected
Aman               95.0        33.3      98.0      +10      +0    229.0
Karan              75.0       100.0      90.0      +10      +0    272.5
```

Aman is closest but is carrying 2 of a 3-order capacity, so his
`workloadScore` collapses — Rahul wins on the combined weighted score, not
raw distance. This is the behaviour the spec explicitly asked for: **"Do not
simply select the nearest driver."**

## Order State Machine

```
PLACED → CONFIRMED → PREPARING → READY → ASSIGNED → PICKED_UP → OUT_FOR_DELIVERY → DELIVERED
   │          │            │         │
   └──────────┴────────────┴─────────┴──────────────────────────▶ CANCELLED
```

Enforced entirely inside `Order::transitionTo()` (see `Order.h`) against a
static table of allowed `(from, to)` pairs. Any other transition throws
`InvalidOrderTransition`, which the API layer maps to `HTTP 409 Conflict`.
`DELIVERED` and `CANCELLED` are terminal — no further transitions are
allowed out of either.

## API Documentation

All responses are JSON with a `success: boolean` field. Errors follow:

```json
{ "success": false, "message": "No suitable delivery agent available" }
```

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/dashboard` | Top-line stats (total orders, active deliveries, available agents, delivered today, avg delivery time, cancellation rate) |
| GET | `/api/analytics` | Full analytics payload (status/priority distribution, orders per hour, agent utilization) |
| GET | `/api/notifications?limit=20` | Recent system notifications |
| GET | `/api/orders` | All orders. Filters: `?status=`, `?customerId=`, `?agentId=` |
| GET | `/api/orders/:id` | Single order |
| POST | `/api/orders` | Create an order — `{ customerId, restaurantId, items[], priority?, paymentMethod? }` |
| PUT | `/api/orders/:id/status` | Transition order status — `{ status }` |
| POST | `/api/orders/:id/cancel` | Cancel an eligible order |
| POST | `/api/orders/:id/dispatch` | Run the DispatchEngine for one `READY` order |
| POST | `/api/orders/:id/reassign` | Admin manual assign/reassign — `{ agentId }` |
| GET | `/api/dispatch/:orderId` | Full candidate score breakdown for an order, without assigning |
| GET | `/api/dispatch-queue` | Peek all orders currently waiting for dispatch |
| POST | `/api/dispatch/auto` | Auto-dispatch every order currently waiting |
| GET | `/api/agents` | All agents. Filter: `?status=` |
| GET | `/api/agents/:id` | Single agent |
| POST | `/api/agents` | Register a new agent |
| PUT | `/api/agents/:id/availability` | `{ status: "AVAILABLE" \| "OFFLINE" }` |
| GET | `/api/restaurants` | All restaurants |
| POST | `/api/restaurants` | Add a restaurant |
| GET | `/api/customers` | All customers |
| GET | `/api/customers/:id` | Single customer |

## Database / Persistence Design

The project uses **structured JSON file persistence** (`PersistenceService`,
writing to `data/*.json`) rather than SQLite, per the brief's guidance to
prefer the simpler option unless SQLite is genuinely needed. The code is
deliberately organised so this is a clean swap later:

- All persistence goes through a single `PersistenceService` class with one
  method (`saveSnapshot`) that the rest of the codebase depends on — nothing
  else touches the filesystem directly.
- Every model already implements `toJson()`, so a SQLite-backed
  `PersistenceService` would only need to change *how* that JSON gets
  written (`INSERT`/`UPDATE` statements) — not the models, managers, or API
  layer.
- On startup the system currently reseeds realistic demo data and writes a
  fresh snapshot; reloading from a previous snapshot on boot is listed under
  [Future Improvements](#future-improvements).

## Project Structure

```
smart-delivery-dispatch/
├── backend/
│   ├── include/
│   │   ├── models/       User, Customer, Admin, DeliveryAgent, Vehicle,
│   │   │                 Order, Delivery, Restaurant, Location, Payment
│   │   ├── managers/      OrderManager, AgentManager, CustomerManager,
│   │   │                 RestaurantManager, AnalyticsManager
│   │   ├── services/      DispatchEngine, Dispatcher, NotificationService,
│   │   │                 PersistenceService
│   │   ├── utils/         Config, IdGenerator, Exceptions
│   │   └── third_party/   httplib.h, json.hpp (vendored, header-only)
│   ├── src/
│   │   ├── services/DispatchEngine.cpp
│   │   └── main.cpp       REST API — thin HTTP layer only
│   ├── database/SeedData.h
│   └── CMakeLists.txt
├── frontend/
│   ├── index.html, admin.html, customer.html, agent.html,
│   │   dispatch.html, analytics.html
│   ├── css/  style.css (design system), components.css, responsive.css
│   └── js/   api.js, utils.js, shell.js, dashboard.js, dispatch.js,
│              customer.js, agent.js, analytics.js
├── data/                   Generated JSON persistence snapshots
├── tests/                  test_dispatch.cpp, test_framework.h
└── README.md
```

## How to Build

Requirements: a C++17 compiler (g++ ≥ 9 or clang ≥ 10) and CMake ≥ 3.15.

```bash
cd backend
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

This produces two binaries inside `backend/build/`:
`dispatch_server` (the REST API) and `tests_build/dispatch_tests` (the test
suite).

## How to Run

```bash
cd backend/build
./dispatch_server
```

The server listens on **`http://localhost:8080`** and also serves the
frontend as static files from the same origin (see `set_mount_point` in
`main.cpp`), so you don't need a separate web server:

```
open http://localhost:8080/index.html
```

On every startup the system reseeds realistic demo data (10 customers, 10
agents, 5 restaurants, 30 orders) so the dashboard is populated immediately.

## Demo Credentials

This is a portfolio/demo application — **no real authentication is
implemented**, by design (see project brief). Sign in with any password
using:

| Role | Email |
|------|-------|
| Admin | `admin@demo.com` |
| Customer | `customer@demo.com` |
| Delivery Agent | `agent@demo.com` |

The login screen has one-click "use demo credentials" buttons for each.

## Testing

```bash
cd backend/build
./tests_build/dispatch_tests
```

Covers the required matrix end-to-end against the real classes (no mocks):

1. **Agent selection** — a close, idle, well-rated agent beats a closer but
   overloaded one.
2. **Priority handling** — URGENT scores higher than NORMAL for an
   identical agent, by exactly the configured bonus.
3. **Order state transitions** — valid chains succeed; skipping states or
   transitioning out of a terminal state throws.
4. **Agent capacity** — an agent at `maxCapacity` is excluded from
   candidates and manual assignment is rejected.
5. **Dispatch scoring** — verifies each score component in isolation
   (co-located agent ⇒ near-100 distance score, idle agent ⇒ near-100
   workload score, etc.).
6. **Cancellation** — eligible orders cancel; delivered orders cannot.
7. **No available agent** — `dispatchOrder()` returns the documented
   failure message rather than throwing or crashing.

All 22 checks currently pass.

## Why C++?

The brief for this project is deliberately backwards from a typical
food-delivery clone: **the C++ backend is the product**, and the web
frontend is presentation. C++ owns:

- The dispatch **algorithm** — a numeric optimisation problem (weighted
  scoring across multiple candidates) is exactly what C++ is good at, and
  doing it server-side means the algorithm can't be tampered with or
  bypassed from the client.
- **Business rules** — the order state machine, capacity limits, and
  priority aging are invariants that must hold regardless of which UI is
  talking to the server; enforcing them in strongly-typed C++ classes
  (rather than loosely in JavaScript) makes them impossible to accidentally
  violate.
- **Data structures** — `priority_queue` for dispatch ordering and
  `unordered_map` for O(1) lookups are textbook STL usage applied to a real
  problem, not decoration.

The frontend's job is only to *display* what the backend decides and to
*collect* input for the backend to act on — every meaningful decision in
this system happens in C++.

## Future Improvements

- Swap `PersistenceService`'s JSON writer for a real SQLite backend (the
  `toJson()`/manager boundary is already designed for this — see
  [Database / Persistence Design](#database--persistence-design)).
- Reload state from a previous snapshot on startup instead of always
  reseeding.
- Real authentication (hashed passwords, sessions/JWT) in place of the demo
  role picker.
- WebSocket/SSE push instead of the frontend's periodic polling, for
  instant dashboard updates.
- Route-aware ETA (actual road distance/time via a routing API) instead of
  haversine distance ÷ average speed.
- Configurable dispatch weights exposed in the Admin UI instead of only in
  `Config.h`.

## Interview Discussion Points

**OOP design**
- *Why does `DeliveryAgent` compose a `Vehicle` instead of inheriting from
  it?* Because an agent doesn't "become" a bike — it *has* one, and can
  swap it. Composition also avoids exploding the `User` hierarchy with
  `BikeAgent`/`CarAgent` subclasses for a property that changes at runtime.
- *Where is inheritance deliberately avoided?* `Order`, `Restaurant`,
  `Payment`, `Delivery` are plain value/entity classes with no inheritance —
  there's no is-a relationship to model, so adding one would be inheritance
  for its own sake.

**Inheritance & polymorphism**
- *How does `DispatchEngine` use a `Vehicle` without knowing if it's a Bike,
  Car, or Van?* It calls `agent.getVehicle().averageSpeedKmph()` through the
  base class reference; the virtual dispatch resolves to the correct
  override at runtime when computing ETA.

**STL**
- *Why `unordered_map` for order/agent lookup instead of `map`?* Lookups by
  ID (`GET /api/orders/:id`) don't need sorted iteration, so the O(1)
  average complexity of `unordered_map` beats `map`'s O(log n) with no
  downside here.
- *Why `priority_queue` instead of sorting a vector every time?* Orders
  arrive and get dispatched continuously; a binary heap gives O(log n)
  insert and extract-max, versus O(n log n) to re-sort a vector on every
  change.

**Dispatch algorithm**
- *What happens if two agents tie exactly on final score?* `std::sort` is
  not guaranteed stable, but ties are effectively broken by iteration order
  from `AgentManager::getAllAgents()`, which follows registration order — a
  documented, deterministic tiebreak rather than undefined behaviour.
- *How is starvation prevented?* `OrderManager::applyAgingAndRebuild()`
  recomputes each waiting order's effective priority based on elapsed wait
  time before every dispatch attempt, so a NORMAL order eventually
  outranks a fresh URGENT order rather than waiting forever.

**API architecture**
- *Why is `main.cpp` so "thin"?* So the dispatch algorithm and business
  rules can be unit tested (see `tests/test_dispatch.cpp`) without spinning
  up an HTTP server, and so the same core library could be reused behind a
  different transport (e.g. gRPC) without touching business logic.

**Database design**
- *Why JSON files instead of SQLite for a "real" project?* The brief
  explicitly prefers the simpler option unless SQLite is genuinely needed;
  with a single-process, single-writer demo workload, JSON snapshots are
  sufficient, and the persistence boundary (`PersistenceService`) is narrow
  enough to swap out without touching model or business logic.

**Complexity**
- `DispatchEngine::evaluateCandidates()` is O(A log A) for A agents
  (linear scoring pass + sort); `OrderManager` lookups are O(1) average;
  dispatch-queue operations are O(log N) for N queued orders.

**Error handling**
- *What happens when no agent is available?* `DispatchEngine::dispatchOrder`
  does **not** throw for this case — it's an expected business outcome, not
  an exceptional one — and returns a `DispatchResult{success:false, message:
  "No suitable delivery agent available"}`, which the API maps to `HTTP 409`.
  Contrast this with `OrderNotFoundException` (a genuine caller error →
  `HTTP 404`) and `AgentCapacityExceededException` (a constraint violation →
  `HTTP 409`) — the exception hierarchy separates "the request itself was
  invalid" from "the request was valid but the business outcome was
  negative."

## Screenshots

*(Add screenshots of the Admin Overview, Dispatch Center, Customer tracking
view, and Agent workflow here before submitting/presenting.)*
