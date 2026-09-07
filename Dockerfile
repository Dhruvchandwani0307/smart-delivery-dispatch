# ============================================================================
# Dockerfile - Smart Delivery Dispatch backend
# Builds the C++17 REST API and runs it standalone. Works as-is on Render,
# Railway, Fly.io, or any host that can build+run a Dockerfile.
#
# IMPORTANT: both stages are pinned to the exact same Debian release
# (bookworm). Using a floating tag like `gcc:13` for the build stage against
# a differently-pinned runtime base is a classic multi-stage Docker bug: if
# gcc:13 ever moves to a newer Debian release than the runtime image, the
# compiled binary links against a newer glibc/libstdc++ than the runtime
# image ships, and the container builds successfully but the binary fails
# to even start (exits immediately with no useful error).
# ============================================================================
FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ cmake make ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY backend/ backend/
COPY tests/ tests/

RUN cd backend && mkdir -p build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j"$(nproc)" dispatch_server

# ---- Runtime image (same Debian release as the build stage, no compiler) ----
FROM debian:bookworm-slim

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
