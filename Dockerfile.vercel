# ============================================================================
# Dockerfile - Smart Delivery Dispatch backend
# Builds the C++17 REST API and runs it standalone. Works as-is on Render,
# Railway, Fly.io, or any host that can build+run a Dockerfile.
# ============================================================================
FROM gcc:13 AS build

RUN apt-get update && apt-get install -y cmake && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY backend/ backend/
COPY tests/ tests/

RUN cd backend && mkdir -p build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j"$(nproc)" dispatch_server

# ---- Runtime image (slim, no compiler) ----
FROM gcc:13

WORKDIR /app
COPY --from=build /app/backend/build/dispatch_server ./dispatch_server
COPY frontend/ ./frontend/
RUN mkdir -p data

# main.cpp mounts the frontend at "../../frontend" and writes snapshots to
# "../../data" relative to the binary's working directory, so replicate the
# backend/build/ nesting the source code expects.
RUN mkdir -p backend/build && mv dispatch_server backend/build/dispatch_server

ENV PORT=8080
EXPOSE 8080

WORKDIR /app/backend/build
CMD ["./dispatch_server"]
